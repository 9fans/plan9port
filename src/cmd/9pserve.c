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

void *gethash(Hash**, uint);
int puthash(Hash**, uint, void*);
int delhash(Hash**, uint, void*);
Msg *mread9p(int);
int mwrite9p(int, Msg*);
uchar *read9ppkt(int);
int write9ppkt(int, uchar*);
Msg *msgnew(void);
void msgput(Msg*);
Msg *msgget(int);
Fid *fidnew(int);
void fidput(Fid*);
void *emalloc(int);
void *erealloc(void*, int);
int sendq(Queue*, void*);
void *recvq(Queue*);
void selectthread(void*);
void connthread(void*);
void listenthread(void*);
void rewritehdr(Fcall*, uchar*);
int tlisten(char*, char*);
int taccept(int, char*);

void
usage(void)
{
	fprint(2, "usage: 9pserve [-u] address\n");
	fprint(2, "\treads/writes 9P messages on stdin/stdout\n");
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	default:
		usage();
	case 'u':
		isunix = 1;
		break;
	}ARGEND

	if(argc != 1)
		usage();

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);

	threadcreateidle(selectthread, nil, STACK);
}

void
listenthread(void *arg)
{
	Conn *c;

	USED(arg);
	for(;;){
		c = malloc(sizeof(Conn));
		if(c == nil){
			fprint(2, "out of memory\n");
			sleep(60*1000);
			continue;
		}
		c->fd = tlisten(adir, c->dir);
		if(c->fd < 0){
			fprint(2, "listen: %r\n");
			close(afd);
			free(c);
			return;
		}
		threadcreate(connthread, c, STACK);
	}	
}

void
err(Msg *m, char *ename)
{
	int n, nn;

	m->rx.type = Rerror;
	m->rx.ename = ename;
	m->rx.tag = m->ctag;
	n = sizeS2M(&m->rx);
	m->rpkt = emalloc(n);
	nn = convS2M(&m->rx, m->rpkt, n);
	if(nn != n)
		sysfatal("sizeS2M + convS2M disagree");
	sendq(m->c->outq, m);
}

void
connthread(void *arg)
{
	int i, fd;
	Conn *c;
	Hash *h;
	Msg *m, *om;
	Fid *f;

	c = arg;
	fd = taccept(c->fd, c->dir);
	if(fd < 0){
		fprint(2, "accept %s: %r\n", c->dir);
		goto out;
	}
	close(c->fd);
	c->fd = fd;
	while((m = mread9p(c->fd)) != nil){
		m->c = c;
		c->nmsg++;
		if(puthash(c->tag, m->tx.tag, m) < 0){
			err(m, "duplicate tag");
			continue;
		}
		switch(m->tx.type){
		case Tflush:
			if((m->oldm = gethash(c->tag, m->tx.oldtag)) == nil){
				m->rx.tag = Rflush;
				sendq(c->outq, m);
				continue;
			}
			break;
		case Tattach:
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
			if((m->afid = gethash(c->fid, m->tx.afid)) == nil){
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
		case Topen:
		case Tclunk:
		case Tread:
		case Twrite:
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
		rewritehdr(&m->tx, m->tpkt);
		sendq(outq, m);
		while(c->nmsg >= MAXMSG){
			c->inputstalled = 1;
			recvp(c->inc);
		}
	}

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
			sendq(outq, m);
			recvp(c->internal);
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
			sendq(outq, m);
			recvp(c->internal);
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

	c = arg;
	while((m = recvq(c->outq)) != nil){
		err = m->tx.type+1 != m->rx.type;
		switch(m->tx.type){
		case Tflush:
			om = m->oldm;
			if(delhash(om->c->tag, om->ctag, om) == 0)
				msgput(om);
			break;
		case Tclunk:
			if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
				fidput(m->fid);
			break;
		case Tauth:
			if(err)
				if(delhash(m->c->fid, m->afid->cfid, m->fid) == 0)
					fidput(m->fid);
		case Tattach:
			if(err)
				if(delhash(m->c->fid, m->fid->cfid, m->fid) == 0)
					fidput(m->fid);
			break;
		case Twalk:
			if(err && m->tx.fid != m->tx.newfid)
				if(delhash(m->c->fid, m->newfid->cfid, m->newfid) == 0)
					fidput(m->newfid);
			break;
		}
		if(mwrite9p(c->fd, m) < 0)
			fprint(2, "write error: %r\n");
		if(delhash(m->c->tag, m->ctag, m) == 0)
			msgput(m);
		msgput(m);
		if(c->inputstalled && c->nmsg < MAXMSG)
			nbsendp(c->inc, 0);
	}
}

void
outputthread(void *arg)
{
	Msg *m;

	USED(arg);

	while((m = recvq(outq)) != nil){
		if(mwrite9p(1, m) < 0)
			sysfatal("output error: %r");
		msgput(m);
	}
}	

void
inputthread(void *arg)
{
	uchar *pkt;
	int n, nn, tag;
	Msg *m;

	while((pkt = read9ppkt(0)) != nil){
		n = GBIT32(pkt);
		if(n < 7){
			fprint(2, "short 9P packet\n");
			free(pkt);
			continue;
		}
		tag = GBIT16(pkt+5);
		if((m = msgget(tag)) == nil){
			fprint(2, "unexpected 9P response tag %d\n", tag);
			free(pkt);
			msgput(m);
			continue;
		}
		if((nn = convM2S(pkt, n, &m->rx)) != n){
			fprint(2, "bad packet - convM2S %d but %d\n", nn, n);
			free(pkt);
			msgput(m);
			continue;
		}
		m->rpkt = pkt;
		m->rx.tag = m->ctag;
		rewritehdr(&m->rx, m->rpkt);
		sendq(m->c->outq, m);
	}
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
			if(h->v != v)
				fprint(2, "hash error\n");
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
		fidtab = erealloc(fidtab, nfidtab*sizeof(fidtab[0]));
		fidtab[nfidtab] = emalloc(sizeof(Fid));
		freefid = fidtab[nfidtab++];
	}
	f = freefid;
	freefid = f->next;
	f->cfid = f->cfid;
	f->ref = 1;
	return f;
}

void
fidput(Fid *f)
{
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
		msgtab = erealloc(msgtab, nmsgtab*sizeof(msgtab[0]));
		msgtab[nmsgtab] = emalloc(sizeof(Msg));
		freemsg = msgtab[nmsgtab++];
	}
	m = freemsg;
	freemsg = m->next;
	m->ref = 1;
	return m;
}

void
msgput(Msg *m)
{
	assert(m->ref > 0);
	if(--m->ref > 0)
		return;
	m->next = freemsg;
	freemsg = m;
}

void*
emalloc(int n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

void*
erealloc(void *v, int n)
{
	v = realloc(v, n);
	if(v == nil)
		sysfatal("out of memory");
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
