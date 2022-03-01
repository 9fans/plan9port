#include "common.h"
#include <auth.h>
#include <fcall.h>
#include <libsec.h>
#include <9pclient.h> /* jpc */
#include <thread.h> /* jpc */
#include "dat.h"

enum
{
	OPERM	= 0x3,		/* mask of all permission types in open mode */
};

typedef struct Fid Fid;

struct Fid
{
	Qid	qid;
	short	busy;
	short	open;
	int	fid;
	Fid	*next;
	Mailbox	*mb;
	Message	*m;
	Message *mtop;		/* top level message */

	/*finger pointers to speed up reads of large directories */
	long	foff;	/* offset/DIRLEN of finger */
	Message	*fptr;	/* pointer to message at off */
	int	fvers;	/* mailbox version when finger was saved */
};

ulong	path;		/* incremented for each new file */
Fid	*fids;
int	mfd[2];
char	user[Elemlen];
int	messagesize = 4*1024*IOHDRSZ;
uchar	mdata[8*1024*IOHDRSZ];
uchar	mbuf[8*1024*IOHDRSZ];
Fcall	thdr;
Fcall	rhdr;
int	fflg;
char	*mntpt;
int	biffing;
int	plumbing = 1;

QLock	mbllock;
Mailbox	*mbl;

Fid		*newfid(int);
void		error(char*);
void		io(void);
void		*erealloc(void*, ulong);
void		*emalloc(ulong);
void		usage(void);
void		run_io(void*);
void		reader(void*);
int		readheader(Message*, char*, int, int);
int		cistrncmp(char*, char*, int);
int		tokenconvert(String*, char*, int);
String*		stringconvert(String*, char*, int);
void		post(char*, char*, int);

char	*rflush(Fid*), *rauth(Fid*),
	*rattach(Fid*), *rwalk(Fid*),
	*ropen(Fid*), *rcreate(Fid*),
	*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
	*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*),
	*rversion(Fid*);

char 	*(*fcalls[])(Fid*) = {
	[Tflush]	rflush,
	[Tversion]	rversion,
	[Tauth]	rauth,
	[Tattach]	rattach,
	[Twalk]		rwalk,
	[Topen]		ropen,
	[Tcreate]	rcreate,
	[Tread]		rread,
	[Twrite]	rwrite,
	[Tclunk]	rclunk,
	[Tremove]	rremove,
	[Tstat]		rstat,
	[Twstat]	rwstat
};

char	Eperm[] =	"permission denied";
char	Enotdir[] =	"not a directory";
char	Enoauth[] =	"upas/fs: authentication not required";
char	Enotexist[] =	"file does not exist";
char	Einuse[] =	"file in use";
char	Eexist[] =	"file exists";
char	Enotowner[] =	"not owner";
char	Eisopen[] = 	"file already open for I/O";
char	Excl[] = 	"exclusive use file already open";
char	Ename[] = 	"illegal name";
char	Ebadctl[] =	"unknown control message";

char *dirtab[] =
{
[Qdir]		".",
[Qbody]		"body",
[Qbcc]		"bcc",
[Qcc]		"cc",
[Qdate]		"date",
[Qdigest]	"digest",
[Qdisposition]	"disposition",
[Qfilename]	"filename",
[Qfrom]		"from",
[Qheader]	"header",
[Qinfo]		"info",
[Qinreplyto]	"inreplyto",
[Qlines]	"lines",
[Qmimeheader]	"mimeheader",
[Qmessageid]	"messageid",
[Qraw]		"raw",
[Qrawunix]	"rawunix",
[Qrawbody]	"rawbody",
[Qrawheader]	"rawheader",
[Qreplyto]	"replyto",
[Qsender]	"sender",
[Qsubject]	"subject",
[Qto]		"to",
[Qtype]		"type",
[Qunixdate]	"unixdate",
[Qunixheader]	"unixheader",
[Qctl]		"ctl",
[Qmboxctl]	"ctl"
};

enum
{
	Hsize=	1277
};

Hash	*htab[Hsize];

int	debug;
int	fflag;
int	logging;

void
usage(void)
{
	fprint(2, "usage: %s [-b -m mountpoint]\n", argv0);
	threadexits("usage");
}

void
notifyf(void *a, char *s)
{
	USED(a);
	if(strncmp(s, "interrupt", 9) == 0)
		noted(NCONT);
	noted(NDFLT);
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	int p[2], std, nodflt;
	char maildir[128];
	char mbox[128];
	char *mboxfile, *err;
	char srvfile[64];
	int srvpost;

	rfork(RFNOTEG);
	mntpt = nil;
	fflag = 0;
	mboxfile = nil;
	std = 0;
	nodflt = 0;
	srvpost = 0;

	ARGBEGIN{
	case 'b':
		biffing = 1;
		break;
	case 'f':
		fflag = 1;
		mboxfile = ARGF();
		break;
	case 'm':
		mntpt = ARGF();
		break;
	case 'd':
		debug = 1;
		break;
	case 'p':
		plumbing = 0;
		break;
	case 's':
		srvpost = 1;
		break;
	case 'l':
		logging = 1;
		break;
	case 'n':
		nodflt = 1;
		break;
	default:
		usage();
	}ARGEND

	if(pipe(p) < 0)
		error("pipe failed");
	mfd[0] = p[0];
	mfd[1] = p[0];

	notify(notifyf);
	strcpy(user, getuser());
	if(mntpt == nil){
		snprint(maildir, sizeof(maildir), "/mail/fs");
		mntpt = maildir;
	}
	if(mboxfile == nil && !nodflt){
		snprint(mbox, sizeof(mbox), "/mail/box/%s/mbox", user);
		mboxfile = mbox;
		std = 1;
	}

	if(debug)
		fmtinstall('F', fcallfmt);

	if(mboxfile != nil){
		err = newmbox(mboxfile, "mbox", std);
		if(err != nil)
			sysfatal("opening mailbox: %s", err);
	}

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){ /* jpc removed RFEND */
	case -1:
		error("fork");
	case 0:
		henter(PATH(0, Qtop), dirtab[Qctl],
			(Qid){PATH(0, Qctl), 0, QTFILE}, nil, nil);
		close(p[1]);
		io();
		postnote(PNGROUP, getpid(), "die yankee pig dog");
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
		if(srvpost){
			sprint(srvfile, "/srv/upasfs.%s", user);
			/* post(srvfile, "upasfs", p[1]);  jpc */
			post9pservice(p[1], "upasfs", nil);   /* jpc */
		} else {
			error("tried to mount, fixme");     /* jpc */
			/* if(mount(p[1], -1, mntpt, MREPL, "") < 0)
				error("mount failed");   jpc */
		}
	}
	threadexits(0);
}

void run_io(void *v) {
	int *p;

	p =  v;
	henter(PATH(0, Qtop), dirtab[Qctl],
		(Qid){PATH(0, Qctl), 0, QTFILE}, nil, nil);
	close(p[1]);
	io();
	postnote(PNGROUP, getpid(), "die yankee pig dog");
}

static int
fileinfo(Message *m, int t, char **pp)
{
	char *p;
	int len;

	p = "";
	len = 0;
	switch(t){
	case Qbody:
		p = m->body;
		len = m->bend - m->body;
		break;
	case Qbcc:
		if(m->bcc822){
			p = s_to_c(m->bcc822);
			len = strlen(p);
		}
		break;
	case Qcc:
		if(m->cc822){
			p = s_to_c(m->cc822);
			len = strlen(p);
		}
		break;
	case Qdisposition:
		switch(m->disposition){
		case Dinline:
			p = "inline";
			break;
		case Dfile:
			p = "file";
			break;
		}
		len = strlen(p);
		break;
	case Qdate:
		if(m->date822){
			p = s_to_c(m->date822);
			len = strlen(p);
		} else if(m->unixdate != nil){
			p = s_to_c(m->unixdate);
			len = strlen(p);
		}
		break;
	case Qfilename:
		if(m->filename){
			p = s_to_c(m->filename);
			len = strlen(p);
		}
		break;
	case Qinreplyto:
		if(m->inreplyto822){
			p = s_to_c(m->inreplyto822);
			len = strlen(p);
		}
		break;
	case Qmessageid:
		if(m->messageid822){
			p = s_to_c(m->messageid822);
			len = strlen(p);
		}
		break;
	case Qfrom:
		if(m->from822){
			p = s_to_c(m->from822);
			len = strlen(p);
		} else if(m->unixfrom != nil){
			p = s_to_c(m->unixfrom);
			len = strlen(p);
		}
		break;
	case Qheader:
		p = m->header;
		len = headerlen(m);
		break;
	case Qlines:
		p = m->lines;
		if(*p == 0)
			countlines(m);
		len = strlen(m->lines);
		break;
	case Qraw:
		p = m->start;
		if(strncmp(m->start, "From ", 5) == 0){
			p = strchr(p, '\n');
			if(p == nil)
				p = m->start;
			else
				p++;
		}
		len = m->end - p;
		break;
	case Qrawunix:
		p = m->start;
		len = m->end - p;
		break;
	case Qrawbody:
		p = m->rbody;
		len = m->rbend - p;
		break;
	case Qrawheader:
		p = m->header;
		len = m->hend - p;
		break;
	case Qmimeheader:
		p = m->mheader;
		len = m->mhend - p;
		break;
	case Qreplyto:
		p = nil;
		if(m->replyto822 != nil){
			p = s_to_c(m->replyto822);
			len = strlen(p);
		} else if(m->from822 != nil){
			p = s_to_c(m->from822);
			len = strlen(p);
		} else if(m->sender822 != nil){
			p = s_to_c(m->sender822);
			len = strlen(p);
		} else if(m->unixfrom != nil){
			p = s_to_c(m->unixfrom);
			len = strlen(p);
		}
		break;
	case Qsender:
		if(m->sender822){
			p = s_to_c(m->sender822);
			len = strlen(p);
		}
		break;
	case Qsubject:
		p = nil;
		if(m->subject822){
			p = s_to_c(m->subject822);
			len = strlen(p);
		}
		break;
	case Qto:
		if(m->to822){
			p = s_to_c(m->to822);
			len = strlen(p);
		}
		break;
	case Qtype:
		if(m->type){
			p = s_to_c(m->type);
			len = strlen(p);
		}
		break;
	case Qunixdate:
		if(m->unixdate){
			p = s_to_c(m->unixdate);
			len = strlen(p);
		}
		break;
	case Qunixheader:
		if(m->unixheader){
			p = s_to_c(m->unixheader);
			len = s_len(m->unixheader);
		}
		break;
	case Qdigest:
		if(m->sdigest){
			p = s_to_c(m->sdigest);
			len = strlen(p);
		}
		break;
	}
	*pp = p;
	return len;
}

int infofields[] = {
	Qfrom,
	Qto,
	Qcc,
	Qreplyto,
	Qunixdate,
	Qsubject,
	Qtype,
	Qdisposition,
	Qfilename,
	Qdigest,
	Qbcc,
	Qinreplyto,
	Qdate,
	Qsender,
	Qmessageid,
	Qlines,
	-1
};

static int
readinfo(Message *m, char *buf, long off, int count)
{
	char *p;
	int len, i, n;
	String *s;

	s = s_new();
	len = 0;
	for(i = 0; len < count && infofields[i] >= 0; i++){
		n = fileinfo(m, infofields[i], &p);
		s = stringconvert(s, p, n);
		s_append(s, "\n");
		p = s_to_c(s);
		n = strlen(p);
		if(off > 0){
			if(off >= n){
				off -= n;
				continue;
			}
			p += off;
			n -= off;
			off = 0;
		}
		if(n > count - len)
			n = count - len;
		if(buf)
			memmove(buf+len, p, n);
		len += n;
	}
	s_free(s);
	return len;
}

static void
mkstat(Dir *d, Mailbox *mb, Message *m, int t)
{
	char *p;

	d->uid = user;
	d->gid = user;
	d->muid = user;
	d->mode = 0444;
	d->qid.vers = 0;
	d->qid.type = QTFILE;
	d->type = 0;
	d->dev = 0;
	if(mb != nil && mb->d != nil){
		d->atime = mb->d->atime;
		d->mtime = mb->d->mtime;
	} else {
		d->atime = time(0);
		d->mtime = d->atime;
	}

	switch(t){
	case Qtop:
		d->name = ".";
		d->mode = DMDIR|0555;
		d->atime = d->mtime = time(0);
		d->length = 0;
		d->qid.path = PATH(0, Qtop);
		d->qid.type = QTDIR;
		break;
	case Qmbox:
		d->name = mb->name;
		d->mode = DMDIR|0555;
		d->length = 0;
		d->qid.path = PATH(mb->id, Qmbox);
		d->qid.type = QTDIR;
		d->qid.vers = mb->vers;
		break;
	case Qdir:
		d->name = m->name;
		d->mode = DMDIR|0555;
		d->length = 0;
		d->qid.path = PATH(m->id, Qdir);
		d->qid.type = QTDIR;
		break;
	case Qctl:
		d->name = dirtab[t];
		d->mode = 0666;
		d->atime = d->mtime = time(0);
		d->length = 0;
		d->qid.path = PATH(0, Qctl);
		break;
	case Qmboxctl:
		d->name = dirtab[t];
		d->mode = 0222;
		d->atime = d->mtime = time(0);
		d->length = 0;
		d->qid.path = PATH(mb->id, Qmboxctl);
		break;
	case Qinfo:
		d->name = dirtab[t];
		d->length = readinfo(m, nil, 0, 1<<30);
		d->qid.path = PATH(m->id, t);
		break;
	default:
		d->name = dirtab[t];
		d->length = fileinfo(m, t, &p);
		d->qid.path = PATH(m->id, t);
		break;
	}
}

char*
rversion(Fid* dummy)
{
	Fid *f;

	if(thdr.msize < 256)
		return "max messagesize too small";
	if(thdr.msize < messagesize)
		messagesize = thdr.msize;
	rhdr.msize = messagesize;
	if(strncmp(thdr.version, "9P2000", 6) != 0)
		return "unknown 9P version";
	else
		rhdr.version = "9P2000";
	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);
	return nil;
}

char*
rauth(Fid* dummy)
{
	return Enoauth;
}

char*
rflush(Fid *f)
{
	USED(f);
	return 0;
}

char*
rattach(Fid *f)
{
	f->busy = 1;
	f->m = nil;
	f->mb = nil;
	f->qid.path = PATH(0, Qtop);
	f->qid.type = QTDIR;
	f->qid.vers = 0;
	rhdr.qid = f->qid;
	if(strcmp(thdr.uname, user) != 0)
		return Eperm;
	return 0;
}

static Fid*
doclone(Fid *f, int nfid)
{
	Fid *nf;

	nf = newfid(nfid);
	if(nf->busy)
		return nil;
	nf->busy = 1;
	nf->open = 0;
	nf->m = f->m;
	nf->mtop = f->mtop;
	nf->mb = f->mb;
	if(f->mb != nil)
		mboxincref(f->mb);
	if(f->mtop != nil){
		qlock(&f->mb->ql);
		msgincref(f->mtop);
		qunlock(&f->mb->ql);
	}
	nf->qid = f->qid;
	return nf;
}

char*
dowalk(Fid *f, char *name)
{
	int t;
	Mailbox *omb, *mb;
	char *rv, *p;
	Hash *h;

	t = FILE(f->qid.path);

	rv = Enotexist;

	omb = f->mb;
	if(omb)
		qlock(&omb->ql);
	else
		qlock(&mbllock);

	/* this must catch everything except . and .. */
retry:
	h = hlook(f->qid.path, name);
	if(h != nil){
		f->mb = h->mb;
		f->m = h->m;
		switch(t){
		case Qtop:
			if(f->mb != nil)
				mboxincref(f->mb);
			break;
		case Qmbox:
			if(f->m){
				msgincref(f->m);
				f->mtop = f->m;
			}
			break;
		}
		f->qid = h->qid;
		rv = nil;
	} else if((p = strchr(name, '.')) != nil && *name != '.'){
		*p = 0;
		goto retry;
	}

	if(omb)
		qunlock(&omb->ql);
	else
		qunlock(&mbllock);
	if(rv == nil)
		return rv;

	if(strcmp(name, ".") == 0)
		return nil;

	if(f->qid.type != QTDIR)
		return Enotdir;

	if(strcmp(name, "..") == 0){
		switch(t){
		case Qtop:
			f->qid.path = PATH(0, Qtop);
			f->qid.type = QTDIR;
			f->qid.vers = 0;
			break;
		case Qmbox:
			f->qid.path = PATH(0, Qtop);
			f->qid.type = QTDIR;
			f->qid.vers = 0;
			qlock(&mbllock);
			mb = f->mb;
			f->mb = nil;
			mboxdecref(mb);
			qunlock(&mbllock);
			break;
		case Qdir:
			qlock(&f->mb->ql);
			if(f->m->whole == f->mb->root){
				f->qid.path = PATH(f->mb->id, Qmbox);
				f->qid.type = QTDIR;
				f->qid.vers = f->mb->d->qid.vers;
				msgdecref(f->mb, f->mtop);
				f->m = f->mtop = nil;
			} else {
				f->m = f->m->whole;
				f->qid.path = PATH(f->m->id, Qdir);
				f->qid.type = QTDIR;
			}
			qunlock(&f->mb->ql);
			break;
		}
		rv = nil;
	}
	return rv;
}

char*
rwalk(Fid *f)
{
	Fid *nf;
	char *rv;
	int i;

	if(f->open)
		return Eisopen;

	rhdr.nwqid = 0;
	nf = nil;

	/* clone if requested */
	if(thdr.newfid != thdr.fid){
		nf = doclone(f, thdr.newfid);
		if(nf == nil)
			return "new fid in use";
		f = nf;
	}

	/* if it's just a clone, return */
	if(thdr.nwname == 0 && nf != nil)
		return nil;

	/* walk each element */
	rv = nil;
	for(i = 0; i < thdr.nwname; i++){
		rv = dowalk(f, thdr.wname[i]);
		if(rv != nil){
			if(nf != nil)
				rclunk(nf);
			break;
		}
		rhdr.wqid[i] = f->qid;
	}
	rhdr.nwqid = i;

	/* we only error out if no walk  */
	if(i > 0)
		rv = nil;

	return rv;
}

char *
ropen(Fid *f)
{
	int file;

	if(f->open)
		return Eisopen;

	file = FILE(f->qid.path);
	if(thdr.mode != OREAD)
		if(file != Qctl && file != Qmboxctl)
			return Eperm;

	/* make sure we've decoded */
	if(file == Qbody){
		if(f->m->decoded == 0)
			decode(f->m);
		if(f->m->converted == 0)
			convert(f->m);
	}

	rhdr.iounit = 0;
	rhdr.qid = f->qid;
	f->open = 1;
	return 0;
}

char *
rcreate(Fid* dummy)
{
	return Eperm;
}

int
readtopdir(Fid* dummy, uchar *buf, long off, int cnt, int blen)
{
	Dir d;
	int m, n;
	long pos;
	Mailbox *mb;

	n = 0;
	pos = 0;
	mkstat(&d, nil, nil, Qctl);
	m = convD2M(&d, &buf[n], blen);
	if(off <= pos){
		if(m <= BIT16SZ || m > cnt)
			return 0;
		n += m;
		cnt -= m;
	}
	pos += m;

	for(mb = mbl; mb != nil; mb = mb->next){
		mkstat(&d, mb, nil, Qmbox);
		m = convD2M(&d, &buf[n], blen-n);
		if(off <= pos){
			if(m <= BIT16SZ || m > cnt)
				break;
			n += m;
			cnt -= m;
		}
		pos += m;
	}
	return n;
}

int
readmboxdir(Fid *f, uchar *buf, long off, int cnt, int blen)
{
	Dir d;
	int n, m;
	long pos;
	Message *msg;

	n = 0;
	if(f->mb->ctl){
		mkstat(&d, f->mb, nil, Qmboxctl);
		m = convD2M(&d, &buf[n], blen);
		if(off == 0){
			if(m <= BIT16SZ || m > cnt){
				f->fptr = nil;
				return 0;
			}
			n += m;
			cnt -= m;
		} else
			off -= m;
	}

	/* to avoid n**2 reads of the directory, use a saved finger pointer */
	if(f->mb->vers == f->fvers && off >= f->foff && f->fptr != nil){
		msg = f->fptr;
		pos = f->foff;
	} else {
		msg = f->mb->root->part;
		pos = 0;
	}

	for(; cnt > 0 && msg != nil; msg = msg->next){
		/* act like deleted files aren't there */
		if(msg->deleted)
			continue;

		mkstat(&d, f->mb, msg, Qdir);
		m = convD2M(&d, &buf[n], blen-n);
		if(off <= pos){
			if(m <= BIT16SZ || m > cnt)
				break;
			n += m;
			cnt -= m;
		}
		pos += m;
	}

	/* save a finger pointer for next read of the mbox directory */
	f->foff = pos;
	f->fptr = msg;
	f->fvers = f->mb->vers;

	return n;
}

int
readmsgdir(Fid *f, uchar *buf, long off, int cnt, int blen)
{
	Dir d;
	int i, n, m;
	long pos;
	Message *msg;

	n = 0;
	pos = 0;
	for(i = 0; i < Qmax; i++){
		mkstat(&d, f->mb, f->m, i);
		m = convD2M(&d, &buf[n], blen-n);
		if(off <= pos){
			if(m <= BIT16SZ || m > cnt)
				return n;
			n += m;
			cnt -= m;
		}
		pos += m;
	}
	for(msg = f->m->part; msg != nil; msg = msg->next){
		mkstat(&d, f->mb, msg, Qdir);
		m = convD2M(&d, &buf[n], blen-n);
		if(off <= pos){
			if(m <= BIT16SZ || m > cnt)
				break;
			n += m;
			cnt -= m;
		}
		pos += m;
	}

	return n;
}

char*
rread(Fid *f)
{
	long off;
	int t, i, n, cnt;
	char *p;

	rhdr.count = 0;
	off = thdr.offset;
	cnt = thdr.count;

	if(cnt > messagesize - IOHDRSZ)
		cnt = messagesize - IOHDRSZ;

	rhdr.data = (char*)mbuf;

	t = FILE(f->qid.path);
	if(f->qid.type & QTDIR){
		if(t == Qtop) {
			qlock(&mbllock);
			n = readtopdir(f, mbuf, off, cnt, messagesize - IOHDRSZ);
			qunlock(&mbllock);
		} else if(t == Qmbox) {
			qlock(&f->mb->ql);
			if(off == 0)
				syncmbox(f->mb, 1);
			n = readmboxdir(f, mbuf, off, cnt, messagesize - IOHDRSZ);
			qunlock(&f->mb->ql);
		} else if(t == Qmboxctl) {
			n = 0;
		} else {
			n = readmsgdir(f, mbuf, off, cnt, messagesize - IOHDRSZ);
		}

		rhdr.count = n;
		return nil;
	}

	if(FILE(f->qid.path) == Qheader){
		rhdr.count = readheader(f->m, (char*)mbuf, off, cnt);
		return nil;
	}

	if(FILE(f->qid.path) == Qinfo){
		rhdr.count = readinfo(f->m, (char*)mbuf, off, cnt);
		return nil;
	}

	i = fileinfo(f->m, FILE(f->qid.path), &p);
	if(off < i){
		if((off + cnt) > i)
			cnt = i - off;
		memmove(mbuf, p + off, cnt);
		rhdr.count = cnt;
	}
	return nil;
}

char*
rwrite(Fid *f)
{
	char *err;
	char *token[1024];
	int t, n;
	String *file;

	t = FILE(f->qid.path);
	rhdr.count = thdr.count;
	switch(t){
	case Qctl:
		if(thdr.count == 0)
			return Ebadctl;
		if(thdr.data[thdr.count-1] == '\n')
			thdr.data[thdr.count-1] = 0;
		else
			thdr.data[thdr.count] = 0;
		n = tokenize(thdr.data, token, nelem(token));
		if(n == 0)
			return Ebadctl;
		if(strcmp(token[0], "open") == 0){
			file = s_new();
			switch(n){
			case 1:
				err = Ebadctl;
				break;
			case 2:
				mboxpath(token[1], getlog(), file, 0);
				err = newmbox(s_to_c(file), nil, 0);
				break;
			default:
				mboxpath(token[1], getlog(), file, 0);
				if(strchr(token[2], '/') != nil)
					err = "/ not allowed in mailbox name";
				else
					err = newmbox(s_to_c(file), token[2], 0);
				break;
			}
			s_free(file);
			return err;
		}
		if(strcmp(token[0], "close") == 0){
			if(n < 2)
				return nil;
			freembox(token[1]);
			return nil;
		}
		if(strcmp(token[0], "delete") == 0){
			if(n < 3)
				return nil;
			delmessages(n-1, &token[1]);
			return nil;
		}
		return Ebadctl;
	case Qmboxctl:
		if(f->mb && f->mb->ctl){
			if(thdr.count == 0)
				return Ebadctl;
			if(thdr.data[thdr.count-1] == '\n')
				thdr.data[thdr.count-1] = 0;
			else
				thdr.data[thdr.count] = 0;
			n = tokenize(thdr.data, token, nelem(token));
			if(n == 0)
				return Ebadctl;
			return (*f->mb->ctl)(f->mb, n, token);
		}
	}
	return Eperm;
}

char *
rclunk(Fid *f)
{
	Mailbox *mb;

	f->busy = 0;
	f->open = 0;
	if(f->mtop != nil){
		qlock(&f->mb->ql);
		msgdecref(f->mb, f->mtop);
		qunlock(&f->mb->ql);
	}
	f->m = f->mtop = nil;
	mb = f->mb;
	if(mb != nil){
		f->mb = nil;
		assert(mb->refs > 0);
		qlock(&mbllock);
		mboxdecref(mb);
		qunlock(&mbllock);
	}
	f->fid = -1;
	return 0;
}

char *
rremove(Fid *f)
{
	if(f->m != nil){
		if(f->m->deleted == 0)
			mailplumb(f->mb, f->m, 1);
		f->m->deleted = 1;
	}
	return rclunk(f);
}

char *
rstat(Fid *f)
{
	Dir d;

	if(FILE(f->qid.path) == Qmbox){
		qlock(&f->mb->ql);
		syncmbox(f->mb, 1);
		qunlock(&f->mb->ql);
	}
	mkstat(&d, f->mb, f->m, FILE(f->qid.path));
	rhdr.nstat = convD2M(&d, mbuf, messagesize - IOHDRSZ);
	rhdr.stat = mbuf;
	return 0;
}

char *
rwstat(Fid* dummy)
{
	return Eperm;
}

Fid *
newfid(int fid)
{
	Fid *f, *ff;

	ff = 0;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff){
		ff->fid = fid;
		ff->fptr = nil;
		return ff;
	}
	f = emalloc(sizeof *f);
	f->fid = fid;
	f->fptr = nil;
	f->next = fids;
	fids = f;
	return f;
}

int
fidmboxrefs(Mailbox *mb)
{
	Fid *f;
	int refs = 0;

	for(f = fids; f; f = f->next){
		if(f->mb == mb)
			refs++;
	}
	return refs;
}

void
io(void)
{
	char *err;
	int n, nw;

	/* start a process to watch the mailboxes*/
	if(plumbing){
		proccreate(reader, nil, 16000);
#if 0 /* jpc */
		switch(rfork(RFPROC|RFMEM)){
		case -1:
			/* oh well */
			break;
		case 0:
			reader();
			threadexits(nil);
		default:
			break;
		}
#endif /* jpc */
	}

	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error
		 */
		checkmboxrefs();
		n = read9pmsg(mfd[0], mdata, messagesize);
		if(n == 0)
			continue;
		if(n < 0)
			return;
		if(convM2S(mdata, n, &thdr) == 0)
			continue;

		if(debug)
			fprint(2, "%s:<-%F\n", argv0, &thdr);

		rhdr.data = (char*)mdata + messagesize;
		if(!fcalls[thdr.type])
			err = "bad fcall type";
		else
			err = (*fcalls[thdr.type])(newfid(thdr.fid));
		if(err){
			rhdr.type = Rerror;
			rhdr.ename = err;
		}else{
			rhdr.type = thdr.type + 1;
			rhdr.fid = thdr.fid;
		}
		rhdr.tag = thdr.tag;
		if(debug)
			fprint(2, "%s:->%F\n", argv0, &rhdr);/**/
		n = convS2M(&rhdr, mdata, messagesize);
		if((nw = write(mfd[1], mdata, n)) != n) {
			fprint(2,"wrote %d bytes\n",nw);
			error("mount write");
		}
	}
}

void
reader(void *dummy)
{
	ulong t;
	Dir *d;
	Mailbox *mb;

	sleep(15*1000);
	for(;;){
		t = time(0);
		qlock(&mbllock);
		for(mb = mbl; mb != nil; mb = mb->next){
			assert(mb->refs > 0);
			if(mb->waketime != 0 && t > mb->waketime){
				qlock(&mb->ql);
				mb->waketime = 0;
				break;
			}

			d = dirstat(mb->path);
			if(d == nil)
				continue;

			qlock(&mb->ql);
			if(mb->d)
			if(d->qid.path != mb->d->qid.path
			   || d->qid.vers != mb->d->qid.vers){
				free(d);
				break;
			}
			qunlock(&mb->ql);
			free(d);
		}
		qunlock(&mbllock);
		if(mb != nil){
			syncmbox(mb, 1);
			qunlock(&mb->ql);
		} else
			sleep(15*1000);
	}
}

int
newid(void)
{
	int rv;
	static int id;
	static Lock idlock;

	lock(&idlock);
	rv = ++id;
	unlock(&idlock);

	return rv;
}

void
error(char *s)
{
	postnote(PNGROUP, getpid(), "die yankee pig dog");
	fprint(2, "%s: %s: %r\n", argv0, s);
	threadexits(s);
}


typedef struct Ignorance Ignorance;
struct Ignorance
{
	Ignorance *next;
	char	*str;		/* string */
	int	partial;	/* true if not exact match */
};
Ignorance *ignorance;

/*
 *  read the file of headers to ignore
 */
void
readignore(void)
{
	char *p;
	Ignorance *i;
	Biobuf *b;

	if(ignorance != nil)
		return;

	b = Bopen("/mail/lib/ignore", OREAD);
	if(b == 0)
		return;
	while(p = Brdline(b, '\n')){
		p[Blinelen(b)-1] = 0;
		while(*p && (*p == ' ' || *p == '\t'))
			p++;
		if(*p == '#')
			continue;
		i = malloc(sizeof(Ignorance));
		if(i == 0)
			break;
		i->partial = strlen(p);
		i->str = strdup(p);
		if(i->str == 0){
			free(i);
			break;
		}
		i->next = ignorance;
		ignorance = i;
	}
	Bterm(b);
}

int
ignore(char *p)
{
	Ignorance *i;

	readignore();
	for(i = ignorance; i != nil; i = i->next)
		if(cistrncmp(i->str, p, i->partial) == 0)
			return 1;
	return 0;
}

int
hdrlen(char *p, char *e)
{
	char *ep;

	ep = p;
	do {
		ep = strchr(ep, '\n');
		if(ep == nil){
			ep = e;
			break;
		}
		ep++;
		if(ep >= e){
			ep = e;
			break;
		}
	} while(*ep == ' ' || *ep == '\t');
	return ep - p;
}

/* rfc2047 non-ascii */
typedef struct Charset Charset;
struct Charset {
	char *name;
	int len;
	int convert;
	char *tcsname;
} charsets[] =
{
	{ "us-ascii",		8,	1, nil, },
	{ "utf-8",		5,	0, nil, },
	{ "iso-8859-1",		10,	1, nil, },
	{ "iso-8859-2",		10,	2, "8859-2", },
	{ "big5",		4,	2, "big5", },
	{ "iso-2022-jp",	11, 2, "jis", },
	{ "windows-1251",	12,	2, "cp1251"},
	{ "koi8-r",		6,	2, "koi8"}
};

int
rfc2047convert(String *s, char *token, int len)
{
	char decoded[1024];
	char utfbuf[2*1024];
	int i;
	char *e, *x;

	if(len == 0)
		return -1;

	e = token+len-2;
	token += 2;

	/* bail if we don't understand the character set */
	for(i = 0; i < nelem(charsets); i++)
		if(cistrncmp(charsets[i].name, token, charsets[i].len) == 0)
		if(token[charsets[i].len] == '?'){
			token += charsets[i].len + 1;
			break;
		}
	if(i >= nelem(charsets))
		return -1;

	/* bail if it doesn't fit  */
	if(e-token > sizeof(decoded)-1)
		return -1;

	/* bail if we don't understand the encoding */
	if(cistrncmp(token, "b?", 2) == 0){
		token += 2;
		len = dec64((uchar*)decoded, sizeof(decoded), token, e-token);
		decoded[len] = 0;
	} else if(cistrncmp(token, "q?", 2) == 0){
		token += 2;
		len = decquoted(decoded, token, e);
		if(len > 0 && decoded[len-1] == '\n')
			len--;
		decoded[len] = 0;
	} else
		return -1;

	switch(charsets[i].convert){
	case 0:
		s_append(s, decoded);
		break;
	case 1:
		latin1toutf(utfbuf, decoded, decoded+len);
		s_append(s, utfbuf);
		break;
	case 2:
		if(xtoutf(charsets[i].tcsname, &x, decoded, decoded+len) <= 0){
			s_append(s, decoded);
		} else {
			s_append(s, x);
			free(x);
		}
		break;
	}

	return 0;
}

char*
rfc2047start(char *start, char *end)
{
	int quests;

	if(*--end != '=')
		return nil;
	if(*--end != '?')
		return nil;

	quests = 0;
	for(end--; end >= start; end--){
		switch(*end){
		case '=':
			if(quests == 3 && *(end+1) == '?')
				return end;
			break;
		case '?':
			++quests;
			break;
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			/* can't have white space in a token */
			return nil;
		}
	}
	return nil;
}

/* convert a header line */
String*
stringconvert(String *s, char *uneaten, int len)
{
	char *token;
	char *p;
	int i;

	s = s_reset(s);
	p = uneaten;
	for(i = 0; i < len; i++){
		if(*p++ == '='){
			token = rfc2047start(uneaten, p);
			if(token != nil){
				s_nappend(s, uneaten, token-uneaten);
				if(rfc2047convert(s, token, p - token) < 0)
					s_nappend(s, token, p - token);
				uneaten = p;
			}
		}
	}
	if(p > uneaten)
		s_nappend(s, uneaten, p-uneaten);
	return s;
}

int
readheader(Message *m, char *buf, int off, int cnt)
{
	char *p, *e;
	int n, ns;
	char *to = buf;
	String *s;

	p = m->header;
	e = m->hend;
	s = nil;

	/* copy in good headers */
	while(cnt > 0 && p < e){
		n = hdrlen(p, e);
		if(ignore(p)){
			p += n;
			continue;
		}

		/* rfc2047 processing */
		s = stringconvert(s, p, n);
		ns = s_len(s);
		if(off > 0){
			if(ns <= off){
				off -= ns;
				p += n;
				continue;
			}
			ns -= off;
		}
		if(ns > cnt)
			ns = cnt;
		memmove(to, s_to_c(s)+off, ns);
		to += ns;
		p += n;
		cnt -= ns;
		off = 0;
	}

	s_free(s);
	return to - buf;
}

int
headerlen(Message *m)
{
	char buf[1024];
	int i, n;

	if(m->hlen >= 0)
		return m->hlen;
	for(n = 0; ; n += i){
		i = readheader(m, buf, n, sizeof(buf));
		if(i <= 0)
			break;
	}
	m->hlen = n;
	return n;
}

QLock hashlock;

uint
hash(ulong ppath, char *name)
{
	uchar *p;
	uint h;

	h = 0;
	for(p = (uchar*)name; *p; p++)
		h = h*7 + *p;
	h += ppath;

	return h % Hsize;
}

Hash*
hlook(ulong ppath, char *name)
{
	int h;
	Hash *hp;

	qlock(&hashlock);
	h = hash(ppath, name);
	for(hp = htab[h]; hp != nil; hp = hp->next)
		if(ppath == hp->ppath && strcmp(name, hp->name) == 0){
			qunlock(&hashlock);
			return hp;
		}
	qunlock(&hashlock);
	return nil;
}

void
henter(ulong ppath, char *name, Qid qid, Message *m, Mailbox *mb)
{
	int h;
	Hash *hp, **l;

	qlock(&hashlock);
	h = hash(ppath, name);
	for(l = &htab[h]; *l != nil; l = &(*l)->next){
		hp = *l;
		if(ppath == hp->ppath && strcmp(name, hp->name) == 0){
			hp->m = m;
			hp->mb = mb;
			hp->qid = qid;
			qunlock(&hashlock);
			return;
		}
	}

	*l = hp = emalloc(sizeof(*hp));
	hp->m = m;
	hp->mb = mb;
	hp->qid = qid;
	hp->name = name;
	hp->ppath = ppath;
	qunlock(&hashlock);
}

void
hfree(ulong ppath, char *name)
{
	int h;
	Hash *hp, **l;

	qlock(&hashlock);
	h = hash(ppath, name);
	for(l = &htab[h]; *l != nil; l = &(*l)->next){
		hp = *l;
		if(ppath == hp->ppath && strcmp(name, hp->name) == 0){
			hp->mb = nil;
			*l = hp->next;
			free(hp);
			break;
		}
	}
	qunlock(&hashlock);
}

int
hashmboxrefs(Mailbox *mb)
{
	int h;
	Hash *hp;
	int refs = 0;

	qlock(&hashlock);
	for(h = 0; h < Hsize; h++){
		for(hp = htab[h]; hp != nil; hp = hp->next)
			if(hp->mb == mb)
				refs++;
	}
	qunlock(&hashlock);
	return refs;
}

void
checkmboxrefs(void)
{
	int f, refs;
	Mailbox *mb;

	qlock(&mbllock);
	for(mb=mbl; mb; mb=mb->next){
		qlock(&mb->ql);
		refs = (f=fidmboxrefs(mb))+1;
		if(refs != mb->refs){
			fprint(2, "mbox %s %s ref mismatch actual %d (%d+1) expected %d\n", mb->name, mb->path, refs, f, mb->refs);
			abort();
		}
		qunlock(&mb->ql);
	}
	qunlock(&mbllock);
}

void
post(char *name, char *envname, int srvfd)
{
	int fd;
	char buf[32];

	fd = create(name, OWRITE, 0600);
	if(fd < 0)
		error("post failed");
	sprint(buf, "%d",srvfd);
	if(write(fd, buf, strlen(buf)) != strlen(buf))
		error("srv write");
	close(fd);
	putenv(envname, name);
}
