#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	verbose;

static void
checkarena(Arena *arena, int scan, int fix)
{
	Arena old;
	int err, e;

	if(verbose && arena->clumps)
		printarena(2, arena);

	old = *arena;

	if(scan){
		arena->used = 0;
		arena->clumps = 0;
		arena->cclumps = 0;
		arena->uncsize = 0;
	}

	err = 0;
	for(;;){
		e = syncarena(arena, 1000, 0, fix);
		err |= e;
		if(!(e & SyncHeader))
			break;
		if(verbose && arena->clumps)
			fprint(2, ".");
	}
	if(verbose && arena->clumps)
		fprint(2, "\n");

	err &= ~SyncHeader;
	if(arena->used != old.used
	|| arena->clumps != old.clumps
	|| arena->cclumps != old.cclumps
	|| arena->uncsize != old.uncsize){
		fprint(2, "incorrect arena header fields\n");
		printarena(2, arena);
		err |= SyncHeader;
	}

	if(!err || !fix)
		return;

	fprint(2, "writing fixed arena header fields\n");
	if(wbarena(arena) < 0)
		fprint(2, "arena header write failed: %r\n");
}

void
usage(void)
{
	fprint(2, "usage: checkarenas [-afv] file\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	ArenaPart *ap;
	Part *part;
	char *file;
	int i, fix, scan;

	fmtinstall('V', vtscorefmt);

	statsinit();

	fix = 0;
	scan = 0;
	ARGBEGIN{
	case 'f':
		fix++;
		break;
	case 'a':
		scan = 1;
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

	file = argv[0];

	part = initpart(file, 0);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	ap = initarenapart(part);
	if(ap == nil)
		sysfatal("can't initialize arena partition in %s: %r", file);

	if(verbose > 1){
		printarenapart(2, ap);
		fprint(2, "\n");
	}

	initdcache(8 * MaxDiskBlock);

	for(i = 0; i < ap->narenas; i++)
		checkarena(ap->arenas[i], scan, fix);

	if(verbose > 1)
		printstats();
	threadexitsall(0);
}
