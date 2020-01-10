#include <u.h>
#include <libc.h>
#include <venti.h>
#include <thread.h>
#include "queue.h"

enum
{
	STACK = 8192
};

typedef struct VtSconn VtSconn;
struct VtSconn
{
	int ctl;
	int ref;
	QLock lk;
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

char *VtServerLog = "libventi/server";

static void
scincref(VtSconn *sc)
{
	qlock(&sc->lk);
	sc->ref++;
	qunlock(&sc->lk);
}

static void
scdecref(VtSconn *sc)
{
	qlock(&sc->lk);
	if(--sc->ref > 0){
		qunlock(&sc->lk);
		return;
	}
	if(sc->c)
		vtfreeconn(sc->c);
	vtfree(sc);
}

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
		ctl = listen(srv->adir, dir);
		if(ctl < 0){
			srv->dead = 1;
			break;
		}
		sc = vtmallocz(sizeof(VtSconn));
		sc->ref = 1;
		sc->ctl = ctl;
		sc->srv = srv;
		strcpy(sc->dir, dir);
		proccreate(connproc, sc, STACK);
	}

	/* hangup */
}

static void
connproc(void *v)
{
	VtSconn *sc;
	VtConn *c;
	Packet *p;
	VtReq *r;
	int fd;
static int first=1;

if(first && chattyventi){
	first=0;
	fmtinstall('F', vtfcallfmt);
}
	r = nil;
	sc = v;
	sc->c = nil;
	if(0) fprint(2, "new call %s on %d\n", sc->dir, sc->ctl);
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

	if(0) fprint(2, "new proc %s\n", sc->dir);
	proccreate(vtsendproc, c, STACK);
	qlock(&c->lk);
	while(!c->writeq)
		rsleep(&c->rpcfork);
	qunlock(&c->lk);

	while((p = vtrecv(c)) != nil){
		r = vtmallocz(sizeof(VtReq));
		if(vtfcallunpack(&r->tx, p) < 0){
			vtlog(VtServerLog, "<font size=-1>%T %s:</font> recv bad packet %p: %r<br>\n", c->addr, p);
			fprint(2, "bad packet on %s: %r\n", sc->dir);
			packetfree(p);
			continue;
		}
		vtlog(VtServerLog, "<font size=-1>%T %s:</font> recv packet %p (%F)<br>\n", c->addr, p, &r->tx);
		if(chattyventi)
			fprint(2, "%s <- %F\n", argv0, &r->tx);
		packetfree(p);
		if(r->tx.msgtype == VtTgoodbye)
			break;
		r->rx.tag = r->tx.tag;
		r->sc = sc;
		scincref(sc);
		if(_vtqsend(sc->srv->q, r) < 0){
			scdecref(sc);
			fprint(2, "hungup queue\n");
			break;
		}
		r = nil;
	}

	if(0) fprint(2, "eof on %s\n", sc->dir);

out:
	if(r){
		vtfcallclear(&r->tx);
		vtfree(r);
	}
	if(0) fprint(2, "freed %s\n", sc->dir);
	scdecref(sc);
	return;
}

VtReq*
vtgetreq(VtSrv *srv)
{
	VtReq *r;

	r = _vtqrecv(srv->q);
	if (r != nil)
		vtlog(VtServerLog, "<font size=-1>%T %s:</font> vtgetreq %F<br>\n", ((VtSconn*)r->sc)->c->addr, &r->tx);
	return r;
}

void
vtrespond(VtReq *r)
{
	Packet *p;
	VtSconn *sc;

	sc = r->sc;
	if(r->rx.tag != r->tx.tag)
		abort();
	if(r->rx.msgtype != r->tx.msgtype+1 && r->rx.msgtype != VtRerror)
		abort();
	if(chattyventi)
		fprint(2, "%s -> %F\n", argv0, &r->rx);
	if((p = vtfcallpack(&r->rx)) == nil){
		vtlog(VtServerLog, "%s: vtfcallpack %F: %r<br>\n", sc->c->addr, &r->rx);
		fprint(2, "fcallpack on %s: %r\n", sc->dir);
		packetfree(p);
		vtfcallclear(&r->rx);
		return;
	}
	vtlog(VtServerLog, "<font size=-1>%T %s:</font> send packet %p (%F)<br>\n", sc->c->addr, p, &r->rx);
	if(vtsend(sc->c, p) < 0)
		fprint(2, "vtsend %F: %r\n", &r->rx);
	scdecref(sc);
	vtfcallclear(&r->tx);
	vtfcallclear(&r->rx);
	vtfree(r);
}
