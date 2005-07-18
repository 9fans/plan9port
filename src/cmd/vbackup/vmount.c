#include <u.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libc.h>
#include "mountnfs.h"

void
usage(void)
{
	fprint(2, "usage: vmount [-v] [-h handle] address mountpoint\n");
	exits("usage");
}

int handlelen = 1;
uchar handle[64] = {
	0x00
};

void
main(int argc, char **argv)
{
	char *p, *net, *unx;
	u32int host;
	int n, port, proto, verbose;
	struct sockaddr_in sa;

	verbose = 0;
	ARGBEGIN{
	case 'h':
		p = EARGF(usage());
		n = strlen(p);
		if(n%2)
			sysfatal("bad handle '%s'", p);
		if(n > 2*sizeof handle)
			sysfatal("handle too long '%s'", p);
		handlelen = n/2;
		if(dec16(handle, n/2, p, n) != n/2)
			sysfatal("bad hex in handle '%s'", p);
		break;

	case 'v':
		verbose = 1;
		break;

	default:
		usage();
	}ARGEND

	if(argc != 2)
		usage();

	p = p9netmkaddr(argv[0], "udp", "nfs");
	if(p9dialparse(strdup(p), &net, &unx, &host, &port) < 0)
		sysfatal("bad address '%s'", p);

	if(verbose)
		print("nfs server is net=%s addr=%d.%d.%d.%d port=%d\n",
			net, host&0xFF, (host>>8)&0xFF, (host>>16)&0xFF, host>>24, port);

	proto = 0;
	if(strcmp(net, "tcp") == 0)
		proto = SOCK_STREAM;
	else if(strcmp(net, "udp") == 0)
		proto = SOCK_DGRAM;
	else
		sysfatal("bad proto %s: can only handle tcp and udp", net);

	memset(&sa, 0, sizeof sa);
	memmove(&sa.sin_addr, &host, 4);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	mountnfs(proto, &sa, handle, handlelen, argv[1]);
	exits(0);
}
