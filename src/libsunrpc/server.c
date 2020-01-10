#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <sunrpc.h>

/*
 * Sun RPC server; for now, no reply cache
 */

static void sunrpcproc(void*);
static void sunrpcrequestthread(void*);
static void sunrpcreplythread(void*);
static void sunrpcforkthread(void*);
static SunProg *sunfindprog(SunSrv*, SunMsg*, SunRpc*, Channel**);

typedef struct Targ Targ;
struct Targ
{
	void (*fn)(void*);
	void *arg;
};

SunSrv*
sunsrv(void)
{
	SunSrv *srv;

	srv = emalloc(sizeof(SunSrv));
	srv->chatty = 0;
	srv->crequest = chancreate(sizeof(SunMsg*), 16);
	srv->creply = chancreate(sizeof(SunMsg*), 16);
	srv->cthread = chancreate(sizeof(Targ), 4);

	proccreate(sunrpcproc, srv, SunStackSize);
	return srv;
}

void
sunsrvprog(SunSrv *srv, SunProg *prog, Channel *c)
{
	if(srv->nprog%16 == 0){
		srv->prog = erealloc(srv->prog, (srv->nprog+16)*sizeof(srv->prog[0]));
		srv->cdispatch = erealloc(srv->cdispatch, (srv->nprog+16)*sizeof(srv->cdispatch[0]));
	}
	srv->prog[srv->nprog] = prog;
	srv->cdispatch[srv->nprog] = c;
	srv->nprog++;
}

static void
sunrpcproc(void *v)
{
	threadcreate(sunrpcreplythread, v, SunStackSize);
	threadcreate(sunrpcrequestthread, v, SunStackSize);
	threadcreate(sunrpcforkthread, v, SunStackSize);

}

static void
sunrpcforkthread(void *v)
{
	SunSrv *srv = v;
	Targ t;

	while(recv(srv->cthread, &t) == 1)
		threadcreate(t.fn, t.arg, SunStackSize);
}

void
sunsrvthreadcreate(SunSrv *srv, void (*fn)(void*), void *arg)
{
	Targ t;

	t.fn = fn;
	t.arg = arg;
	send(srv->cthread, &t);
}

static void
sunrpcrequestthread(void *v)
{
	int status;
	uchar *p, *ep;
	Channel *c;
	SunSrv *srv = v;
	SunMsg *m;
	SunProg *pg;
	SunStatus ok;

	while((m = recvp(srv->crequest)) != nil){
		/* could look up in cache here? */

if(srv->chatty) fprint(2, "sun msg %p count %d\n", m, m->count);
		m->srv = srv;
		p = m->data;
		ep = p+m->count;
		status = m->rpc.status;
		if(sunrpcunpack(p, ep, &p, &m->rpc) != SunSuccess){
			fprint(2, "in: %.*H unpack failed\n", m->count, m->data);
			sunmsgdrop(m);
			continue;
		}
		if(srv->chatty)
			fprint(2, "in: %B\n", &m->rpc);
		if(status){
			sunmsgreplyerror(m, status);
			continue;
		}
		if(srv->alwaysreject){
			if(srv->chatty)
				fprint(2, "\trejecting\n");
			sunmsgreplyerror(m, SunAuthTooWeak);
			continue;
		}

		if(!m->rpc.iscall){
			sunmsgreplyerror(m, SunGarbageArgs);
			continue;
		}

		if((pg = sunfindprog(srv, m, &m->rpc, &c)) == nil){
			/* sunfindprog sent error */
			continue;
		}

		p = m->rpc.data;
		ep = p+m->rpc.ndata;
		m->call = nil;
		if((ok = suncallunpackalloc(pg, m->rpc.proc<<1, p, ep, &p, &m->call)) != SunSuccess){
			sunmsgreplyerror(m, ok);
			continue;
		}
		m->call->rpc = m->rpc;

		if(srv->chatty)
			fprint(2, "\t%C\n", m->call);

		m->pg = pg;
		sendp(c, m);
	}
}

static SunProg*
sunfindprog(SunSrv *srv, SunMsg *m, SunRpc *rpc, Channel **pc)
{
	int i, vlo, vhi, any;
	SunProg *pg;

	vlo = 0;
	vhi = 0;
	any = 0;

	for(i=0; i<srv->nprog; i++){
		pg = srv->prog[i];
		if(pg->prog != rpc->prog)
			continue;
		if(pg->vers == rpc->vers){
			*pc = srv->cdispatch[i];
			return pg;
		}
		/* right program, wrong version: record range */
		if(!any++){
			vlo = pg->vers;
			vhi = pg->vers;
		}else{
			if(pg->vers < vlo)
				vlo = pg->vers;
			if(pg->vers > vhi)
				vhi = pg->vers;
		}
	}
	if(vhi == -1){
		if(srv->chatty)
			fprint(2, "\tprogram %ud unavailable\n", rpc->prog);
		sunmsgreplyerror(m, SunProgUnavail);
	}else{
		/* putting these in rpc is a botch */
		rpc->low = vlo;
		rpc->high = vhi;
		if(srv->chatty)
			fprint(2, "\tversion %ud unavailable; have %d-%d\n", rpc->vers, vlo, vhi);
		sunmsgreplyerror(m, SunProgMismatch);
	}
	return nil;
}

static void
sunrpcreplythread(void *v)
{
	SunMsg *m;
	SunSrv *srv = v;

	while((m = recvp(srv->creply)) != nil){
		/* could record in cache here? */
		sendp(m->creply, m);
	}
}

int
sunmsgreplyerror(SunMsg *m, SunStatus error)
{
	uchar *p, *bp, *ep;
	int n;

	m->rpc.status = error;
	m->rpc.iscall = 0;
	m->rpc.verf.flavor = SunAuthNone;
	m->rpc.data = nil;
	m->rpc.ndata = 0;

	if(m->srv->chatty)
		fprint(2, "out: %B\n", &m->rpc);

	n = sunrpcsize(&m->rpc);
	bp = emalloc(n);
	ep = bp+n;
	p = bp;
	if((int32)sunrpcpack(p, ep, &p, &m->rpc) < 0){
		fprint(2, "sunrpcpack failed\n");
		sunmsgdrop(m);
		return 0;
	}
	if(p != ep){
		fprint(2, "sunmsgreplyerror: rpc sizes didn't work out\n");
		sunmsgdrop(m);
		return 0;
	}
	free(m->data);
	m->data = bp;
	m->count = n;
	sendp(m->srv->creply, m);
	return 0;
}

int
sunmsgreply(SunMsg *m, SunCall *c)
{
	int n1, n2;
	uchar *bp, *p, *ep;

	c->type = m->call->type+1;
	c->rpc.iscall = 0;
	c->rpc.prog = m->rpc.prog;
	c->rpc.vers = m->rpc.vers;
	c->rpc.proc = m->rpc.proc;
	c->rpc.xid = m->rpc.xid;

	if(m->srv->chatty){
		fprint(2, "out: %B\n", &c->rpc);
		fprint(2, "\t%C\n", c);
	}

	n1 = sunrpcsize(&c->rpc);
	n2 = suncallsize(m->pg, c);

	bp = emalloc(n1+n2);
	ep = bp+n1+n2;
	p = bp;
	if(sunrpcpack(p, ep, &p, &c->rpc) != SunSuccess){
		fprint(2, "sunrpcpack failed\n");
		return sunmsgdrop(m);
	}
	if(suncallpack(m->pg, p, ep, &p, c) != SunSuccess){
		fprint(2, "pg->pack failed\n");
		return sunmsgdrop(m);
	}
	if(p != ep){
		fprint(2, "sunmsgreply: sizes didn't work out\n");
		return sunmsgdrop(m);
	}
	free(m->data);
	m->data = bp;
	m->count = n1+n2;

	sendp(m->srv->creply, m);
	return 0;
}

int
sunmsgdrop(SunMsg *m)
{
	free(m->data);
	free(m->call);
	memset(m, 0xFB, sizeof *m);
	free(m);
	return 0;
}
