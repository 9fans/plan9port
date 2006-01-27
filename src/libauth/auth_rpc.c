#include <u.h>
#include <libc.h>
#include <auth.h>
#include <9pclient.h>
#include "authlocal.h"

static struct {
	char *verb;
	int val;
} tab[] = {
	"ok",			ARok,
	"done",		ARdone,
	"error",		ARerror,
	"needkey",	ARneedkey,
	"badkey",		ARbadkey,
	"phase",		ARphase,
	"toosmall",	ARtoosmall,
	"error",		ARerror,
};

static long
rpcread(AuthRpc *rpc, void *buf, int buflen)
{
	if (rpc->afd >= 0)
		return read(rpc->afd, buf, buflen);
	else
		return fsread(rpc->afid, buf, buflen);
}

static long
rpcwrite(AuthRpc *rpc, void *buf, int buflen)
{
	if (rpc->afd >= 0)
		return write(rpc->afd, buf, buflen);
	else
		return fswrite(rpc->afid, buf, buflen);
}

static int
classify(char *buf, uint n, AuthRpc *rpc)
{
	int i, len;

	for(i=0; i<nelem(tab); i++){
		len = strlen(tab[i].verb);
		if(n >= len && memcmp(buf, tab[i].verb, len) == 0 && (n==len || buf[len]==' ')){
			if(n==len){
				rpc->narg = 0;
				rpc->arg = "";
			}else{
				rpc->narg = n - (len+1);
				rpc->arg = (char*)buf+len+1;
			}
			return tab[i].val;
		}
	}
	werrstr("malformed rpc response: %s", buf);
	return ARrpcfailure;
}

AuthRpc*
auth_allocrpc(void)
{
	AuthRpc *rpc;

	rpc = mallocz(sizeof(*rpc), 1);
	if(rpc == nil)
		return nil;
	rpc->afd = open("/mnt/factotum/rpc", ORDWR);
	if(rpc->afd < 0){
		rpc->afid = nsopen("factotum", nil, "rpc", ORDWR);
		if(rpc->afid == nil){
			free(rpc);
			return nil;
		}
	}
	return rpc;
}

void
auth_freerpc(AuthRpc *rpc)
{
	if(rpc == nil)
		return;
	if(rpc->afd >= 0)
		close(rpc->afd);
	if(rpc->afid != nil)
		fsclose(rpc->afid);
	free(rpc);
}

uint
auth_rpc(AuthRpc *rpc, char *verb, void *a, int na)
{
	int l, n, type;
	char *f[4];

	l = strlen(verb);
	if(na+l+1 > AuthRpcMax){
		werrstr("rpc too big");
		return ARtoobig;
	}

	memmove(rpc->obuf, verb, l);
	rpc->obuf[l] = ' ';
	memmove(rpc->obuf+l+1, a, na);
	if((n=rpcwrite(rpc, rpc->obuf, l+1+na)) != l+1+na){
		if(n >= 0)
			werrstr("auth_rpc short write");
		return ARrpcfailure;
	}

	if((n=rpcread(rpc, rpc->ibuf, AuthRpcMax)) < 0)
		return ARrpcfailure;
	rpc->ibuf[n] = '\0';

	/*
	 * Set error string for good default behavior.
	 */
	switch(type = classify(rpc->ibuf, n, rpc)){
	default:
		werrstr("unknown rpc type %d (bug in auth_rpc.c)", type);
		break;
	case ARok:
		break;
	case ARrpcfailure:
		break;
	case ARerror:
		if(rpc->narg == 0)
			werrstr("unspecified rpc error");
		else
			werrstr("%s", rpc->arg);
		break;
	case ARneedkey:
		werrstr("needkey %s", rpc->arg);
		break;
	case ARbadkey:
		if(getfields(rpc->arg, f, nelem(f), 0, "\n") < 2)
			werrstr("badkey %s", rpc->arg);
		else
			werrstr("badkey %s", f[1]);
		break;
	case ARphase:
		werrstr("phase error %s", rpc->arg);
		break;
	}
	return type;
}
