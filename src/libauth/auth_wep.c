#include <u.h>
#include <libc.h>
#include <auth.h>
#include "authlocal.h"

/*
 *  make factotum add wep keys to an 802.11 device
 */
int
auth_wep(char *dev, char *fmt, ...)
{
	AuthRpc *rpc;
	char *params, *p;
	int fd;
	va_list arg;
	int rv;

	rv = -1;

	if(dev == nil){
		werrstr("no device specified");
		return rv;
	}

	fd = open("/mnt/factotum/rpc", ORDWR);
	if(fd < 0)
		return rv;

	rpc = auth_allocrpc(fd);
	if(rpc != nil){
		quotefmtinstall();	/* just in case */
		va_start(arg, fmt);
		params = vsmprint(fmt, arg);
		va_end(arg);
		if(params != nil){
			p = smprint("proto=wep %s", params);
			if(p != nil){
				if(auth_rpc(rpc, "start", p, strlen(p)) == ARok
				&& auth_rpc(rpc, "write", dev, strlen(dev)) == ARok)
					rv = 0;
				free(p);
			}
			free(params);
		}
		auth_freerpc(rpc);
	}
	close(fd);
		
	return rv;
}
