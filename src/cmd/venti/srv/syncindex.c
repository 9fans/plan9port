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

Config conf;

void
threadmain(int argc, char *argv[])
{
	u32int bcmem, icmem;
	int fix;

	fix = 0;
	bcmem = 0;
	icmem = 0;
	ARGBEGIN{
	case 'B':
		bcmem = unittoull(EARGF(usage()));
		break;
	case 'I':
		icmem = unittoull(EARGF(usage()));
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

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);
	initlumpcache(1*1024*1024, 1024/8);
	icmem = u64log2(icmem / (sizeof(IEntry)+sizeof(IEntry*)) / ICacheDepth);
	if(icmem < 4)
		icmem = 4;
	if(1) fprint(2, "initialize %d bytes of index cache for %d index entries\n",
		(sizeof(IEntry)+sizeof(IEntry*)) * (1 << icmem) * ICacheDepth,
		(1 << icmem) * ICacheDepth);
	initicache(icmem, ICacheDepth);
	initicachewrite();
	if(mainindex->bloom)
		startbloomproc(mainindex->bloom);

	if(verbose)
		printindex(2, mainindex);
	if(syncindex(mainindex, fix, 1, 0) < 0)
		sysfatal("failed to sync index=%s: %r\n", mainindex->name);

	threadexitsall(0);
}
