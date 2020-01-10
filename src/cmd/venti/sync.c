#include <u.h>
#include <libc.h>
#include <thread.h>
#include <venti.h>

char *host;
int donothing;

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

	fmtinstall('V', vtscorefmt);
	fmtinstall('F', vtfcallfmt);

	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		if(host == nil)
			usage();
		break;
	case 'x':
		donothing = 1;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 0)
		usage();

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	if(!donothing)
	if(vtsync(z) < 0)
		sysfatal("vtsync: %r");

	vthangup(z);
	threadexitsall(0);
}
