#include "stdinc.h"
#include "dat.h"
#include "fns.h"

char *host;

void
usage(void)
{
	fprint(2, "usage: write [-z] [-h host] [-t type] <datablock\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	uchar *p, score[VtScoreSize];
	int type, n, dotrunc;
	VtConn *z;

	dotrunc = 0;
	type = VtDataType;
	ARGBEGIN{
	case 'z':
		dotrunc = 1;
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 't':
		type = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 0)
		usage();


	fmtinstall('V', vtscorefmt);

	p = ezmalloc(VtMaxLumpSize+1);
	n = readn(0, p, VtMaxLumpSize+1);
	if(n > VtMaxLumpSize)
		sysfatal("data too big");
	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");
	if(dotrunc)
		n = vtzerotruncate(type, p, n);
	if(vtwrite(z, score, type, p, n) < 0)
		sysfatal("vtwrite: %r");
	vthangup(z);
	print("%V\n", score);
	threadexitsall(0);
}
