#include <u.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libc.h>
#include "mountnfs.h"

void
usage(void)
{
	fprint(2, "usage: vmount [-v] [-h handle] address mountpoint\n");
	exits("usage");
}

int handlelen = 20;
uchar handle[64] = {
	/* SHA1("/") */
	0x42, 0x09, 0x9B, 0x4A, 0xF0, 0x21, 0xE5, 0x3F, 0xD8, 0xFD,
	0x4E, 0x05, 0x6C, 0x25, 0x68, 0xD7, 0xC2, 0xE3, 0xFF, 0xA8
};

void
main(int argc, char **argv)
{
	char *p, *net, *unx;
	char host[INET_ADDRSTRLEN];
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
	if(p9dialparse(strdup(p), &net, &unx, &sa, &port) < 0)
		sysfatal("bad address '%s'", p);

	if(sa.sin_family != AF_INET)
		sysfatal("only IPv4 is supported");

	inet_ntop(AF_INET, &(sa.sin_addr), host, INET_ADDRSTRLEN);

	if(verbose)
		print("nfs server is net=%s addr=%s port=%d\n",
			net, host, port);

	proto = 0;
	if(strcmp(net, "tcp") == 0)
		proto = SOCK_STREAM;
	else if(strcmp(net, "udp") == 0)
		proto = SOCK_DGRAM;
	else
		sysfatal("bad proto %s: can only handle tcp and udp", net);

	mountnfs(proto, &sa, handle, handlelen, argv[1]);
	exits(0);
}
