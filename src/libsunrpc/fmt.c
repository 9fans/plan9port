#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

/*
 * print formatters
 */
int
sunrpcfmt(Fmt *f)
{
	SunRpc *rpc;

	rpc = va_arg(f->args, SunRpc*);
	sunrpcprint(f, rpc);
	return 0;
}

static SunProg **fmtProg;
static int nfmtProg;
static RWLock fmtLock;

void
sunfmtinstall(SunProg *p)
{
	int i;

	wlock(&fmtLock);
	for(i=0; i<nfmtProg; i++){
		if(fmtProg[i] == p){
			wunlock(&fmtLock);
			return;
		}
	}
	if(nfmtProg%16 == 0)
		fmtProg = erealloc(fmtProg, sizeof(fmtProg[0])*(nfmtProg+16));
	fmtProg[nfmtProg++] = p;
	wunlock(&fmtLock);
}

int
suncallfmt(Fmt *f)
{
	int i;
	void (*fmt)(Fmt*, SunCall*);
	SunCall *c;
	SunProg *p;

	c = va_arg(f->args, SunCall*);
	rlock(&fmtLock);
	for(i=0; i<nfmtProg; i++){
		p = fmtProg[i];
		if(p->prog == c->rpc.prog && p->vers == c->rpc.vers){
			runlock(&fmtLock);
			if((int32)c->type < 0 || c->type >= p->nproc || (fmt=p->proc[c->type].fmt) == nil)
				return fmtprint(f, "unknown proc %c%d", "TR"[c->type&1], c->type>>1);
			(*fmt)(f, c);
			return 0;
		}
	}
	runlock(&fmtLock);
	fmtprint(f, "<sunrpc %d %d %c%d>", c->rpc.prog, c->rpc.vers, "TR"[c->type&1], c->type>>1);
	return 0;
}
