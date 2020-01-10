#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int verbose, quiet;

void
usage(void)
{
	fprint(2, "usage: rdarena [-qv] arenapart arena\n");
	threadexitsall(0);
}

static void
rdarena(Arena *arena)
{
	ZBlock *b;
	u64int a, e;
	u32int bs;

	if (!quiet) {
		fprint(2, "copying %s to standard output\n", arena->name);
		printarena(2, arena);
	}

	bs = MaxIoSize;
	if(bs < arena->blocksize)
		bs = arena->blocksize;

	b = alloczblock(bs, 0, arena->blocksize);
	e = arena->base + arena->size + arena->blocksize;
	for(a = arena->base - arena->blocksize; a + arena->blocksize <= e; a += bs){
		if(a + bs > e)
			bs = arena->blocksize;
		if(readpart(arena->part, a, b->data, bs) < 0)
			fprint(2, "can't copy %s, read at %lld failed: %r\n", arena->name, a);
		if(write(1, b->data, bs) != bs)
			sysfatal("can't copy %s, write at %lld failed: %r", arena->name, a);
	}

	freezblock(b);
}

void
threadmain(int argc, char *argv[])
{
	ArenaPart *ap;
	Part *part;
	char *file, *aname;
	int i;

	ventifmtinstall();
	statsinit();

	ARGBEGIN{
	case 'q':
		quiet++;
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	readonly = 1;

	if(argc != 2)
		usage();

	file = argv[0];
	aname = argv[1];

	part = initpart(file, OREAD|ODIRECT);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	ap = initarenapart(part);
	if(ap == nil)
		sysfatal("can't initialize arena partition in %s: %r", file);

	if(verbose)
		printarenapart(2, ap);

	initdcache(8 * MaxDiskBlock);

	for(i = 0; i < ap->narenas; i++){
		if(strcmp(ap->arenas[i]->name, aname) == 0){
			rdarena(ap->arenas[i]);
			threadexitsall(0);
		}
	}

	sysfatal("couldn't find arena %s", aname);
}
