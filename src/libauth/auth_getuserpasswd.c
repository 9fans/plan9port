#include <u.h>
#include <libc.h>
#include <auth.h>
#include "authlocal.h"

enum {
	ARgiveup = 100,
};

static int
dorpc(AuthRpc *rpc, char *verb, char *val, int len, AuthGetkey *getkey)
{
	int ret;

	for(;;){
		if((ret = auth_rpc(rpc, verb, val, len)) != ARneedkey && ret != ARbadkey)
			return ret;
		if(getkey == nil)
			return ARgiveup;	/* don't know how */
		if((*getkey)(rpc->arg) < 0)
			return ARgiveup;	/* user punted */
	}
}

UserPasswd*
auth_getuserpasswd(AuthGetkey *getkey, char *fmt, ...)
{
	AuthRpc *rpc;
	char *f[3], *p, *params;
	int fd;
	va_list arg;
	UserPasswd *up;

	up = nil;
	rpc = nil;
	params = nil;

	fd = open("/mnt/factotum/rpc", ORDWR);
	if(fd < 0)
		goto out;
	rpc = auth_allocrpc(fd);
	if(rpc == nil)
		goto out;
	quotefmtinstall();	/* just in case */
	va_start(arg, fmt);
	params = vsmprint(fmt, arg);
	va_end(arg);
	if(params == nil)
		goto out;

	if(dorpc(rpc, "start", params, strlen(params), getkey) != ARok
	|| dorpc(rpc, "read", nil, 0, getkey) != ARok)
		goto out;

	rpc->arg[rpc->narg] = '\0';
	if(tokenize(rpc->arg, f, 2) != 2){
		werrstr("bad answer from factotum");
		goto out;
	}
	up = malloc(sizeof(*up)+rpc->narg+1);
	if(up == nil)
		goto out;
	p = (char*)&up[1];
	strcpy(p, f[0]);
	up->user = p;
	p += strlen(p)+1;
	strcpy(p, f[1]);
	up->passwd = p;

out:
	free(params);
	auth_freerpc(rpc);
	close(fd);
	return up;
}
