#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static	int	verbose;
void
usage(void)
{
	fprint(2, "usage: syncindex [-fv] [-B blockcachesize] config\n");
	threadexitsall("usage");
}

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
	case 'f':
		fix++;
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(!fix)
		readonly = 1;

	if(argc != 1)
		usage();

	if(initventi(argv[0]) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	if(verbose)
		printindex(2, mainindex);
	if(syncindex(mainindex, fix) < 0)
		sysfatal("failed to sync index=%s: %r\n", mainindex->name);

	threadexitsall(0);
}
