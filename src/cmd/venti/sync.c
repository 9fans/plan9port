#include "stdinc.h"
#include "dat.h"
#include "fns.h"

char *host;

void
usage(void)
{
	fprint(2, "usage: sync [-h host]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	VtConn *z;

	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		if(host == nil)
			usage();
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 0)
		usage();


	fmtinstall('V', vtscorefmt);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	if(vtsync(z) < 0)
		sysfatal("vtsync: %r");

	vthangup(z);
	threadexitsall(0);
}
