#include <u.h>
#include <libc.h>
#include <venti.h>
#include <thread.h>
#include "queue.h"

enum
{
	STACK = 8192,
};

typedef struct VtSconn VtSconn;
struct VtSconn
{
	int ctl;
	char dir[NETPATHLEN];
	VtSrv *srv;
	VtConn *c;
};

struct VtSrv
{
	int afd;
	int dead;
	char adir[NETPATHLEN];
	Queue *q;	/* Queue(VtReq*) */
};

static void listenproc(void*);
static void connproc(void*);

VtSrv*
vtlisten(char *addr)
{
	VtSrv *s;

	s = vtmallocz(sizeof(VtSrv));
	s->afd = announce(addr, s->adir);
	if(s->afd < 0){
		free(s);
		return nil;
	}
	s->q = _vtqalloc();
	proccreate(listenproc, s, STACK);
	return s;
}

static void
listenproc(void *v)
{
	int ctl;
	char dir[NETPATHLEN];
	VtSrv *srv;
	VtSconn *sc;

	srv = v;
	for(;;){
fprint(2, "listen for venti\n");
		ctl = listen(srv->adir, dir);
		if(ctl < 0){
			srv->dead = 1;
			break;
		}
fprint(2, "got one\n");
		sc = vtmallocz(sizeof(VtSconn));
		sc->ctl = ctl;
		sc->srv = srv;
		strcpy(sc->dir, dir);
		proccreate(connproc, sc, STACK);
	}

	// hangup
}

static void
connproc(void *v)
{
	VtSconn *sc;
	VtConn *c;
	Packet *p;
	VtReq *r;
	int fd;

	r = nil;
	c = nil;
	sc = v;
	fprint(2, "new call %s on %d\n", sc->dir, sc->ctl);
	fd = accept(sc->ctl, sc->dir);
	close(sc->ctl);
	if(fd < 0){
		fprint(2, "accept %s: %r\n", sc->dir);
		goto out;
	}

	c = vtconn(fd, fd);
	sc->c = c;
	if(vtversion(c) < 0){
		fprint(2, "vtversion %s: %r\n", sc->dir);
		goto out;
	}
	if(vtsrvhello(c) < 0){
		fprint(2, "vtsrvhello %s: %r\n", sc->dir);
		goto out;
	}

	fprint(2, "new proc %s\n", sc->dir);
	proccreate(vtsendproc, c, STACK);
	qlock(&c->lk);
	while(!c->writeq)
		rsleep(&c->rpcfork);
	qunlock(&c->lk);

	while((p = vtrecv(c)) != nil){
		r = vtmallocz(sizeof(VtReq));
		if(vtfcallunpack(&r->tx, p) < 0){
			packetfree(p);
			fprint(2, "bad packet on %s: %r\n", sc->dir);
			continue;
		}
		packetfree(p);
		if(r->tx.type == VtTgoodbye)
			break;
		r->rx.tag = r->tx.tag;
		r->sc = sc;
		if(_vtqsend(sc->srv->q, r) < 0){
			fprint(2, "hungup queue\n");
			break;
		}
		r = nil;
	}

	fprint(2, "eof on %s\n", sc->dir);

out:
	if(r){
		vtfcallclear(&r->tx);
		vtfree(r);
	}
	if(c)
		vtfreeconn(c);
	fprint(2, "freed %s\n", sc->dir);
	vtfree(sc);
	return;
}

VtReq*
vtgetreq(VtSrv *srv)
{
	return _vtqrecv(srv->q);
}

void
vtrespond(VtReq *r)
{
	Packet *p;
	VtSconn *sc;

	sc = r->sc;
	if(r->rx.tag != r->tx.tag)
		abort();
	if(r->rx.type != r->tx.type+1 && r->rx.type != VtRerror)
		abort();
	if((p = vtfcallpack(&r->rx)) == nil){
		fprint(2, "fcallpack on %s: %r\n", sc->dir);
		packetfree(p);
		vtfcallclear(&r->rx);
		return;
	}
	vtsend(sc->c, p);
	vtfcallclear(&r->tx);
	vtfcallclear(&r->rx);
	vtfree(r);
}

