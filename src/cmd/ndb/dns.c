#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <bio.h>
#include <ctype.h>
#include <ip.h>
#include <ndb.h>
#include <thread.h>
#include "dns.h"

enum
{
	Maxrequest=		1024,
	Ncache=			8,
	Maxpath=		128,
	Maxreply=		512,
	Maxrrr=			16,
	Maxfdata=		8192,

	Qdir=			0,
	Qdns=			1
};

typedef struct Mfile	Mfile;
typedef struct Job	Job;
typedef struct Network	Network;

int vers;		/* incremented each clone/attach */

struct Mfile
{
	Mfile		*next;		/* next free mfile */
	int		ref;

	char		*user;
	Qid		qid;
	int		fid;

	int		type;		/* reply type */
	char		reply[Maxreply];
	ushort		rr[Maxrrr];	/* offset of rr's */
	ushort		nrr;		/* number of rr's */
};

/*
 * active local requests
 */
struct Job
{
	Job	*next;
	int	flushed;
	Fcall	request;
	Fcall	reply;
};
Lock	joblock;
Job	*joblist;

struct {
	Lock	lk;
	Mfile	*inuse;		/* active mfile's */
} mfalloc;

int	mfd[2];
int	debug;
int traceactivity;
int	cachedb;
ulong	now;
int	testing;
char	*trace;
int	needrefresh;
int	resolver;
uchar	ipaddr[IPaddrlen];	/* my ip address */
int	maxage;
char	*zonerefreshprogram;
int	sendnotifies;

void	rversion(Job*);
void	rauth(Job*);
void	rflush(Job*);
void	rattach(Job*, Mfile*);
char*	rwalk(Job*, Mfile*);
void	ropen(Job*, Mfile*);
void	rcreate(Job*, Mfile*);
void	rread(Job*, Mfile*);
void	rwrite(Job*, Mfile*, Request*);
void	rclunk(Job*, Mfile*);
void	rremove(Job*, Mfile*);
void	rstat(Job*, Mfile*);
void	rwstat(Job*, Mfile*);
void	sendmsg(Job*, char*);
void	mountinit(char*);
void	io(void);
int	fillreply(Mfile*, int);
Job*	newjob(void);
void	freejob(Job*);
void	setext(char*, int, char*);

char *tcpaddr = "tcp!*!domain";
char *udpaddr = "udp!*!domain";
char	*logfile = "dns";
char	*dbfile;
char	mntpt[Maxpath];
char	*LOG;

void
usage(void)
{
	fprint(2, "usage: dns [-dnrst] [-a maxage] [-f ndb-file] [-T tcpaddr] [-U udpaddr] [-x service] [-z zoneprog]\n");
	threadexitsall("usage");
}

void
checkaddress(void)
{
	char *u, *t;

	u = strchr(udpaddr, '!');
	t = strchr(tcpaddr, '!');
	if(u && t && strcmp(u, t) != 0)
		fprint(2, "warning: announce mismatch %s %s\n", udpaddr, tcpaddr);
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	int serveudp, servetcp;
	char *service;

	serveudp = 0;
	servetcp = 0;
	service = "dns";
	ARGBEGIN{
	case 'd':
		debug = 1;
		traceactivity = 1;
		break;
	case 'f':
		dbfile = EARGF(usage());
		break;
	case 'x':
		service = EARGF(usage());
		break;
	case 'r':
		resolver = 1;
		break;
	case 's':
		serveudp = 1;
		cachedb = 1;
		break;
	case 't':
		servetcp = 1;
		cachedb = 1;
		break;
	case 'a':
		maxage = atoi(EARGF(usage()));
		break;
	case 'z':
		zonerefreshprogram = EARGF(usage());
		break;
	case 'n':
		sendnotifies = 1;
		break;
	case 'U':
		udpaddr = estrdup(netmkaddr(EARGF(usage()), "udp", "domain"));
		break;
	case 'T':
		tcpaddr = estrdup(netmkaddr(EARGF(usage()), "tcp", "domain"));
		break;
	default:
		usage();
	}ARGEND

	if(argc)
		usage();
	if(serveudp && servetcp)
		checkaddress();

	rfork(RFNOTEG);

	/* start syslog before we fork */
	fmtinstall('F', fcallfmt);
	dninit();
	if(myipaddr(ipaddr, mntpt) < 0)
		sysfatal("can't read my ip address");

	syslog(0, logfile, "starting dns on %I", ipaddr);

	opendatabase();

	mountinit(service);

	now = time(0);
	srand(now*getpid());
	db2cache(1);

	if(serveudp)
		proccreate(dnudpserver, nil, STACK);
	if(servetcp)
		proccreate(dntcpserver, nil, STACK);
	if(sendnotifies)
		proccreate(notifyproc, nil, STACK);

	io();
}

/*
 *  if a mount point is specified, set the cs extention to be the mount point
 *  with '_'s replacing '/'s
 */
void
setext(char *ext, int n, char *p)
{
	int i, c;

	n--;
	for(i = 0; i < n; i++){
		c = p[i];
		if(c == 0)
			break;
		if(c == '/')
			c = '_';
		ext[i] = c;
	}
	ext[i] = 0;
}

void
mountinit(char *service)
{
	int p[2];

	if(pipe(p) < 0)
		abort(); /* "pipe failed" */;
	if(post9pservice(p[1], service, nil) < 0)
		fprint(2, "post9pservice dns: %r\n");
	close(p[1]);
	mfd[0] = mfd[1] = p[0];
}

Mfile*
newfid(int fid, int needunused)
{
	Mfile *mf;

	lock(&mfalloc.lk);
	for(mf = mfalloc.inuse; mf != nil; mf = mf->next){
		if(mf->fid == fid){
			unlock(&mfalloc.lk);
			if(needunused)
				return nil;
			return mf;
		}
	}
	if(!needunused){
		unlock(&mfalloc.lk);
		return nil;
	}
	mf = emalloc(sizeof(*mf));
	if(mf == nil)
		sysfatal("out of memory");
	mf->fid = fid;
	mf->next = mfalloc.inuse;
	mfalloc.inuse = mf;
	unlock(&mfalloc.lk);
	return mf;
}

void
freefid(Mfile *mf)
{
	Mfile **l;

	lock(&mfalloc.lk);
	for(l = &mfalloc.inuse; *l != nil; l = &(*l)->next){
		if(*l == mf){
			*l = mf->next;
			if(mf->user)
				free(mf->user);
			free(mf);
			unlock(&mfalloc.lk);
			return;
		}
	}
	sysfatal("freeing unused fid");
}

Mfile*
copyfid(Mfile *mf, int fid)
{
	Mfile *nmf;

	nmf = newfid(fid, 1);
	if(nmf == nil)
		return nil;
	nmf->fid = fid;
	nmf->user = estrdup(mf->user);
	nmf->qid.type = mf->qid.type;
	nmf->qid.path = mf->qid.path;
	nmf->qid.vers = vers++;
	return nmf;
}

Job*
newjob(void)
{
	Job *job;

	job = emalloc(sizeof(*job));
	lock(&joblock);
	job->next = joblist;
	joblist = job;
	job->request.tag = -1;
	unlock(&joblock);
	return job;
}

void
freejob(Job *job)
{
	Job **l;

	lock(&joblock);
	for(l = &joblist; *l; l = &(*l)->next){
		if((*l) == job){
			*l = job->next;
			free(job);
			break;
		}
	}
	unlock(&joblock);
}

void
flushjob(int tag)
{
	Job *job;

	lock(&joblock);
	for(job = joblist; job; job = job->next){
		if(job->request.tag == tag && job->request.type != Tflush){
			job->flushed = 1;
			break;
		}
	}
	unlock(&joblock);
}

void
ioproc0(void *v)
{
	long n;
	Mfile *mf;
	uchar mdata[IOHDRSZ + Maxfdata];
	Request req;
	Job *job;

	USED(v);

	for(;;){
		n = read9pmsg(mfd[0], mdata, sizeof mdata);
		if(n <= 0){
			syslog(0, logfile, "error reading mntpt: %r");
			break;
		}
		job = newjob();
		if(convM2S(mdata, n, &job->request) != n){
			freejob(job);
			continue;
		}
		if(debug)
			syslog(0, logfile, "%F", &job->request);

		getactivity(&req);
		req.aborttime = now + 60;	/* don't spend more than 60 seconds */

		mf = nil;
		switch(job->request.type){
		case Tversion:
		case Tauth:
		case Tflush:
			break;
		case Tattach:
			mf = newfid(job->request.fid, 1);
			if(mf == nil){
				sendmsg(job, "fid in use");
				goto skip;
			}
			break;
		default:
			mf = newfid(job->request.fid, 0);
			if(mf == nil){
				sendmsg(job, "unknown fid");
				goto skip;
			}
			break;
		}

		switch(job->request.type){
		default:
			syslog(1, logfile, "unknown request type %d", job->request.type);
			break;
		case Tversion:
			rversion(job);
			break;
		case Tauth:
			rauth(job);
			break;
		case Tflush:
			rflush(job);
			break;
		case Tattach:
			rattach(job, mf);
			break;
		case Twalk:
			rwalk(job, mf);
			break;
		case Topen:
			ropen(job, mf);
			break;
		case Tcreate:
			rcreate(job, mf);
			break;
		case Tread:
			rread(job, mf);
			break;
		case Twrite:
			rwrite(job, mf, &req);
			break;
		case Tclunk:
			rclunk(job, mf);
			break;
		case Tremove:
			rremove(job, mf);
			break;
		case Tstat:
			rstat(job, mf);
			break;
		case Twstat:
			rwstat(job, mf);
			break;
		}
skip:
		freejob(job);
		putactivity();
	}
}

void
io(void)
{
	int i;

	for(i=0; i<Maxactive; i++)
		proccreate(ioproc0, 0, STACK);
}

void
rversion(Job *job)
{
	if(job->request.msize > IOHDRSZ + Maxfdata)
		job->reply.msize = IOHDRSZ + Maxfdata;
	else
		job->reply.msize = job->request.msize;
	if(strncmp(job->request.version, "9P2000", 6) != 0)
		sendmsg(job, "unknown 9P version");
	else{
		job->reply.version = "9P2000";
		sendmsg(job, 0);
	}
}

void
rauth(Job *job)
{
	sendmsg(job, "dns: authentication not required");
}

/*
 *  don't flush till all the slaves are done
 */
void
rflush(Job *job)
{
	flushjob(job->request.oldtag);
	sendmsg(job, 0);
}

void
rattach(Job *job, Mfile *mf)
{
	if(mf->user != nil)
		free(mf->user);
	mf->user = estrdup(job->request.uname);
	mf->qid.vers = vers++;
	mf->qid.type = QTDIR;
	mf->qid.path = 0LL;
	job->reply.qid = mf->qid;
	sendmsg(job, 0);
}

char*
rwalk(Job *job, Mfile *mf)
{
	char *err;
	char **elems;
	int nelems;
	int i;
	Mfile *nmf;
	Qid qid;

	err = 0;
	nmf = nil;
	elems = job->request.wname;
	nelems = job->request.nwname;
	job->reply.nwqid = 0;

	if(job->request.newfid != job->request.fid){
		/* clone fid */
		nmf = copyfid(mf, job->request.newfid);
		if(nmf == nil){
			err = "clone bad newfid";
			goto send;
		}
		mf = nmf;
	}
	/* else nmf will be nil */

	qid = mf->qid;
	if(nelems > 0){
		/* walk fid */
		for(i=0; i<nelems && i<MAXWELEM; i++){
			if((qid.type & QTDIR) == 0){
				err = "not a directory";
				break;
			}
			if(strcmp(elems[i], "..") == 0 || strcmp(elems[i], ".") == 0){
				qid.type = QTDIR;
				qid.path = Qdir;
    Found:
				job->reply.wqid[i] = qid;
				job->reply.nwqid++;
				continue;
			}
			if(strcmp(elems[i], "dns") == 0){
				qid.type = QTFILE;
				qid.path = Qdns;
				goto Found;
			}
			err = "file does not exist";
			break;
		}
	}

    send:
	if(nmf != nil && (err!=nil || job->reply.nwqid<nelems))
		freefid(nmf);
	if(err == nil)
		mf->qid = qid;
	sendmsg(job, err);
	return err;
}

void
ropen(Job *job, Mfile *mf)
{
	int mode;
	char *err;

	err = 0;
	mode = job->request.mode;
	if(mf->qid.type & QTDIR){
		if(mode)
			err = "permission denied";
	}
	job->reply.qid = mf->qid;
	job->reply.iounit = 0;
	sendmsg(job, err);
}

void
rcreate(Job *job, Mfile *mf)
{
	USED(mf);
	sendmsg(job, "creation permission denied");
}

void
rread(Job *job, Mfile *mf)
{
	int i, n, cnt;
	long off;
	Dir dir;
	uchar buf[Maxfdata];
	char *err;
	long clock;

	n = 0;
	err = 0;
	off = job->request.offset;
	cnt = job->request.count;
	if(mf->qid.type & QTDIR){
		clock = time(0);
		if(off == 0){
			dir.name = "dns";
			dir.qid.type = QTFILE;
			dir.qid.vers = vers;
			dir.qid.path = Qdns;
			dir.mode = 0666;
			dir.length = 0;
			dir.uid = mf->user;
			dir.gid = mf->user;
			dir.muid = mf->user;
			dir.atime = clock;	/* wrong */
			dir.mtime = clock;	/* wrong */
			n = convD2M(&dir, buf, sizeof buf);
		}
		job->reply.data = (char*)buf;
	} else {
		for(i = 1; i <= mf->nrr; i++)
			if(mf->rr[i] > off)
				break;
		if(i > mf->nrr)
			goto send;
		if(off + cnt > mf->rr[i])
			n = mf->rr[i] - off;
		else
			n = cnt;
		job->reply.data = mf->reply + off;
	}
send:
	job->reply.count = n;
	sendmsg(job, err);
}

void
rwrite(Job *job, Mfile *mf, Request *req)
{
	int cnt, rooted, status;
	long n;
	char *err, *p, *atype;
	RR *rp, *tp, *neg;
	int wantsav;
	static char *dumpfile;

	err = 0;
	cnt = job->request.count;
	if(mf->qid.type & QTDIR){
		err = "can't write directory";
		goto send;
	}
	if(cnt >= Maxrequest){
		err = "request too long";
		goto send;
	}
	job->request.data[cnt] = 0;
	if(cnt > 0 && job->request.data[cnt-1] == '\n')
		job->request.data[cnt-1] = 0;

	/*
	 *  special commands
	 */
	p = job->request.data;
	if(strcmp(p, "debug")==0){
		debug ^= 1;
		goto send;
	} else if(strcmp(p, "dump")==0){
		if(dumpfile == nil)
			dumpfile = unsharp("#9/ndb/dnsdump");
		dndump(dumpfile);
		goto send;
	} else if(strncmp(p, "dump ", 5) == 0){
		if(*(p+5))
			dndump(p+5);
		else
			err = "bad filename";
		goto send;
	} else if(strcmp(p, "refresh")==0){
		needrefresh = 1;
		goto send;
	}

	/*
	 *  kill previous reply
	 */
	mf->nrr = 0;
	mf->rr[0] = 0;

	/*
	 *  break up request (into a name and a type)
	 */
	atype = strchr(job->request.data, ' ');
	if(atype == 0){
		err = "illegal request";
		goto send;
	} else
		*atype++ = 0;

	/*
	 *  tracing request
	 */
	if(strcmp(atype, "trace") == 0){
		if(trace)
			free(trace);
		if(*job->request.data)
			trace = estrdup(job->request.data);
		else
			trace = 0;
		goto send;
	}

	mf->type = rrtype(atype);
	if(mf->type < 0){
		err = "unknown type";
		goto send;
	}

	p = atype - 2;
	if(p >= job->request.data && *p == '.'){
		rooted = 1;
		*p = 0;
	} else
		rooted = 0;

	p = job->request.data;
	if(*p == '!'){
		wantsav = 1;
		p++;
	} else
		wantsav = 0;
	dncheck(0, 1);
	rp = dnresolve(p, Cin, mf->type, req, 0, 0, Recurse, rooted, &status);
	dncheck(0, 1);
	neg = rrremneg(&rp);
	if(neg){
		status = neg->negrcode;
		rrfreelist(neg);
	}
	if(rp == 0){
		switch(status){
		case Rname:
			err = "name does not exist";
			break;
		case Rserver:
			err = "dns failure";
			break;
		default:
			err = "resource does not exist";
			break;
		}
	} else {
		lock(&joblock);
		if(!job->flushed){
			/* format data to be read later */
			n = 0;
			mf->nrr = 0;
			for(tp = rp; mf->nrr < Maxrrr-1 && n < Maxreply && tp &&
					tsame(mf->type, tp->type); tp = tp->next){
				mf->rr[mf->nrr++] = n;
				if(wantsav)
					n += snprint(mf->reply+n, Maxreply-n, "%Q", tp);
				else
					n += snprint(mf->reply+n, Maxreply-n, "%R", tp);
			}
			mf->rr[mf->nrr] = n;
		}
		unlock(&joblock);
		rrfreelist(rp);
	}

    send:
	dncheck(0, 1);
	job->reply.count = cnt;
	sendmsg(job, err);
}

void
rclunk(Job *job, Mfile *mf)
{
	freefid(mf);
	sendmsg(job, 0);
}

void
rremove(Job *job, Mfile *mf)
{
	USED(mf);
	sendmsg(job, "remove permission denied");
}

void
rstat(Job *job, Mfile *mf)
{
	Dir dir;
	uchar buf[IOHDRSZ+Maxfdata];

	if(mf->qid.type & QTDIR){
		dir.name = ".";
		dir.mode = DMDIR|0555;
	} else {
		dir.name = "dns";
		dir.mode = 0666;
	}
	dir.qid = mf->qid;
	dir.length = 0;
	dir.uid = mf->user;
	dir.gid = mf->user;
	dir.muid = mf->user;
	dir.atime = dir.mtime = time(0);
	job->reply.nstat = convD2M(&dir, buf, sizeof buf);
	job->reply.stat = buf;
	sendmsg(job, 0);
}

void
rwstat(Job *job, Mfile *mf)
{
	USED(mf);
	sendmsg(job, "wstat permission denied");
}

void
sendmsg(Job *job, char *err)
{
	int n;
	uchar mdata[IOHDRSZ + Maxfdata];
	char ename[ERRMAX];

	if(err){
		job->reply.type = Rerror;
		snprint(ename, sizeof(ename), "dns: %s", err);
		job->reply.ename = ename;
	}else{
		job->reply.type = job->request.type+1;
	}
	job->reply.tag = job->request.tag;
	n = convS2M(&job->reply, mdata, sizeof mdata);
	if(n == 0){
		syslog(1, logfile, "sendmsg convS2M of %F returns 0", &job->reply);
		abort();
	}
	lock(&joblock);
	if(job->flushed == 0)
		if(write(mfd[1], mdata, n)!=n)
			sysfatal("mount write");
	unlock(&joblock);
	if(debug)
		syslog(0, logfile, "%F %d", &job->reply, n);
}

/*
 *  the following varies between dnsdebug and dns
 */
void
logreply(int id, uchar *addr, DNSmsg *mp)
{
	RR *rp;

	syslog(0, LOG, "%d: rcvd %I flags:%s%s%s%s%s", id, addr,
		mp->flags & Fauth ? " auth" : "",
		mp->flags & Ftrunc ? " trunc" : "",
		mp->flags & Frecurse ? " rd" : "",
		mp->flags & Fcanrec ? " ra" : "",
		mp->flags & (Fauth|Rname) == (Fauth|Rname) ?
		" nx" : "");
	for(rp = mp->qd; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I qd %s", id, addr, rp->owner->name);
	for(rp = mp->an; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I an %R", id, addr, rp);
	for(rp = mp->ns; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I ns %R", id, addr, rp);
	for(rp = mp->ar; rp != nil; rp = rp->next)
		syslog(0, LOG, "%d: rcvd %I ar %R", id, addr, rp);
}

void
logsend(int id, int subid, uchar *addr, char *sname, char *rname, int type)
{
	char buf[12];

	syslog(0, LOG, "%d.%d: sending to %I/%s %s %s",
		id, subid, addr, sname, rname, rrname(type, buf, sizeof buf));
}

RR*
getdnsservers(int class)
{
	return dnsservers(class);
}
