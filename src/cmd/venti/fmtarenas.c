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
	ArenaPart *ap;
	Part *part;
	Arena *arena;
	u64int addr, limit, asize, apsize;
	char *file, *name, aname[ANameSize];
	int i, n, blocksize, tabsize, zero;

	fmtinstall('V', vtscorefmt);

	statsinit();

	blocksize = 8 * 1024;
	asize = 512 * 1024 *1024;
	tabsize = 64 * 1024;		/* BUG: should be determine from number of arenas */
	zero = 1;
	ARGBEGIN{
	case 'a':
		asize = unittoull(ARGF());
		if(asize == TWID64)
			usage();
		break;
	case 'b':
		blocksize = unittoull(ARGF());
		if(blocksize == ~0)
			usage();
		if(blocksize > MaxDiskBlock){
			fprint(2, "block size too large, max %d\n", MaxDiskBlock);
			threadexitsall("usage");
		}
		break;
	case 'Z':
		zero = 0;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 2)
		usage();

	name = argv[0];
	file = argv[1];

	if(nameok(name) < 0)
		sysfatal("illegal name template %s", name);

	part = initpart(file, 1);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	if(zero)
		zeropart(part, blocksize);

	ap = newarenapart(part, blocksize, tabsize);
	if(ap == nil)
		sysfatal("can't initialize arena: %r");

	apsize = ap->size - ap->arenabase;
	n = apsize / asize;
	if(apsize - (n * asize) >= MinArenaSize)
		n++;
	
	fprint(2, "configuring %s with arenas=%d for a total storage of bytes=%lld and directory bytes=%d\n",
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
				sysfatal("bad arena set math: runt arena at %lld,%lld %lld\n", addr, limit, ap->size);
		}

		snprint(aname, ANameSize, "%s%d", name, i);

		fprint(2, "adding arena %s at [%lld,%lld)\n", aname, addr, limit);

		arena = newarena(part, aname, addr, limit - addr, blocksize);
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

	threadexitsall(0);
}
