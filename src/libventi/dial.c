#include <u.h>
#include <libc.h>
#include <venti.h>

VtConn*
vtdial(char *addr)
{
	char *na;
	int fd;

	if(addr == nil)
		addr = getenv("venti");
	if(addr == nil)
		addr = "$venti";

	na = netmkaddr(addr, "net", "venti");
	if((fd = dial(na, nil, nil, nil)) < 0)
		return nil;

	return vtconn(fd, fd);
}
