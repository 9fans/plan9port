#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
printindex(int fd, Index *ix)
{
	int i;

	fprint(fd, "index=%s version=%d blocksize=%d tabsize=%d\n",
		ix->name, ix->version, ix->blocksize, ix->tabsize);
	fprint(fd, "\tbuckets=%d div=%d\n", ix->buckets, ix->div);
	for(i = 0; i < ix->nsects; i++)
		fprint(fd, "\tsect=%s for buckets [%lld,%lld)\n", ix->smap[i].name, ix->smap[i].start, ix->smap[i].stop);
	for(i = 0; i < ix->narenas; i++)
		fprint(fd, "\tarena=%s at [%lld,%lld)\n", ix->amap[i].name, ix->amap[i].start, ix->amap[i].stop);
}

void
printarenapart(int fd, ArenaPart *ap)
{
	int i;

	fprint(fd, "arena partition=%s\n\tversion=%d blocksize=%d arenas=%d\n\tsetbase=%d setsize=%d\n",
		ap->part->name, ap->version, ap->blocksize, ap->narenas, ap->tabbase, ap->tabsize);
	for(i = 0; i < ap->narenas; i++)
		fprint(fd, "\tarena=%s at [%lld,%lld)\n", ap->map[i].name, ap->map[i].start, ap->map[i].stop);
}

void
printarena(int fd, Arena *arena)
{
	fprint(fd, "arena='%s' [%lld,%lld)\n\tversion=%d created=%d modified=%d",
		arena->name, arena->base, arena->base + arena->size + 2 * arena->blocksize,
		arena->version, arena->ctime, arena->wtime);
	if(arena->memstats.sealed)
		fprint(2, " sealed\n");
	else
		fprint(2, "\n");
	if(scorecmp(zeroscore, arena->score) != 0)
		fprint(2, "\tscore=%V\n", arena->score);

	fprint(fd, "\tclumps=%,d compressed clumps=%,d data=%,lld compressed data=%,lld disk storage=%,lld\n",
		arena->memstats.clumps, arena->memstats.cclumps, arena->memstats.uncsize,
		arena->memstats.used - arena->memstats.clumps * ClumpSize,
		arena->memstats.used + arena->memstats.clumps * ClumpInfoSize);
}
