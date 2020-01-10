#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int	count[VtMaxLumpSize][VtMaxType];
Config conf;

enum
{
	ClumpChunks	= 32*1024
};

static int
readarenainfo(Arena *arena)
{
	ClumpInfo *ci, *cis;
	u32int clump;
	int i, n, ok;

	if(arena->memstats.clumps)
		fprint(2, "reading directory for arena=%s with %d entries\n", arena->name, arena->memstats.clumps);

	cis = MKN(ClumpInfo, ClumpChunks);
	ok = 0;
	for(clump = 0; clump < arena->memstats.clumps; clump += n){
		n = ClumpChunks;

		if(n > arena->memstats.clumps - clump)
			n = arena->memstats.clumps - clump;

		if((i=readclumpinfos(arena, clump, cis, n)) != n){
			seterr(EOk, "arena directory read failed %d not %d: %r", i, n);
			ok = -1;
			break;
		}

		for(i = 0; i < n; i++){
			ci = &cis[i];
			if(ci->type >= VtMaxType || ci->uncsize >= VtMaxLumpSize) {
				fprint(2, "bad clump: %d: type = %d: size = %d\n", clump+i, ci->type, ci->uncsize);
				continue;
			}
			count[ci->uncsize][ci->type]++;
		}
	}
	free(cis);
	if(ok < 0)
		return TWID32;
	return clump;
}

static void
clumpstats(Index *ix)
{
	int ok;
	ulong clumps, n;
	int i, j, t;

	ok = 0;
	clumps = 0;
	for(i = 0; i < ix->narenas; i++){
		n = readarenainfo(ix->arenas[i]);
		if(n == TWID32){
			ok = -1;
			break;
		}
		clumps += n;
	}

	if(ok < 0)
		return;

	print("clumps = %ld\n", clumps);
	for(i=0; i<VtMaxLumpSize; i++) {
		t = 0;
		for(j=0; j<VtMaxType; j++)
			t += count[i][j];
		if(t == 0)
			continue;
		print("%d\t%d", i, t);
		for(j=0; j<VtMaxType; j++)
			print("\t%d", count[i][j]);
		print("\n");
	}
}


void
usage(void)
{
	fprint(2, "usage: clumpstats [-B blockcachesize] config\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	u32int bcmem;

	bcmem = 0;

	ARGBEGIN{
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	default:
		usage();
		break;
	}ARGEND

	readonly = 1;

	if(argc != 1)
		usage();

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	clumpstats(mainindex);

	threadexitsall(0);
}
