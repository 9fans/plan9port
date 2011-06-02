#include "stdinc.h"
#include "dat.h"
#include "fns.h"

enum
{
	ClumpChunks	= 32*1024
};

static int	verbose;

int
clumpinfoeq(ClumpInfo *c, ClumpInfo *d)
{
	return c->type == d->type
		&& c->size == d->size
		&& c->uncsize == d->uncsize
		&& scorecmp(c->score, d->score)==0;
}

int
findscore(Arena *arena, uchar *score)
{
	IEntry ie;
	ClumpInfo *ci, *cis;
	u64int a;
	u32int clump;
	int i, n, found;

//ZZZ remove fprint?
	if(arena->memstats.clumps)
		fprint(2, "reading directory for arena=%s with %d entries\n",
			arena->name, arena->memstats.clumps);

	cis = MKN(ClumpInfo, ClumpChunks);
	found = 0;
	a = 0;
	memset(&ie, 0, sizeof(IEntry));
	for(clump = 0; clump < arena->memstats.clumps; clump += n){
		n = ClumpChunks;
		if(n > arena->memstats.clumps - clump)
			n = arena->memstats.clumps - clump;
		if(readclumpinfos(arena, clump, cis, n) != n){
			seterr(EOk, "arena directory read failed: %r");
			break;
		}

		for(i = 0; i < n; i++){
			ci = &cis[i];
			if(scorecmp(score, ci->score)==0){
				fprint(2, "found at clump=%d with type=%d size=%d csize=%d position=%lld\n",
					clump + i, ci->type, ci->uncsize, ci->size, a);
				found++;
			}
			a += ci->size + ClumpSize;
		}
	}
	free(cis);
	return found;
}

void
usage(void)
{
	fprint(2, "usage: findscore [-v] arenafile score\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	ArenaPart *ap;
	Part *part;
	char *file;
	u8int score[VtScoreSize];
	int i, found;

	ventifmtinstall();

	ARGBEGIN{
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
	if(strscore(argv[1], score) < 0)
		sysfatal("bad score %s", argv[1]);

	part = initpart(file, OREAD|ODIRECT);
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

	found = 0;
	for(i = 0; i < ap->narenas; i++)
		found += findscore(ap->arenas[i], score);

	print("found %d occurrences of %V\n", found, score);

	if(verbose > 1)
		printstats();
	threadexitsall(0);
}
