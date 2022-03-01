#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <errno.h>

#define err err9pserve /* Darwin x86 */

enum
{
	STACK = 32768,
	NHASH = 31,
	MAXMSG = 64,	/* per connection */
	MAXMSGSIZE = 4*1024*1024
};

typedef struct Hash Hash;
typedef struct Fid Fid;
typedef struct Msg Msg;
typedef struct Conn Conn;
typedef struct Queue Queue;

struct Hash
{
	Hash *next;
	uint n;
	void *v;
};

struct Fid
{
	int fid;
	int ref;
	int cfid;
	int openfd;
	int offset;
	int coffset;
	int isdir;
	Fid *next;
};

struct Msg
{
	Conn *c;
	int internal;
	int sync;
	int ref;
	int ctag;
	int tag;
	int isopenfd;
	Fcall tx;
	Fcall rx;
	Fid *fid;
	Fid *newfid;
	Fid *afid;
	Msg *oldm;
	Msg *next;
	uchar *tpkt;
	uchar *rpkt;
};

struct Conn
{
	int fd;
	int fdmode;
	Fid *fdfid;
	int nmsg;
	int nfid;
	Channel *inc;
	Channel *internal;
	int inputstalled;
	char dir[40];
	Hash *tag[NHASH];
	Hash *fid[NHASH];
	Queue *outq;
	Queue *inq;
	Channel *outqdead;
};

char *xaname;
char *addr;
int afd;
char adir[40];
int isunix;
Queue *outq;
Queue *inq;
int verbose = 0;
int logging = 0;
int msize = 8192;
u32int xafid = NOFID;
int attached;
int versioned;
int noauth;

void *gethash(Hash**, uint);
int puthash(Hash**, uint, void*);
int delhash(Hash**, uint, void*);
Msg *mread9p(Ioproc*, int);
int mwrite9p(Ioproc*, int, uchar*);
uchar *read9ppkt(Ioproc*, int);
int write9ppkt(int, uchar*);
Msg *msgnew(int);
void msgput(Msg*);
void msgclear(Msg*);
Msg *msgget(int);
void msgincref(Msg*);
Fid *fidnew(int);
void fidput(Fid*);
void *emalloc(int);
void *erealloc(void*, int);
Queue *qalloc(void);
int sendq(Queue*, void*);
void *recvq(Queue*);
void connthread(void*);
void connoutthread(void*);
void listenthread(void*);
void outputthread(void*);
void inputthread(void*);
void rewritehdr(Fcall*, uchar*);
void repack(Fcall*, uchar**);
int tlisten(char*, char*);
int taccept(int, char*);
int iolisten(Ioproc*, char*, char*);
int ioaccept(Ioproc*, int, char*);
int iorecvfd(Ioproc*, int);
int iosendfd(Ioproc*, int, int);
void mainproc(void*);
int ignorepipe(void*, char*);
int timefmt(Fmt*);
void dorootstat(void);

void
usage(void)
{
	fprint(2, "usage: 9pserve [-lnv] [-A aname afid] [-c addr] [-M msize] address\n");
	fprint(2, "\treads/writes 9P messages on stdin/stdout\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

uchar vbuf[128];
extern int _threaddebuglevel;
void
threadmain(int argc, char **argv)
{
	char *file, *x, *addr;
	int fd;

	rfork(RFNOTEG);
	x = getenv("verbose9pserve");
	if(x){
		verbose = atoi(x);
		fprint(2, "verbose9pserve %s => %d\n", x, verbose);
	}
	ARGBEGIN{
	default:
		usage();
	case 'A':
		attached = 1;
		xaname = EARGF(usage());
		xafid = atoi(EARGF(usage()));
		break;
	case 'M':
		versioned = 1;
		msize = atoi(EARGF(usage()));
		break;
	case 'c':
		addr = netmkaddr(EARGF(usage()), "net", "9fs");
		if((fd = dial(addr, nil, nil, nil)) < 0)
			sysfatal("dial %s: %r", addr);
		dup(fd, 0);
		dup(fd, 1);
		if(fd > 1)
			close(fd);
		break;
	case 'n':
		noauth = 1;
		break;
	case 'v':
		verbose++;
		break;
	case 'u':
		isunix++;
		break;
	case 'l':
		logging++;
		break;
	}ARGEND

	if(attached && !versioned){
		fprint(2, "-A must be used with -M\n");
		usage();
	}

	if(argc != 1)
		usage();
	addr = argv[0];

	fmtinstall('T', timefmt);

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);
	if(logging){
		if(strncmp(addr, "unix!", 5) == 0)
			addr += 5;
		file = smprint("%s.log", addr);
		if(file == nil)
			sysfatal("smprint log: %r");
		if((fd = create(file, OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", file);
		dup(fd, 2);
		if(fd > 2)
			close(fd);
	}
	if(verbose) fprint(2, "%T 9pserve running\n");
	proccreate(mainproc, nil, STACK);
}

void
mainproc(void *v)
{
	int n, nn;
	Fcall f;
	USED(v);

	atnotify(ignorepipe, 1);
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);
	fmtinstall('F', fcallfmt);
	fmtinstall('H', encodefmt);

	outq = qalloc();
	inq = qalloc();

	if(!versioned){
		f.type = Tversion;
		f.version = "9P2000";
		f.msize = msize;
		f.tag = NOTAG;
		n = convS2M(&f, vbuf, sizeof vbuf);
		if(n <= BIT16SZ)
			sysfatal("convS2M conversion error");
		if(verbose > 1) fprint(2, "%T * <- %F\n", &f);
		nn = write(1, vbuf, n);
		if(n != nn)
			sysfatal("error writing Tversion: %r\n");
		n = read9pmsg(0, vbuf, sizeof vbuf);
		if(n < 0)
			sysfatal("read9pmsg failure");
		if(convM2S(vbuf, n, &f) != n)
			sysfatal("convM2S failure");
		if(f.msize < msize)
			msize = f.msize;
		if(verbose > 1) fprint(2, "%T * -> %F\n", &f);
	}

	threadcreate(inputthread, nil, STACK);
	threadcreate(outputthread, nil, STACK);

/*	if(rootfid) */
/*		dorootstat(); */

	threadcreate(listenthread, nil, STACK);
	threadexits(0);
}

int
ignorepipe(void *v, char *s)
{
	USED(v);
	if(strcmp(s, "sys: write on closed pipe") == 0)
		return 1;
	if(strcmp(s, "sys: tstp") == 0)
		return 1;
	if(strcmp(s, "sys: window size change") == 0)
		return 1;
	fprint(2, "9pserve %s: %T note: %s\n", addr, s);
	return 0;
}

void
listenthread(void *arg)
{
	Conn *c;
	Ioproc *io;

	io = ioproc();
	USED(arg);
	threadsetname("listen %s", adir);
	for(;;){
		c = emalloc(sizeof(Conn));
		c->fd = iolisten(io, adir, c->dir);
		if(c->fd < 0){
			if(verbose) fprint(2, "%T listen: %r\n");
			close(afd);
			free(c);
			return;
		}
		c->inc = chancreate(sizeof(void*), 0);
		c->internal = chancreate(sizeof(void*), 0);
		c->inq = qalloc();
		c->outq = qalloc();
		c->outqdead = chancreate(sizeof(void*), 0);
		if(verbose) fprint(2, "%T incoming call on %s\n", c->dir);
		threadcreate(connthread, c, STACK);
	}
}

void
send9pmsg(Msg *m)
{
	int n, nn;

	n = sizeS2M(&m->rx);
	m->rpkt = emalloc(n);
	nn = convS2M(&m->rx, m->rpkt, n);
	if(nn <= BIT16SZ)
		sysfatal("convS2M conversion error");
	if(nn != n)
		sysfatal("sizeS2M and convS2M disagree");
	sendq(m->c->outq, m);
}

void
sendomsg(Msg *m)
{
	int n, nn;

	n = sizeS2M(&m->tx);
	m->tpkt = emalloc(n);
	nn = convS2M(&m->tx, m->tpkt, n);
	if(nn <= BIT16SZ)
		sysfatal("convS2M conversion error");
	if(nn != n)
		sysfatal("sizeS2M and convS2M disagree");
	sendq(outq, m);
}

void
err(Msg *m, char *ename)
{
	m->rx.type = Rerror;
	m->rx.ename = ename;
	m->rx.tag = m->tx.tag;
	send9pmsg(m);
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

void
connthread(void *arg)
{
	int i, fd;
	Conn *c;
	Hash *h, *hnext;
	Msg *m, *om, *mm, sync;
	Fid *f;
	Ioproc *io;

	c = arg;
	threadsetname("conn %s", c->dir);
	io = ioproc();
	fd = ioaccept(io, c->fd, c->dir);
	if(fd < 0){
		if(verbose) fprint(2, "%T accept %s: %r\n", c->dir);
		goto out;
	}
	close(c->fd);
	c->fd = fd;
	threadcreate(connoutthread, c, STACK);
	while((m = mread9p(io, c->fd)) != nil){
		if(verbose > 1) fprint(2, "%T fd#%d -> %F\n", c->fd, &m->tx);
		m->c = c;
		m->ctag = m->tx.tag;
		c->nmsg++;
		if(verbose > 1) fprint(2, "%T fd#%d: new msg %p\n", c->fd, m);
		if(puthash(c->tag, m->tx.tag, m) < 0){
			err(m, "duplicate tag");
			continue;
		}
		msgincref(m);
		switch(m->tx.type){
		case Tversion:
			m->rx.tag = m->tx.tag;
			m->rx.msize = m->tx.msize;
			if(m->rx.msize > msize)
				m->rx.msize = msize;
			m->rx.version = "9P2000";
			m->rx.type = Rversion;
			send9pmsg(m);
			continue;
		case Tflush:
			if((m->oldm = gethash(c->tag, m->tx.oldtag)) == nil){
				m->rx.tag = m->tx.tag;
				m->rx.type = Rflush;
				send9pmsg(m);
				continue;
			}
			msgincref(m->oldm);
			break;
		case Tattach:
			m->afid = nil;
			if(m->tx.afid != NOFID
			&& (m->afid = gethash(c->fid, m->tx.afid)) == nil){
				err(m, "unknown fid");
				continue;
			}
			if(m->afid)
				m->afid->ref++;
			m->fid = fidnew(m->tx.fid);
			if(puthash(c->fid, m->tx.fid, m->fid) < 0){
				err(m, "duplicate fid");
				continue;
			}
			m->fid->ref++;
			if(attached && m->afid==nil){
				if(m->tx.aname[0] && strcmp(xaname, m->tx.aname) != 0){
					err(m, "invalid attach name");
					continue;
				}
				m->tx.afid = xafid;
				m->tx.aname = xaname;
				m->tx.uname = getuser();	/* what srv.c used */
				repack(&m->tx, &m->tpkt);
			}
			break;
		case Twalk:
			if((m->fid = gethash(c->fid, m->tx.fid)) == nil){
				err(m, "unknown fid");
				continue;
			}
			m->fid->ref++;
			if(m->tx.newfid == m->tx.fid){
				m->fid->ref++;
				m->newfid = m->fid;
			}else{
				m->newfid = fidnew(m->tx.newfid);
				if(puthash(c->fid, m->tx.newfid, m->newfid) < 0){
					err(m, "duplicate fid");
					continue;
				}
				m->newfid->ref++;
			}
			break;
		case Tauth:
			if(attached){
				err(m, "authentication not required");
				continue;
			}
			if(noauth){
				err(m, "authentication rejected");
				continue;
			}
			m->afid = fidnew(m->tx.afid);
			if(puthash(c->fid, m->tx.afid, m->afid) < 0){
				err(m, "duplicate fid");
				continue;
			}
			m->afid->ref++;
			break;
		case Tcreate:
			if(m->tx.perm&(DMSYMLINK|DMDEVICE|DMNAMEDPIPE|DMSOCKET)){
				err(m, "unsupported file type");
				continue;
			}
			goto caseTopen;
		case Topenfd:
			if(m->tx.mode&~(OTRUNC|3)){
				err(m, "bad openfd mode");
				continue;
			}
			m->isopenfd = 1;
			m->tx.type = Topen;
			m->tpkt[4] = Topen;
			/* fall through */
		caseTopen:
		case Topen:
		case Tclunk:
		case Tread:
		case Twrite:
		case Tremove:
		case Tstat:
		case Twstat:
			if((m->fid = gethash(c->fid, m->tx.fid)) == nil){
				err(m, "unknown fid");
				continue;
			}
			m->fid->ref++;
			break;
		}

		/* have everything - translate and send */
		m->c = c;
		m->ctag = m->tx.tag;
		m->tx.tag = m->tag;
		if(m->fid)
			m->tx.fid = m->fid->fid;
		if(m->newfid)
			m->tx.newfid = m->newfid->fid;
		if(m->afid)
			m->tx.afid = m->afid->fid;
		if(m->oldm)
			m->tx.oldtag = m->oldm->tag;
		/* reference passes to outq */
		sendq(outq, m);
		while(c->nmsg >= MAXMSG){
			c->inputstalled = 1;
			recvp(c->inc);
		}
	}

	if(verbose) fprint(2, "%T fd#%d eof; flushing conn\n", c->fd);

	/* flush all outstanding messages */
	for(i=0; i<NHASH; i++){
		while((h = c->tag[i]) != nil){
			om = h->v;
			msgincref(om); /* for us */
			m = msgnew(0);
			m->internal = 1;
			m->c = c;
			c->nmsg++;
			m->tx.type = Tflush;
			m->tx.tag = m->tag;
			m->tx.oldtag = om->tag;
			m->oldm = om;
			msgincref(om);
			msgincref(m);	/* for outq */
			sendomsg(m);
			mm = recvp(c->internal);
			assert(mm == m);
			msgput(m);	/* got from recvp */
			msgput(m);	/* got from msgnew */
			if(delhash(c->tag, om->ctag, om) == 0)
				msgput(om);	/* got from hash table */
			msgput(om);	/* got from msgincref */
		}
	}

	/*
	 * outputthread has written all its messages
	 * to the remote connection (because we've gotten all the replies!),
	 * but it might not have gotten a chance to msgput
	 * the very last one.  sync up to make sure.
	 */
	memset(&sync, 0, sizeof sync);
	sync.sync = 1;
	sync.c = c;
	sendq(outq, &sync);
	recvp(c->outqdead);

	/* everything is quiet; can close the local output queue. */
	sendq(c->outq, nil);
	recvp(c->outqdead);

	/* should be no messages left anywhere. */
	assert(c->nmsg == 0);

	/* clunk all outstanding fids */
	for(i=0; i<NHASH; i++){
		for(h=c->fid[i]; h; h=hnext){
			f = h->v;
			m = msgnew(0);
			m->internal = 1;
			m->c = c;
			c->nmsg++;
			m->tx.type = Tclunk;
			m->tx.tag = m->tag;
			m->tx.fid = f->fid;
			m->fid = f;
			f->ref++;
			msgincref(m);
			sendomsg(m);
			mm = recvp(c->internal);
			assert(mm == m);
			msgclear(m);
			msgput(m);	/* got from recvp */
			msgput(m);	/* got from msgnew */
			fidput(f);	/* got from hash table */
			hnext = h->next;
			free(h);
		}
	}

out:
	closeioproc(io);
	assert(c->nmsg == 0);
	assert(c->nfid == 0);
	close(c->fd);
	chanfree(c->internal);
	c->internal = 0;
	chanfree(c->inc);
	c->inc = 0;
	free(c->inq);
	c->inq = 0;
	free(c);
}

static void
openfdthread(void *v)
{
	Conn *c;
	Fid *fid;
	Msg *m;
	int n;
	vlong tot;
	Ioproc *io;
	char buf[1024];

	c = v;
	fid = c->fdfid;
	io = ioproc();
	threadsetname("openfd %s", c->fdfid);
	tot = 0;
	m = nil;
	if(c->fdmode == OREAD){
		for(;;){
			if(verbose) fprint(2, "%T tread...");
			m = msgnew(0);
			m->internal = 1;
			m->c = c;
			m->tx.type = Tread;
			m->tx.count = msize - IOHDRSZ;
			m->tx.fid = fid->fid;
			m->tx.tag = m->tag;
			m->tx.offset = tot;
			m->fid = fid;
			fid->ref++;
			msgincref(m);
			sendomsg(m);
			recvp(c->internal);
			if(m->rx.type == Rerror){
			/*	fprint(2, "%T read error: %s\n", m->rx.ename); */
				break;
			}
			if(m->rx.count == 0)
				break;
			tot += m->rx.count;
			if(iowrite(io, c->fd, m->rx.data, m->rx.count) != m->rx.count){
				/* fprint(2, "%T pipe write error: %r\n"); */
				break;
			}
			msgput(m);
			msgput(m);
			m = nil;
		}
	}else{
		for(;;){
			if(verbose) fprint(2, "%T twrite...");
			n = sizeof buf;
			if(n > msize)
				n = msize;
			if((n=ioread(io, c->fd, buf, n)) <= 0){
				if(n < 0)
					fprint(2, "%T pipe read error: %r\n");
				break;
			}
			m = msgnew(0);
			m->internal = 1;
			m->c = c;
			m->tx.type = Twrite;
			m->tx.fid = fid->fid;
			m->tx.data = buf;
			m->tx.count = n;
			m->tx.tag = m->tag;
			m->tx.offset = tot;
			m->fid = fid;
			fid->ref++;
			msgincref(m);
			sendomsg(m);
			recvp(c->internal);
			if(m->rx.type == Rerror){
			/*	fprint(2, "%T write error: %s\n", m->rx.ename); */
			}
			tot += n;
			msgput(m);
			msgput(m);
			m = nil;
		}
	}
	if(verbose) fprint(2, "%T eof on %d fid %d\n", c->fd, fid->fid);
	close(c->fd);
	closeioproc(io);
	if(m){
		msgput(m);
		msgput(m);
	}
	if(verbose) fprint(2, "%T eof on %d fid %d ref %d\n", c->fd, fid->fid, fid->ref);
	if(--fid->openfd == 0){
		m = msgnew(0);
		m->internal = 1;
		m->c = c;
		m->tx.type = Tclunk;
		m->tx.tag = m->tag;
		m->tx.fid = fid->fid;
		m->fid = fid;
		fid->ref++;
		msgincref(m);
		sendomsg(m);
		recvp(c->internal);
		msgput(m);
		msgput(m);
	}
	fidput(fid);
	c->fdfid = nil;
	chanfree(c->internal);
	c->internal = 0;
	free(c);
}

int
xopenfd(Msg *m)
{
	char errs[ERRMAX];
	int n, p[2];
	Conn *nc;

	if(pipe(p) < 0){
		rerrstr(errs, sizeof errs);
		err(m, errs);
		/* XXX return here? */
	}
	if(verbose) fprint(2, "%T xopen pipe %d %d...", p[0], p[1]);

	/* now we're committed. */

	/* a new connection for this fid */
	nc = emalloc(sizeof(Conn));
	nc->internal = chancreate(sizeof(void*), 0);

	/* a ref for us */
	nc->fdfid = m->fid;
	m->fid->ref++;
	nc->fdfid->openfd++;
	nc->fdmode = m->tx.mode;
	nc->fd = p[0];

	/* a thread to tend the pipe */
	threadcreate(openfdthread, nc, STACK);

	/* if mode is ORDWR, that openfdthread will write; start a reader */
	if((m->tx.mode&3) == ORDWR){
		nc = emalloc(sizeof(Conn));
		nc->internal = chancreate(sizeof(void*), 0);
		nc->fdfid = m->fid;
		m->fid->ref++;
		nc->fdfid->openfd++;
		nc->fdmode = OREAD;
		nc->fd = dup(p[0], -1);
		threadcreate(openfdthread, nc, STACK);
	}

	/* steal fid from other connection */
	if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
		fidput(m->fid);

	/* rewrite as Ropenfd */
	m->rx.type = Ropenfd;
	n = GBIT32(m->rpkt);
	m->rpkt = erealloc(m->rpkt, n+4);
	PBIT32(m->rpkt+n, p[1]);
	n += 4;
	PBIT32(m->rpkt, n);
	m->rpkt[4] = Ropenfd;
	m->rx.unixfd = p[1];
	return 0;
}

void
connoutthread(void *arg)
{
	int err;
	Conn *c;
	Msg *m, *om;
	Ioproc *io;

	c = arg;
	io = ioproc();
	threadsetname("connout %s", c->dir);
	while((m = recvq(c->outq)) != nil){
		err = m->tx.type+1 != m->rx.type;
		if(!err && m->isopenfd)
			if(xopenfd(m) < 0)
				continue;
		switch(m->tx.type){
		case Tflush:
			om = m->oldm;
			if(om)
				if(delhash(om->c->tag, om->ctag, om) == 0)
					msgput(om);
			break;
		case Tclunk:
		case Tremove:
			if(m->fid)
				if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
					fidput(m->fid);
			break;
		case Tauth:
			if(err && m->afid){
				if(verbose) fprint(2, "%T auth error\n");
				if(delhash(m->c->fid, m->afid->cfid, m->afid) == 0)
					fidput(m->afid);
			}
			break;
		case Tattach:
			if(err && m->fid)
				if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
					fidput(m->fid);
			break;
		case Twalk:
			if(err || m->rx.nwqid < m->tx.nwname)
			if(m->tx.fid != m->tx.newfid && m->newfid)
				if(delhash(m->c->fid, m->newfid->cfid, m->newfid) == 0)
					fidput(m->newfid);
			break;
		case Tread:
			break;
		case Tstat:
			break;
		case Topen:
		case Tcreate:
			m->fid->isdir = (m->rx.qid.type & QTDIR);
			break;
		}
		if(delhash(m->c->tag, m->ctag, m) == 0)
			msgput(m);
		if(verbose > 1) fprint(2, "%T fd#%d <- %F\n", c->fd, &m->rx);
		rewritehdr(&m->rx, m->rpkt);
		if(mwrite9p(io, c->fd, m->rpkt) < 0)
			if(verbose) fprint(2, "%T write error: %r\n");
		msgput(m);
		if(c->inputstalled && c->nmsg < MAXMSG)
			nbsendp(c->inc, 0);
	}
	closeioproc(io);
	free(c->outq);
	c->outq = nil;
	sendp(c->outqdead, nil);
}

void
outputthread(void *arg)
{
	Msg *m;
	Ioproc *io;

	USED(arg);
	io = ioproc();
	threadsetname("output");
	while((m = recvq(outq)) != nil){
		if(m->sync){
			sendp(m->c->outqdead, nil);
			continue;
		}
		if(verbose > 1) fprint(2, "%T * <- %F\n", &m->tx);
		rewritehdr(&m->tx, m->tpkt);
		if(mwrite9p(io, 1, m->tpkt) < 0)
			sysfatal("output error: %r");
		msgput(m);
	}
	closeioproc(io);
	fprint(2, "%T output eof\n");
	threadexitsall(0);
}

void
inputthread(void *arg)
{
	uchar *pkt;
	int n, nn, tag;
	Msg *m;
	Ioproc *io;

	threadsetname("input");
	if(verbose) fprint(2, "%T input thread\n");
	io = ioproc();
	USED(arg);
	while((pkt = read9ppkt(io, 0)) != nil){
		n = GBIT32(pkt);
		if(n < 7){
			fprint(2, "%T short 9P packet from server\n");
			free(pkt);
			continue;
		}
		if(verbose > 2) fprint(2, "%T read %.*H\n", n, pkt);
		tag = GBIT16(pkt+5);
		if((m = msgget(tag)) == nil){
			fprint(2, "%T unexpected 9P response tag %d\n", tag);
			free(pkt);
			continue;
		}
		if((nn = convM2S(pkt, n, &m->rx)) != n){
			fprint(2, "%T bad packet - convM2S %d but %d\n", nn, n);
			free(pkt);
			msgput(m);
			continue;
		}
		if(verbose > 1) fprint(2, "%T * -> %F%s\n", &m->rx,
			m->internal ? " (internal)" : "");
		m->rpkt = pkt;
		m->rx.tag = m->ctag;
		if(m->internal)
			sendp(m->c->internal, m);
		else if(m->c->outq)
			sendq(m->c->outq, m);
		else
			msgput(m);
	}
	closeioproc(io);
	/*fprint(2, "%T input eof\n"); */
	threadexitsall(0);
}

void*
gethash(Hash **ht, uint n)
{
	Hash *h;

	for(h=ht[n%NHASH]; h; h=h->next)
		if(h->n == n)
			return h->v;
	return nil;
}

int
delhash(Hash **ht, uint n, void *v)
{
	Hash *h, **l;

	for(l=&ht[n%NHASH]; h=*l; l=&h->next)
		if(h->n == n){
			if(h->v != v){
				if(verbose) fprint(2, "%T delhash %d got %p want %p\n", n, h->v, v);
				return -1;
			}
			*l = h->next;
			free(h);
			return 0;
		}
	return -1;
}

int
puthash(Hash **ht, uint n, void *v)
{
	Hash *h;

	if(gethash(ht, n))
		return -1;
	h = emalloc(sizeof(Hash));
	h->next = ht[n%NHASH];
	h->n = n;
	h->v = v;
	ht[n%NHASH] = h;
	return 0;
}

Fid **fidtab;
int nfidtab;
Fid *freefid;

Fid*
fidnew(int cfid)
{
	Fid *f;

	if(freefid == nil){
		fidtab = erealloc(fidtab, (nfidtab+1)*sizeof(fidtab[0]));
		if(nfidtab == xafid){
			fidtab[nfidtab++] = nil;
			fidtab = erealloc(fidtab, (nfidtab+1)*sizeof(fidtab[0]));
		}
		fidtab[nfidtab] = emalloc(sizeof(Fid));
		freefid = fidtab[nfidtab];
		freefid->fid = nfidtab++;
	}
	f = freefid;
	freefid = f->next;
	f->cfid = cfid;
	f->ref = 1;
	f->offset = 0;
	f->coffset = 0;
	f->isdir = -1;
	return f;
}

void
fidput(Fid *f)
{
	if(f == nil)
		return;
	assert(f->ref > 0);
	if(--f->ref > 0)
		return;
	f->next = freefid;
	f->cfid = -1;
	freefid = f;
}

Msg **msgtab;
int nmsgtab;
int nmsg;
Msg *freemsg;

void
msgincref(Msg *m)
{
	if(verbose > 1) fprint(2, "%T msgincref @0x%lux %p tag %d/%d ref %d=>%d\n",
		getcallerpc(&m), m, m->tag, m->ctag, m->ref, m->ref+1);
	m->ref++;
}

Msg*
msgnew(int x)
{
	Msg *m;

	if(freemsg == nil){
		msgtab = erealloc(msgtab, (nmsgtab+1)*sizeof(msgtab[0]));
		msgtab[nmsgtab] = emalloc(sizeof(Msg));
		freemsg = msgtab[nmsgtab];
		freemsg->tag = nmsgtab++;
	}
	m = freemsg;
	freemsg = m->next;
	m->ref = 1;
	if(verbose > 1) fprint(2, "%T msgnew @0x%lux %p tag %d ref %d\n",
		getcallerpc(&x), m, m->tag, m->ref);
	nmsg++;
	return m;
}

/*
 * Clear data associated with connections, so that
 * if all msgs have been msgcleared, the connection
 * can be freed.  Note that this does *not* free the tpkt
 * and rpkt; they are freed in msgput with the msg itself.
 * The io write thread might still be holding a ref to msg
 * even once the connection has finished with it.
 */
void
msgclear(Msg *m)
{
	if(m->c){
		m->c->nmsg--;
		m->c = nil;
	}
	if(m->oldm){
		msgput(m->oldm);
		m->oldm = nil;
	}
	if(m->fid){
		fidput(m->fid);
		m->fid = nil;
	}
	if(m->afid){
		fidput(m->afid);
		m->afid = nil;
	}
	if(m->newfid){
		fidput(m->newfid);
		m->newfid = nil;
	}
	if(m->rx.type == Ropenfd && m->rx.unixfd >= 0){
		close(m->rx.unixfd);
		m->rx.unixfd = -1;
	}
}

void
msgput(Msg *m)
{
	if(m == nil)
		return;

	if(verbose > 1) fprint(2, "%T msgput 0x%lux %p tag %d/%d ref %d\n",
		getcallerpc(&m), m, m->tag, m->ctag, m->ref);
	assert(m->ref > 0);
	if(--m->ref > 0)
		return;
	nmsg--;
	msgclear(m);
	if(m->tpkt){
		free(m->tpkt);
		m->tpkt = nil;
	}
	if(m->rpkt){
		free(m->rpkt);
		m->rpkt = nil;
	}
	m->isopenfd = 0;
	m->internal = 0;
	m->next = freemsg;
	freemsg = m;
}

Msg*
msgget(int n)
{
	Msg *m;

	if(n < 0 || n >= nmsgtab)
		return nil;
	m = msgtab[n];
	if(m->ref == 0)
		return nil;
	if(verbose) fprint(2, "%T msgget %d = %p\n", n, m);
	msgincref(m);
	return m;
}


void*
emalloc(int n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil){
		abort();
		sysfatal("out of memory allocating %d", n);
	}
	return v;
}

void*
erealloc(void *v, int n)
{
	v = realloc(v, n);
	if(v == nil){
		abort();
		sysfatal("out of memory reallocating %d", n);
	}
	return v;
}

typedef struct Qel Qel;
struct Qel
{
	Qel *next;
	void *p;
};

struct Queue
{
	QLock lk;
	Rendez r;
	Qel *head;
	Qel *tail;
};

Queue*
qalloc(void)
{
	Queue *q;

	q = mallocz(sizeof(Queue), 1);
	if(q == nil)
		return nil;
	q->r.l = &q->lk;
	return q;
}

int
sendq(Queue *q, void *p)
{
	Qel *e;

	e = emalloc(sizeof(Qel));
	qlock(&q->lk);
	e->p = p;
	e->next = nil;
	if(q->head == nil)
		q->head = e;
	else
		q->tail->next = e;
	q->tail = e;
	rwakeup(&q->r);
	qunlock(&q->lk);
	return 0;
}

void*
recvq(Queue *q)
{
	void *p;
	Qel *e;

	qlock(&q->lk);
	while(q->head == nil)
		rsleep(&q->r);
	e = q->head;
	q->head = e->next;
	qunlock(&q->lk);
	p = e->p;
	free(e);
	return p;
}

uchar*
read9ppkt(Ioproc *io, int fd)
{
	uchar buf[4], *pkt;
	int n, nn;

	n = ioreadn(io, fd, buf, 4);
	if(n != 4)
		return nil;
	n = GBIT32(buf);
	if(n > MAXMSGSIZE)
		return nil;
	pkt = emalloc(n);
	PBIT32(pkt, n);
	nn = ioreadn(io, fd, pkt+4, n-4);
	if(nn != n-4){
		free(pkt);
		return nil;
	}
/* would do this if we ever got one of these, but we only generate them
	if(pkt[4] == Ropenfd){
		newfd = iorecvfd(io, fd);
		PBIT32(pkt+n-4, newfd);
	}
*/
	return pkt;
}

Msg*
mread9p(Ioproc *io, int fd)
{
	int n, nn;
	uchar *pkt;
	Msg *m;

	if((pkt = read9ppkt(io, fd)) == nil)
		return nil;

	m = msgnew(0);
	m->tpkt = pkt;
	n = GBIT32(pkt);
	nn = convM2S(pkt, n, &m->tx);
	if(nn != n){
		fprint(2, "%T read bad packet from %d\n", fd);
		free(m->tpkt);
		free(m);
		return nil;
	}
	return m;
}

int
mwrite9p(Ioproc *io, int fd, uchar *pkt)
{
	int n, nfd;

	n = GBIT32(pkt);
	if(verbose > 2) fprint(2, "%T write %d %d %.*H\n", fd, n, n, pkt);
if(verbose > 1) fprint(2, "%T before iowrite\n");
	if(iowrite(io, fd, pkt, n) != n){
		fprint(2, "%T write error: %r\n");
		return -1;
	}
if(verbose > 1) fprint(2, "%T after iowrite\n");
	if(pkt[4] == Ropenfd){
		nfd = GBIT32(pkt+n-4);
		if(iosendfd(io, fd, nfd) < 0){
			fprint(2, "%T send fd error: %r\n");
			return -1;
		}
	}
	return 0;
}

void
restring(uchar *pkt, int pn, char *s)
{
	int n;

	if(s < (char*)pkt || s >= (char*)pkt+pn)
		return;

	n = strlen(s);
	memmove(s+1, s, n);
	PBIT16((uchar*)s-1, n);
}

void
repack(Fcall *f, uchar **ppkt)
{
	uint n, nn;
	uchar *pkt;

	pkt = *ppkt;
	n = GBIT32(pkt);
	nn = sizeS2M(f);
	if(nn > n){
		free(pkt);
		pkt = emalloc(nn);
		*ppkt = pkt;
	}
	n = convS2M(f, pkt, nn);
	if(n <= BIT16SZ)
		sysfatal("convS2M conversion error");
	if(n != nn)
		sysfatal("convS2M and sizeS2M disagree");
}

void
rewritehdr(Fcall *f, uchar *pkt)
{
	int i, n;

	n = GBIT32(pkt);
	PBIT16(pkt+5, f->tag);
	switch(f->type){
	case Tversion:
	case Rversion:
		restring(pkt, n, f->version);
		break;
	case Tauth:
		PBIT32(pkt+7, f->afid);
		restring(pkt, n, f->uname);
		restring(pkt, n, f->aname);
		break;
	case Tflush:
		PBIT16(pkt+7, f->oldtag);
		break;
	case Tattach:
		restring(pkt, n, f->uname);
		restring(pkt, n, f->aname);
		PBIT32(pkt+7, f->fid);
		PBIT32(pkt+11, f->afid);
		break;
	case Twalk:
		PBIT32(pkt+7, f->fid);
		PBIT32(pkt+11, f->newfid);
		for(i=0; i<f->nwname; i++)
			restring(pkt, n, f->wname[i]);
		break;
	case Tcreate:
		restring(pkt, n, f->name);
		/* fall through */
	case Topen:
	case Tclunk:
	case Tremove:
	case Tstat:
	case Twstat:
	case Twrite:
		PBIT32(pkt+7, f->fid);
		break;
	case Tread:
		PBIT32(pkt+7, f->fid);
		PBIT64(pkt+11, f->offset);
		break;
	case Rerror:
		restring(pkt, n, f->ename);
		break;
	}
}

static long
_iolisten(va_list *arg)
{
	char *a, *b;

	a = va_arg(*arg, char*);
	b = va_arg(*arg, char*);
	return listen(a, b);
}

int
iolisten(Ioproc *io, char *a, char *b)
{
	return iocall(io, _iolisten, a, b);
}

static long
_ioaccept(va_list *arg)
{
	int fd;
	char *dir;

	fd = va_arg(*arg, int);
	dir = va_arg(*arg, char*);
	return accept(fd, dir);
}

int
ioaccept(Ioproc *io, int fd, char *dir)
{
	return iocall(io, _ioaccept, fd, dir);
}

int
timefmt(Fmt *fmt)
{
	static char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	vlong ns;
	Tm tm;
	ns = nsec();
	tm = *localtime(time(0));
	return fmtprint(fmt, "%s %2d %02d:%02d:%02d.%03d",
		mon[tm.mon], tm.mday, tm.hour, tm.min, tm.sec,
		(int)(ns%1000000000)/1000000);
}
