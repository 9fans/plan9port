#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>

void
usage(void)
{
	fprint(2, "usage: root [-h host] score\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int i, n;
	uchar score[VtScoreSize];
	uchar *buf;
	VtConn *z;
	char *host;
	VtRoot root;

	fmtinstall('F', vtfcallfmt);
	fmtinstall('V', vtscorefmt);
	quotefmtinstall();

	host = nil;
	ARGBEGIN{
	case 'h':
		host = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc == 0)
		usage();

	buf = vtmallocz(VtMaxLumpSize);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	for(i=0; i<argc; i++){
		if(vtparsescore(argv[i], nil, score) < 0){
			fprint(2, "cannot parse score '%s': %r\n", argv[i]);
			continue;
		}
		n = vtread(z, score, VtRootType, buf, VtMaxLumpSize);
		if(n < 0){
			fprint(2, "could not read block %V: %r\n", score);
			continue;
		}
		if(n != VtRootSize){
			fprint(2, "block %V is wrong size %d != 300\n", score, n);
			continue;
		}
		if(vtrootunpack(&root, buf) < 0){
			fprint(2, "unpacking block %V: %r\n", score);
			continue;
		}
		print("%V: %q %q %V %d %V\n", score, root.name, root.type, root.score, root.blocksize, root.prev);
	}
	vthangup(z);
	threadexitsall(0);
}
