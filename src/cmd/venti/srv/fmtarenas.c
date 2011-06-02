#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
usage(void)
{
	fprint(2, "usage: fmtarenas [-Z] [-b blocksize] [-a arenasize] name file\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	int vers;
	ArenaPart *ap;
	Part *part;
	Arena *arena;
	u64int addr, limit, asize, apsize;
	char *file, *name, aname[ANameSize];
	int i, n, blocksize, tabsize, zero;

	ventifmtinstall();
	statsinit();

	blocksize = 8 * 1024;
	asize = 512 * 1024 *1024;
	tabsize = 512 * 1024;		/* BUG: should be determine from number of arenas */
	zero = -1;
	vers = ArenaVersion5;
	ARGBEGIN{
	case 'D':
		settrace(EARGF(usage()));
		break;
	case 'a':
		asize = unittoull(EARGF(usage()));
		if(asize == TWID64)
			usage();
		break;
	case 'b':
		blocksize = unittoull(EARGF(usage()));
		if(blocksize == ~0)
			usage();
		if(blocksize > MaxDiskBlock){
			fprint(2, "block size too large, max %d\n", MaxDiskBlock);
			threadexitsall("usage");
		}
		break;
	case '4':
		vers = ArenaVersion4;
		break;
	case 'Z':
		zero = 0;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(zero == -1){
		if(vers == ArenaVersion4)
			zero = 1;
		else
			zero = 0;
	}

	if(argc != 2)
		usage();

	name = argv[0];
	file = argv[1];

	if(nameok(name) < 0)
		sysfatal("illegal name template %s", name);

	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	if(zero)
		zeropart(part, blocksize);

	maxblocksize = blocksize;
	initdcache(20*blocksize);

	ap = newarenapart(part, blocksize, tabsize);
	if(ap == nil)
		sysfatal("can't initialize arena: %r");

	apsize = ap->size - ap->arenabase;
	n = apsize / asize;
	if(apsize - (n * asize) >= MinArenaSize)
		n++;

	fprint(2, "fmtarenas %s: %,d arenas, %,lld bytes storage, %,d bytes for index map\n",
		file, n, apsize, ap->tabsize);

	ap->narenas = n;
	ap->map = MKNZ(AMap, n);
	ap->arenas = MKNZ(Arena*, n);

	addr = ap->arenabase;
	for(i = 0; i < n; i++){
		limit = addr + asize;
		if(limit >= ap->size || ap->size - limit < MinArenaSize){
			limit = ap->size;
			if(limit - addr < MinArenaSize)
				sysfatal("bad arena set math: runt arena at %lld,%lld %lld", addr, limit, ap->size);
		}

		snprint(aname, ANameSize, "%s%d", name, i);

		if(0) fprint(2, "adding arena %s at [%lld,%lld)\n", aname, addr, limit);

		arena = newarena(part, vers, aname, addr, limit - addr, blocksize);
		if(!arena)
			fprint(2, "can't make new arena %s: %r", aname);
		freearena(arena);

		ap->map[i].start = addr;
		ap->map[i].stop = limit;
		namecp(ap->map[i].name, aname);

		addr = limit;
	}

	if(wbarenapart(ap) < 0)
		fprint(2, "can't write back arena partition header for %s: %r\n", file);

	flushdcache();
	threadexitsall(0);
}
