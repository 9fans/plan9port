#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>

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
int verbose;

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
void selectthread(void*);
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

void
usage(void)
{
	fprint(2, "usage: 9pserve [-s service] [-u] address\n");
	fprint(2, "\treads/writes 9P messages on stdin/stdout\n");
	exits("usage");
}

uchar vbuf[128];

void
threadmain(int argc, char **argv)
{
	char *file;
	int n;
	Fcall f;

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

	if(argc != 1)
		usage();
	addr = argv[0];

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);

	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);
	fmtinstall('F', fcallfmt);
	fmtinstall('H', encodefmt);

	outq = qalloc();
	inq = qalloc();

//	threadcreateidle(selectthread, nil, STACK);
	threadcreate(inputthread, nil, STACK);
	threadcreate(outputthread, nil, STACK);

	f.type = Tversion;
	f.version = "9P2000";
	f.msize = 8192;
	f.tag = NOTAG;
	n = convS2M(&f, vbuf, sizeof vbuf);
	if(verbose > 1) fprint(2, "* <- %F\n", &f);
	write(1, vbuf, n);
	n = read9pmsg(0, vbuf, sizeof vbuf);
	if(convM2S(vbuf, n, &f) != n)
		sysfatal("convM2S failure");
	if(verbose > 1) fprint(2, "* -> %F\n", &f);
	threadcreate(listenthread, nil, STACK);
}

void
listenthread(void *arg)
{
	Conn *c;
	Ioproc *io;

	io = ioproc();
	USED(arg);
	for(;;){
		c = emalloc(sizeof(Conn));
		c->inc = chancreate(sizeof(void*), 0);
		c->internal = chancreate(sizeof(void*), 0);
		c->inq = qalloc();
		c->outq = qalloc();
		c->fd = iolisten(io, adir, c->dir);
		if(c->fd < 0){
			if(verbose) fprint(2, "listen: %r\n");
			close(afd);
			free(c);
			return;
		}
		if(verbose) fprint(2, "incoming call on %s\n", c->dir);
		threadcreate(connthread, c, STACK);
	}	
}

void
sendmsg(Msg *m)
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
	sendmsg(m);
}

void
connthread(void *arg)
{
	int i, fd;
	Conn *c;
	Hash *h;
	Msg *m, *om;
	Fid *f;
	Ioproc *io;

	c = arg;
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
		if(verbose > 1) fprint(2, "%s -> %F\n", c->dir, &m->tx);
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
			if(m->rx.msize > 8192)
				m->rx.msize = 8192;
			m->rx.version = "9P2000";
			m->rx.type = Rversion;
			sendmsg(m);
			continue;
		case Tflush:
			if((m->oldm = gethash(c->tag, m->tx.oldtag)) == nil){
				m->rx.tag = m->tx.tag;
				m->rx.type = Rflush;
				sendmsg(m);
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

	if(verbose) fprint(2, "%s eof\n", c->dir);

	/* flush all outstanding messages */
	for(i=0; i<NHASH; i++){
		for(h=c->tag[i]; h; h=h->next){
			om = h->v;
			m = msgnew();
			m->internal = 1;
			m->c = c;
			m->tx.type = Tflush;
			m->tx.tag = m->tag;
			m->tx.oldtag = om->tag;
			m->oldm = om;
			om->ref++;
			m->ref++;	/* for outq */
			sendomsg(m);
			recvp(c->internal);
			msgput(m);
		}
	}

	/* clunk all outstanding fids */
	for(i=0; i<NHASH; i++){
		for(h=c->fid[i]; h; h=h->next){
			f = h->v;
			m = msgnew();
			m->internal = 1;
			m->c = c;
			m->tx.type = Tclunk;
			m->tx.tag = m->tag;
			m->tx.fid = f->fid;
			m->fid = f;
			f->ref++;
			m->ref++;
			sendomsg(m);
			recvp(c->internal);
			msgput(m);
		}
	}

out:
	assert(c->nmsg == 0);
	assert(c->nfid == 0);
	close(c->fd);
	free(c);
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
	while((m = recvq(c->outq)) != nil){
		err = m->tx.type+1 != m->rx.type;
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
		if(verbose > 1) fprint(2, "%s <- %F\n", c->dir, &m->rx);
		rewritehdr(&m->rx, m->rpkt);
		if(mwrite9p(io, c->fd, m->rpkt) < 0)
			if(verbose) fprint(2, "write error: %r\n");
		msgput(m);
		if(c->inputstalled && c->nmsg < MAXMSG)
			nbsendp(c->inc, 0);
	}
	closeioproc(io);
}

void
outputthread(void *arg)
{
	Msg *m;
	Ioproc *io;

	USED(arg);
	io = ioproc();
	while((m = recvq(outq)) != nil){
		if(verbose > 1) fprint(2, "* <- %F\n", &m->tx);
		rewritehdr(&m->tx, m->tpkt);
		if(mwrite9p(io, 1, m->tpkt) < 0)
			sysfatal("output error: %r");
		msgput(m);
	}
	closeioproc(io);
}	

void
inputthread(void *arg)
{
	uchar *pkt;
	int n, nn, tag;
	Msg *m;
	Ioproc *io;

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
		if(verbose > 1) fprint(2, "* -> %F\n", &m->rx);
		m->rpkt = pkt;
		m->rx.tag = m->ctag;
		if(m->internal)
			sendp(m->c->internal, 0);
		else
			sendq(m->c->outq, m);
	}
	closeioproc(io);
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
	if(verbose > 2) fprint(2, "msgput tag %d/%d ref %d\n", m->tag, m->ctag, m->ref);
	assert(m->ref > 0);
	if(--m->ref > 0)
		return;
	m->c->nmsg--;
	m->c = nil;
	fidput(m->fid);
	fidput(m->afid);
	fidput(m->newfid);
	free(m->tpkt);
	free(m->rpkt);
	m->fid = nil;
	m->afid = nil;
	m->newfid = nil;
	m->tpkt = nil;
	m->rpkt = nil;
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
	int n;

	n = GBIT32(pkt);
	if(verbose > 2) fprint(2, "write %d %d %.*H\n", fd, n, n, pkt);
	if(iowrite(io, fd, pkt, n) != n){
		fprint(2, "write error: %r\n");
		return -1;
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

#ifdef _LIB9_H_
/* unix select-based polling */
Ioproc*
ioproc(void)
{
	return nil;
}

long
ioread(Ioproc *io, int fd, void *v, long n)
{
	USED(io);

	xxx;
}

long
iowrite(Ioproc *io, int fd, void *v, long n)
{
	USED(io);

	xxx;
}

int
iolisten(Ioproc *io, char *a, char *b)
{
	USED(io);

	xxx;
}

int
ioaccept(Ioproc *io, int fd, char *dir)
{
	USED(io);

	xxx;
}

#else
/* real plan 9 io procs */
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
#endif
