#include <u.h>
#include <libc.h>
#include <auth.h>
#include <authsrv.h>
#include "authlocal.h"

Attr*
auth_attr(AuthRpc *rpc)
{
	if(auth_rpc(rpc, "attr", nil, 0) != ARok)
		return nil;
	return _parseattr(rpc->arg);
}
