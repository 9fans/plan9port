#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <errno.h>

enum
{
	STACK = 32768,
	NHASH = 31,
	MAXMSG = 64,	/* per connection */
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
	Fid *next;
};

struct Msg
{
	Conn *c;
	int internal;
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
};

char *addr;
int afd;
char adir[40];
int isunix;
Queue *outq;
Queue *inq;
int verbose = 0;
int msize = 8192;

void *gethash(Hash**, uint);
int puthash(Hash**, uint, void*);
int delhash(Hash**, uint, void*);
Msg *mread9p(Ioproc*, int);
int mwrite9p(Ioproc*, int, uchar*);
uchar *read9ppkt(Ioproc*, int);
int write9ppkt(int, uchar*);
Msg *msgnew(void);
void msgput(Msg*);
Msg *msgget(int);
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
int tlisten(char*, char*);
int taccept(int, char*);
int iolisten(Ioproc*, char*, char*);
int ioaccept(Ioproc*, int, char*);
int iorecvfd(Ioproc*, int);
int iosendfd(Ioproc*, int, int);
void mainproc(void*);
int ignorepipe(void*, char*);

void
usage(void)
{
	fprint(2, "usage: 9pserve [-s service] [-u] address\n");
	fprint(2, "\treads/writes 9P messages on stdin/stdout\n");
	exits("usage");
}

uchar vbuf[128];
extern int _threaddebuglevel;
void
threadmain(int argc, char **argv)
{
	char *file;

	ARGBEGIN{
	default:
		usage();
	case 'v':
		verbose++;
		break;
	case 's':
		close(0);
		if(open(file=EARGF(usage()), ORDWR) != 0)
			sysfatal("open %s: %r", file);
		dup(0, 1);
		break;
	case 'u':
		isunix = 1;
		break;
	}ARGEND

	if(verbose) fprint(2, "9pserve running\n");
	if(argc != 1)
		usage();
	addr = argv[0];

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);

	threaddaemonize();
	mainproc(nil);
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

	f.type = Tversion;
	f.version = "9P2000";
	f.msize = msize;
	f.tag = NOTAG;
	n = convS2M(&f, vbuf, sizeof vbuf);
	if(verbose > 1) fprint(2, "* <- %F\n", &f);
	nn = write(1, vbuf, n);
	if(n != nn)
		sysfatal("error writing Tversion: %r\n");
	n = read9pmsg(0, vbuf, sizeof vbuf);
	if(convM2S(vbuf, n, &f) != n)
		sysfatal("convM2S failure");
	if(f.msize < msize)
		msize = f.msize;
	if(verbose > 1) fprint(2, "* -> %F\n", &f);

	threadcreate(inputthread, nil, STACK);
	threadcreate(outputthread, nil, STACK);
	threadcreate(listenthread, nil, STACK);
	threadexits(0);
}

int
ignorepipe(void *v, char *s)
{
	USED(v);
	if(strcmp(s, "sys: write on closed pipe") == 0)
		return 1;
	fprint(2, "msg: %s\n", s);
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
			if(verbose) fprint(2, "listen: %r\n");
			close(afd);
			free(c);
			return;
		}
		c->inc = chancreate(sizeof(void*), 0);
		c->internal = chancreate(sizeof(void*), 0);
		c->inq = qalloc();
		c->outq = qalloc();
		if(verbose) fprint(2, "incoming call on %s\n", c->dir);
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
	if(nn != n)
		sysfatal("sizeS2M + convS2M disagree");
	sendq(m->c->outq, m);
}

void
sendomsg(Msg *m)
{
	int n, nn;

	n = sizeS2M(&m->tx);
	m->tpkt = emalloc(n);
	nn = convS2M(&m->tx, m->tpkt, n);
	if(nn != n)
		sysfatal("sizeS2M + convS2M disagree");
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

void
connthread(void *arg)
{
	int i, fd;
	Conn *c;
	Hash *h, *hnext;
	Msg *m, *om, *mm;
	Fid *f;
	Ioproc *io;

	c = arg;
	threadsetname("conn %s", c->dir);
	io = ioproc();
	fd = ioaccept(io, c->fd, c->dir);
	if(fd < 0){
		if(verbose) fprint(2, "accept %s: %r\n", c->dir);
		goto out;
	}
	close(c->fd);
	c->fd = fd;
	threadcreate(connoutthread, c, STACK);
	while((m = mread9p(io, c->fd)) != nil){
		if(verbose > 1) fprint(2, "fd#%d -> %F\n", c->fd, &m->tx);
		m->c = c;
		m->ctag = m->tx.tag;
		c->nmsg++;
		if(puthash(c->tag, m->tx.tag, m) < 0){
			err(m, "duplicate tag");
			continue;
		}
		m->ref++;
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
			m->oldm->ref++;
			break;
		case Tattach:
			m->afid = nil;
			if(m->tx.afid != NOFID
			&& (m->afid = gethash(c->fid, m->tx.afid)) == nil){
				err(m, "unknown fid");
				continue;
			}
			m->fid = fidnew(m->tx.fid);
			if(puthash(c->fid, m->tx.fid, m->fid) < 0){
				err(m, "duplicate fid");
				continue;
			}
			m->fid->ref++;
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
			m->afid = fidnew(m->tx.afid);
			if(puthash(c->fid, m->tx.afid, m->afid) < 0){
				err(m, "duplicate fid");
				continue;
			}
			m->afid->ref++;
			break;
		case Topenfd:
			if(m->tx.mode&~(OTRUNC|3)){
				err(m, "bad openfd mode");
				continue;
			}
			m->isopenfd = 1;
			m->tx.type = Topen;
			m->tpkt[4] = Topen;
			/* fall through */
		case Tcreate:
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

	if(verbose) fprint(2, "fd#%d eof; flushing conn\n", c->fd);

	/* flush the output queue */
	sendq(c->outq, nil);
	while(c->outq != nil)
		yield();

	/* flush all outstanding messages */
	for(i=0; i<NHASH; i++){
		for(h=c->tag[i]; h; h=hnext){
			om = h->v;
			m = msgnew();
			m->internal = 1;
			m->c = c;
			c->nmsg++;
			m->tx.type = Tflush;
			m->tx.tag = m->tag;
			m->tx.oldtag = om->tag;
			m->oldm = om;
			om->ref++;	/* for m->oldm */
			m->ref++;	/* for outq */
			sendomsg(m);
			mm = recvp(c->internal);
			assert(mm == m);
			msgput(m);	/* got from recvp */
			msgput(m);	/* got from msgnew */
			msgput(om);	/* got from hash table */
			hnext = h->next;
			free(h);
		}
	}

	/* clunk all outstanding fids */
	for(i=0; i<NHASH; i++){
		for(h=c->fid[i]; h; h=hnext){
			f = h->v;
			m = msgnew();
			m->internal = 1;
			m->c = c;
			c->nmsg++;
			m->tx.type = Tclunk;
			m->tx.tag = m->tag;
			m->tx.fid = f->fid;
			m->fid = f;
			f->ref++;
			m->ref++;
			sendomsg(m);
			mm = recvp(c->internal);
			assert(mm == m);
			msgput(m);	/* got from recvp */
			msgput(m);	/* got from msgnew */
			fidput(f);	/* got from hash table */
			hnext = h->next;
			free(h);
		}
	}

out:
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
			if(verbose) fprint(2, "tread...");
			m = msgnew();
			m->internal = 1;
			m->c = c;
			m->tx.type = Tread;
			m->tx.count = msize - IOHDRSZ;
			m->tx.fid = fid->fid;
			m->tx.tag = m->tag;
			m->tx.offset = tot;
			m->fid = fid;
			fid->ref++;
			m->ref++;
			sendomsg(m);
			recvp(c->internal);
			if(m->rx.type == Rerror){
			//	fprint(2, "read error: %s\n", m->rx.ename);
				break;
			}
			if(m->rx.count == 0)
				break;
			tot += m->rx.count;
			if(iowrite(io, c->fd, m->rx.data, m->rx.count) != m->rx.count){
				// fprint(2, "pipe write error: %r\n");
				break;
			}
			msgput(m);
			msgput(m);
			m = nil;
		}
	}else{
		for(;;){
			if(verbose) fprint(2, "twrite...");
			n = sizeof buf;
			if(n > msize)
				n = msize;
			if((n=ioread(io, c->fd, buf, n)) <= 0){
				if(n < 0)
					fprint(2, "pipe read error: %r\n");
				break;
			}
			m = msgnew();
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
			m->ref++;
			sendomsg(m);
			recvp(c->internal);
			if(m->rx.type == Rerror){
			//	fprint(2, "write error: %s\n", m->rx.ename);
			}
			tot += n;
			msgput(m);
			msgput(m);
			m = nil;
		}
	}
	if(verbose) fprint(2, "eof on %d fid %d\n", c->fd, fid->fid);
	close(c->fd);
	closeioproc(io);
	if(m){
		msgput(m);
		msgput(m);
	}
	if(fid->ref == 1){
		m = msgnew();
		m->internal = 1;
		m->c = c;
		m->tx.type = Tclunk;
		m->tx.tag = m->tag;
		m->tx.fid = fid->fid;
		m->fid = fid;
		fid->ref++;
		m->ref++;
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
	}
	if(verbose) fprint(2, "xopen pipe %d %d...", p[0], p[1]);

	/* now we're committed. */

	/* a new connection for this fid */
	nc = emalloc(sizeof(Conn));
	nc->internal = chancreate(sizeof(void*), 0);

	/* a ref for us */
	nc->fdfid = m->fid;
	m->fid->ref++;
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
	Queue *outq;
	Msg *m, *om;
	Ioproc *io;

	c = arg;
	outq = c->outq;
	io = ioproc();
	threadsetname("connout %s", c->dir);
	while((m = recvq(outq)) != nil){
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
				fprint(2, "auth error\n");
				if(delhash(m->c->fid, m->afid->cfid, m->afid) == 0)
					fidput(m->fid);
			}
			break;
		case Tattach:
			if(err && m->fid)
				if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
					fidput(m->fid);
			break;
		case Twalk:
			if(err && m->tx.fid != m->tx.newfid && m->newfid)
				if(delhash(m->c->fid, m->newfid->cfid, m->newfid) == 0)
					fidput(m->newfid);
			break;
		}
		if(delhash(m->c->tag, m->ctag, m) == 0)
			msgput(m);
		if(verbose > 1) fprint(2, "fd#%d <- %F\n", c->fd, &m->rx);
		rewritehdr(&m->rx, m->rpkt);
		if(mwrite9p(io, c->fd, m->rpkt) < 0)
			if(verbose) fprint(2, "write error: %r\n");
		msgput(m);
		if(c->inputstalled && c->nmsg < MAXMSG)
			nbsendp(c->inc, 0);
	}
	closeioproc(io);
	free(outq);
	c->outq = nil;
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
		if(verbose > 1) fprint(2, "* <- %F\n", &m->tx);
		rewritehdr(&m->tx, m->tpkt);
		if(mwrite9p(io, 1, m->tpkt) < 0)
			sysfatal("output error: %r");
		msgput(m);
	}
	closeioproc(io);
	fprint(2, "output eof\n");
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
	if(verbose) fprint(2, "input thread\n");
	io = ioproc();
	USED(arg);
	while((pkt = read9ppkt(io, 0)) != nil){
		n = GBIT32(pkt);
		if(n < 7){
			fprint(2, "short 9P packet from server\n");
			free(pkt);
			continue;
		}
		if(verbose > 2) fprint(2, "read %.*H\n", n, pkt);
		tag = GBIT16(pkt+5);
		if((m = msgget(tag)) == nil){
			fprint(2, "unexpected 9P response tag %d\n", tag);
			free(pkt);
			continue;
		}
		if((nn = convM2S(pkt, n, &m->rx)) != n){
			fprint(2, "bad packet - convM2S %d but %d\n", nn, n);
			free(pkt);
			msgput(m);
			continue;
		}
		if(verbose > 1) fprint(2, "* -> %F%s\n", &m->rx,
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
	//fprint(2, "input eof\n");
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
				if(verbose) fprint(2, "delhash %d got %p want %p\n", n, h->v, v);
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
		fidtab[nfidtab] = emalloc(sizeof(Fid));
		freefid = fidtab[nfidtab];
		freefid->fid = nfidtab++;
	}
	f = freefid;
	freefid = f->next;
	f->cfid = cfid;
	f->ref = 1;
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
Msg *freemsg;

Msg*
msgnew(void)
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
	return m;
}

void
msgput(Msg *m)
{
	if(m == nil)
		return;

	if(verbose > 2) fprint(2, "msgput tag %d/%d ref %d\n", m->tag, m->ctag, m->ref);
	assert(m->ref > 0);
	if(--m->ref > 0)
		return;
	m->c->nmsg--;
	m->c = nil;
	msgput(m->oldm);
	m->oldm = nil;
	fidput(m->fid);
	m->fid = nil;
	fidput(m->afid);
	m->afid = nil;
	fidput(m->newfid);
	m->newfid = nil;
	free(m->tpkt);
	m->tpkt = nil;
	free(m->rpkt);
	m->rpkt = nil;
	if(m->rx.type == Ropenfd)
		close(m->rx.unixfd);
	m->rx.unixfd = -1;
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
	if(verbose) fprint(2, "msgget %d = %p\n", n, m);
	m->ref++;
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
	int hungup;
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
	if(q->hungup){
		werrstr("hungup queue");
		qunlock(&q->lk);
		return -1;
	}
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
	while(q->head == nil && !q->hungup)
		rsleep(&q->r);
	if(q->hungup){
		qunlock(&q->lk);
		return nil;
	}
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

	m = msgnew();
	m->tpkt = pkt;
	n = GBIT32(pkt);
	nn = convM2S(pkt, n, &m->tx);
	if(nn != n){
		fprint(2, "read bad packet from %d\n", fd);
		return nil;
	}
	return m;
}

int
mwrite9p(Ioproc *io, int fd, uchar *pkt)
{
	int n, nfd;

	n = GBIT32(pkt);
	if(verbose > 2) fprint(2, "write %d %d %.*H\n", fd, n, n, pkt);
	if(iowrite(io, fd, pkt, n) != n){
		fprint(2, "write error: %r\n");
		return -1;
	}
	if(pkt[4] == Ropenfd){
		nfd = GBIT32(pkt+n-4);
		if(iosendfd(io, fd, nfd) < 0){
			fprint(2, "send fd error: %r\n");
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
	case Tread:
	case Twrite:
	case Tclunk:
	case Tremove:
	case Tstat:
	case Twstat:
		PBIT32(pkt+7, f->fid);
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
