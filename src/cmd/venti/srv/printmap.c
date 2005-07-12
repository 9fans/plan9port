#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
usage(void)
{
	fprint(2, "usage: printmap [-B blockcachesize] config\n");
	threadexitsall("usage");
}

Config conf;

void
threadmain(int argc, char *argv[])
{
	u32int bcmem;
	int fix;

	fix = 0;
	bcmem = 0;
	ARGBEGIN{
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	default:
		usage();
		break;
	}ARGEND

	if(!fix)
		readonly = 1;

	if(argc != 1)
		usage();

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");

	printindex(1, mainindex);
	threadexitsall(0);
}
