#include <u.h>
#include <libc.h>
#include <venti.h>

VtConn*
vtdial(char *addr)
{
	char *na;
	int fd;
	VtConn *z;

	if(addr == nil)
		addr = getenv("venti");
	if(addr == nil)
		addr = "$venti";

	na = netmkaddr(addr, "tcp", "venti");
	if((fd = dial(na, nil, nil, nil)) < 0)
		return nil;

	z = vtconn(fd, fd);
	if(z)
		strecpy(z->addr, z->addr+sizeof z->addr, na);
	return z;
}
