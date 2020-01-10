#include "stdinc.h"
#include "dat.h"
#include "fns.h"

ulong lasttime[2];
int manualscheduling;
int l0quantum = 120;
int l1quantum = 120;
ulong lasticachechange;

void
disksched(void)
{
	int p, nwrite, nflush, ndirty, tdirty, toflush;
	ulong t;
	vlong cflush;
	Stats *prev;

	/*
	 * no locks because all the data accesses are atomic.
	 */
	t = time(0);
	if(manualscheduling){
		lasticachechange = t;
		return;
	}

	if(t-lasttime[0] < l0quantum){
		/* level-0 disk access going on */
		p = icachedirtyfrac();
		if(p < IcacheFrac*5/10){	/* can wait */
			icachesleeptime = SleepForever;
			lasticachechange = t;
		}else if(p > IcacheFrac*9/10){	/* can't wait */
			icachesleeptime = 0;
			lasticachechange = t;
		}else if(t-lasticachechange > 60){
			/* have minute worth of data for current rate */
			prev = &stathist[(stattime-60+nstathist)%nstathist];

			/* # entries written to index cache */
			nwrite = stats.n[StatIcacheWrite] - prev->n[StatIcacheWrite];

			/* # dirty entries in index cache */
			ndirty = stats.n[StatIcacheDirty] - prev->n[StatIcacheDirty];

			/* # entries flushed to disk */
			nflush = nwrite - ndirty;

			/* want to stay around 70% dirty */
			tdirty = (vlong)stats.n[StatIcacheSize]*700/1000;

			/* assume nflush*icachesleeptime is a constant */
			cflush = (vlong)nflush*(icachesleeptime+1);

			/* computer number entries to write in next minute */
			toflush = nwrite + (stats.n[StatIcacheDirty] - tdirty);

			/* schedule for  that many */
			if(toflush <= 0 || cflush/toflush > 100000)
				icachesleeptime = SleepForever;
			else
				icachesleeptime = cflush/toflush;
		}
		arenasumsleeptime = SleepForever;
		return;
	}
	if(t-lasttime[1] < l1quantum){
		/* level-1 disk access (icache flush) going on */
		icachesleeptime = 0;
		arenasumsleeptime = SleepForever;
		return;
	}
	/* no disk access going on - no holds barred*/
	icachesleeptime = 0;
	arenasumsleeptime = 0;
}

void
diskaccess(int level)
{
	if(level < 0 || level >= nelem(lasttime)){
		fprint(2, "bad level in diskaccess; caller=%#p\n",
			getcallerpc(&level));
		return;
	}
	lasttime[level] = time(0);
}
