#include <u.h>
#include <libc.h>
#include <auth.h>
#include <authsrv.h>
#include "authlocal.h"

Chalstate*
auth_challenge(char *fmt, ...)
{
	char *p;
	va_list arg;
	Chalstate *c;

	quotefmtinstall();	/* just in case */
	va_start(arg, fmt);
	p = vsmprint(fmt, arg);
	va_end(arg);
	if(p == nil)
		return nil;

	c = mallocz(sizeof(*c), 1);
	if(c == nil){
		free(p);
		return nil;
	}

	if((c->afd = open("/mnt/factotum/rpc", ORDWR)) < 0){
	Error:
		auth_freechal(c);
		free(p);
		return nil;
	}

	if((c->rpc=auth_allocrpc(c->afd)) == nil
	|| auth_rpc(c->rpc, "start", p, strlen(p)) != ARok
	|| auth_rpc(c->rpc, "read", nil, 0) != ARok)
		goto Error;

	if(c->rpc->narg > sizeof(c->chal)-1){
		werrstr("buffer too small for challenge");
		goto Error;
	}
	memmove(c->chal, c->rpc->arg, c->rpc->narg);
	c->nchal = c->rpc->narg;
	free(p);
	return c;
}

AuthInfo*
auth_response(Chalstate *c)
{
	int ret;
	AuthInfo *ai;

	ai = nil;
	if(c->afd < 0){
		werrstr("auth_response: connection not open");
		return nil;
	}
	if(c->resp == nil){
		werrstr("auth_response: nil response");
		return nil;
	}
	if(c->nresp == 0){
		werrstr("auth_response: unspecified response length");
		return nil;
	}

	if(c->user){
		if(auth_rpc(c->rpc, "write", c->user, strlen(c->user)) != ARok){
			/*
			 * if this fails we're out of phase with factotum.
			 * give up.
			 */
			goto Out;
		}
	}

	if(auth_rpc(c->rpc, "write", c->resp, c->nresp) != ARok){
		/*
		 * don't close the connection -- maybe we'll try again.
		 */
		return nil;
	}

	switch(ret = auth_rpc(c->rpc, "read", nil, 0)){
	case ARok:
	default:
		werrstr("factotum protocol botch %d %s", ret, c->rpc->ibuf);
		break;
	case ARdone:
		ai = auth_getinfo(c->rpc);
		break;
	}

Out:
	close(c->afd);
	auth_freerpc(c->rpc);
	c->afd = -1;
	c->rpc = nil;
	return ai;
}

void
auth_freechal(Chalstate *c)
{
	if(c == nil)
		return;

	if(c->afd >= 0)
		close(c->afd);
	if(c->rpc != nil)
		auth_freerpc(c->rpc);

	memset(c, 0xBB, sizeof(*c));
	free(c);
}
