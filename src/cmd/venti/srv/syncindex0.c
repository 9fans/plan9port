#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int
syncarenaindex(Arena *arena, u64int a0)
{
	int ok;
	u32int clump;
	u64int a;
	ClumpInfo ci;
	IAddr ia;
	AState as;

	if(arena->diskstats.clumps == arena->memstats.clumps)
		return 0;

	memset(&as, 0, sizeof as);
	as.arena = arena;
	as.stats = arena->diskstats;

	ok = 0;
	a = a0 + arena->diskstats.used;
	for(clump=arena->diskstats.clumps; clump < arena->memstats.clumps; clump++){
		if(readclumpinfo(arena, clump, &ci) < 0){
			fprint(2, "%s: clump %d: cannot read clumpinfo\n",
				arena->name, clump);
			ok = -1;
			break;
		}

		ia.type = ci.type;
		ia.size = ci.uncsize;
		ia.addr = a;
		ia.blocks = (ClumpSize + ci.size + (1 << ABlockLog) - 1) >> ABlockLog;
		a += ClumpSize + ci.size;

		as.stats.used += ClumpSize + ci.size;
		as.stats.uncsize += ia.size;
		as.stats.clumps++;
		if(ci.uncsize > ci.size)
			as.stats.cclumps++;
		as.aa = a;
		insertscore(ci.score, &ia, IEDirty, &as);
	}
	flushdcache();
	return ok;
}

int
syncindex(Index *ix)
{
	Arena *arena;
	int i, e, e1, ok;

	ok = 0;
	for(i = 0; i < ix->narenas; i++){
		trace(TraceProc, "syncindex start %d", i);
		arena = ix->arenas[i];
		e = syncarena(arena, TWID32, 1, 1);
		e1 = e;
		e1 &= ~(SyncHeader|SyncCIZero|SyncCIErr);
		if(e & SyncHeader)
			fprint(2, "arena %s: header is out-of-date\n", arena->name);
		if(e1){
			fprint(2, "arena %s: %x\n", arena->name, e1);
			ok = -1;
			continue;
		}
		flushdcache();

		if(arena->memstats.clumps == arena->diskstats.clumps)
			continue;

		fprint(2, "%T %s: indexing %d clumps...\n",
			arena->name,
			arena->memstats.clumps - arena->diskstats.clumps);

		if(syncarenaindex(arena, ix->amap[i].start) < 0){
			fprint(2, "arena %s: syncarenaindex: %r\n", arena->name);
			ok = -1;
			continue;
		}
		if(wbarena(arena) < 0){
			fprint(2, "arena %s: wbarena: %r\n", arena->name);
			ok = -1;
			continue;
		}
		flushdcache();
		delaykickicache();
	}
	return ok;
}
