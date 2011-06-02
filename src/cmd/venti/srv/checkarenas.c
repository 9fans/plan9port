#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	verbose;

static void
checkarena(Arena *arena, int scan, int fix)
{
	ATailStats old;
	int err, e;

	if(verbose && arena->memstats.clumps)
		printarena(2, arena);

	old = arena->memstats;

	if(scan){
		arena->memstats.used = 0;
		arena->memstats.clumps = 0;
		arena->memstats.cclumps = 0;
		arena->memstats.uncsize = 0;
	}

	err = 0;
	for(;;){
		e = syncarena(arena, 1000, 0, fix);
		err |= e;
		if(!(e & SyncHeader))
			break;
		if(verbose && arena->memstats.clumps)
			fprint(2, ".");
	}
	if(verbose && arena->memstats.clumps)
		fprint(2, "\n");

	err &= ~SyncHeader;
	if(arena->memstats.used != old.used
	|| arena->memstats.clumps != old.clumps
	|| arena->memstats.cclumps != old.cclumps
	|| arena->memstats.uncsize != old.uncsize){
		fprint(2, "%s: incorrect arena header fields\n", arena->name);
		printarena(2, arena);
		err |= SyncHeader;
	}

	if(!err || !fix)
		return;

	fprint(2, "%s: writing fixed arena header fields\n", arena->name);
	arena->diskstats = arena->memstats;
	if(wbarena(arena) < 0)
		fprint(2, "arena header write failed: %r\n");
	flushdcache();
}

void
usage(void)
{
	fprint(2, "usage: checkarenas [-afv] file [arenaname...]\n");
	threadexitsall(0);
}

int
should(char *name, int argc, char **argv)
{
	int i;

	if(argc == 0)
		return 1;
	for(i=0; i<argc; i++)
		if(strcmp(name, argv[i]) == 0)
			return 1;
	return 0;
}

void
threadmain(int argc, char *argv[])
{
	ArenaPart *ap;
	Part *part;
	char *file;
	int i, fix, scan;

	ventifmtinstall();
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

	if(argc < 1)
		usage();

	file = argv[0];
	argc--;
	argv++;

	part = initpart(file, (fix ? ORDWR : OREAD)|ODIRECT);
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
		if(should(ap->arenas[i]->name, argc, argv)) {
			debugarena = i;
			checkarena(ap->arenas[i], scan, fix);
		}

	if(verbose > 1)
		printstats();
	threadexitsall(0);
}
