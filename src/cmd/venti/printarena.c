#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int readonly = 1;	/* for part.c */

void
usage(void)
{
	fprint(2, "usage: printarena arenafile [offset]\n");
	threadexitsall("usage");
}

static void
rdarena(Arena *arena, u64int offset)
{
	u64int a, aa, e;
	u32int magic;
	Clump cl;
	uchar score[VtScoreSize];
	ZBlock *lump;

	printarena(2, arena);

	a = arena->base;
	e = arena->base + arena->size;
	if(offset != ~(u64int)0) {
		if(offset >= e-a)
			sysfatal("bad offset %llud >= %llud\n",
				offset, e-a);
		aa = offset;
	} else
		aa = 0;

	for(; aa < e; aa += ClumpSize+cl.info.size) {
		magic = clumpmagic(arena, aa);
		if(magic == ClumpFreeMagic)
			break;
		if(magic != ClumpMagic) {
			fprint(2, "illegal clump magic number %#8.8ux offset %llud\n",
				magic, aa);
			break;
		}
		lump = loadclump(arena, aa, 0, &cl, score, 0);
		if(lump == nil) {
			fprint(2, "clump %llud failed to read: %r\n", aa);
			break;
		}
		if(cl.info.type != VtTypeCorrupt) {
			scoremem(score, lump->data, cl.info.uncsize);
			if(scorecmp(cl.info.score, score) != 0) {
				fprint(2, "clump %llud has mismatched score\n", aa);
				break;
			}
			if(vttypevalid(cl.info.type) < 0) {
				fprint(2, "clump %llud has bad type %d\n", aa, cl.info.type);
				break;
			}
		}
		print("%llud %V %d %d\n", aa, score, cl.info.type, cl.info.uncsize);
		freezblock(lump);
	}
	print("end offset %llud\n", aa);
}

void
threadmain(int argc, char *argv[])
{
	char *file;
	Arena *arena;
	u64int offset, aoffset;
	Part *part;
	Dir *d;
	uchar buf[8192];
	ArenaHead head;

	aoffset = 0;
	ARGBEGIN{
	case 'o':
		aoffset = strtoull(EARGF(usage()), 0, 0);
		break;
	default:
		usage();
		break;
	}ARGEND

	offset = ~(u64int)0;
	switch(argc) {
	default:
		usage();
	case 2:
		offset = strtoull(argv[1], 0, 0);
		/* fall through */
	case 1:
		file = argv[0];
	}


	fmtinstall('V', vtscorefmt);

	statsinit();

	if((d = dirstat(file)) == nil)
		sysfatal("can't stat file %s: %r", file);

	part = initpart(file, 0);
	if(part == nil)
		sysfatal("can't open file %s: %r", file);
	if(readpart(part, aoffset, buf, sizeof buf) < 0)
		sysfatal("can't read file %s: %r", file);

	if(unpackarenahead(&head, buf) < 0)
		sysfatal("corrupted arena header: %r");

	if(aoffset+head.size > d->length)
		sysfatal("arena is truncated: want %llud bytes have %llud\n",
			head.size, d->length);

	partblocksize(part, head.blocksize);
	initdcache(8 * MaxDiskBlock);

	arena = initarena(part, aoffset, head.size, head.blocksize);
	if(arena == nil)
		sysfatal("initarena: %r");

	rdarena(arena, offset);
	threadexitsall(0);
}
