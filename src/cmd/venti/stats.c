#include "stdinc.h"
#include "dat.h"
#include "fns.h"

Stats stats;

void
statsinit(void)
{
}

static int
percent(long v, long total)
{
	if(total == 0)
		total = 1;
	if(v < 1000*1000)
		return (v * 100) / total;
	total /= 100;
	if(total == 0)
		total = 1;
	return v / total;
}

void
printstats(void)
{
	fprint(2, "lump writes=%,ld\n", stats.lumpwrites);
	fprint(2, "lump reads=%,ld\n", stats.lumpreads);
	fprint(2, "lump cache read hits=%,ld\n", stats.lumphit);
	fprint(2, "lump cache read misses=%,ld\n", stats.lumpmiss);

	fprint(2, "clump disk writes=%,ld\n", stats.clumpwrites);
	fprint(2, "clump disk bytes written=%,lld\n", stats.clumpbwrites);
	fprint(2, "clump disk bytes compressed=%,lld\n", stats.clumpbcomp);
	fprint(2, "clump disk reads=%,ld\n", stats.clumpreads);
	fprint(2, "clump disk bytes read=%,lld\n", stats.clumpbreads);
	fprint(2, "clump disk bytes uncompressed=%,lld\n", stats.clumpbuncomp);

	fprint(2, "clump directory disk writes=%,ld\n", stats.ciwrites);
	fprint(2, "clump directory disk reads=%,ld\n", stats.cireads);

	fprint(2, "index disk writes=%,ld\n", stats.indexwrites);
	fprint(2, "index disk reads=%,ld\n", stats.indexreads);
	fprint(2, "index disk reads for modify=%,ld\n", stats.indexwreads);
	fprint(2, "index disk reads for allocation=%,ld\n", stats.indexareads);
	fprint(2, "index block splits=%,ld\n", stats.indexsplits);

	fprint(2, "index cache lookups=%,ld\n", stats.iclookups);
	fprint(2, "index cache hits=%,ld %d%%\n", stats.ichits,
		percent(stats.ichits, stats.iclookups));
	fprint(2, "index cache fills=%,ld %d%%\n", stats.icfills,
		percent(stats.icfills, stats.iclookups));
	fprint(2, "index cache inserts=%,ld\n", stats.icinserts);

	fprint(2, "disk cache hits=%,ld\n", stats.pchit);
	fprint(2, "disk cache misses=%,ld\n", stats.pcmiss);
	fprint(2, "disk cache reads=%,ld\n", stats.pcreads);
	fprint(2, "disk cache bytes read=%,lld\n", stats.pcbreads);

	fprint(2, "disk cache writes=%,ld\n", stats.dirtydblocks);
	fprint(2, "disk cache writes absorbed=%,ld %d%%\n", stats.absorbedwrites,
		percent(stats.absorbedwrites, stats.dirtydblocks));

	fprint(2, "disk writes=%,ld\n", stats.diskwrites);
	fprint(2, "disk bytes written=%,lld\n", stats.diskbwrites);
	fprint(2, "disk reads=%,ld\n", stats.diskreads);
	fprint(2, "disk bytes read=%,lld\n", stats.diskbreads);

}
