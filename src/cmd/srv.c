#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>

void
usage(void)
{
	fprint(2, "usage: srv addr [srvname]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	int fd;
	char *addr, *service;
	
	ARGBEGIN{
	default:
		usage();
	}ARGEND
	
	if(argc != 1 && argc != 2)
		usage();

	addr = netmkaddr(argv[0], "tcp", "9fs");
	if((fd = dial(addr, nil, nil, nil)) < 0)
		sysfatal("dial %s: %r", addr);
	
	if(argc == 2)
		service = argv[1];
	else
		service = argv[0];
	
	if(post9pservice(fd, service) < 0)
		sysfatal("post9pservice: %r");

	threadexitsall(0);
}
