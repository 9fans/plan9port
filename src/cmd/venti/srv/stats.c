#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int collectstats = 1;

/* keep in sync with dat.h:/NStat */
Statdesc statdesc[NStat] =
{
	{ "rpc total", },
	{ "rpc reads", },
	{ "rpc reads ok", },
	{ "rpc reads failed", },
	{ "rpc read bytes", },
	{ "rpc read time", },
	{ "rpc read cached", },
	{ "rpc read cached time", },
	{ "rpc read uncached", },
	{ "rpc read uncached time "},

	{ "rpc writes", },
	{ "rpc writes new", },
	{ "rpc writes old", },
	{ "rpc writes failed", },
	{ "rpc write bytes", },
	{ "rpc write time", },
	{ "rpc write new time", },
	{ "rpc write old time", },

	{ "lump cache hits", },
	{ "lump cache misses", },
	{ "lump cache reads", },
	{ "lump cache writes", },
	{ "lump cache size", },
	{ "lump cache stall", },
	{ "lump cache read time", },

	{ "disk cache hits", },
	{ "disk cache misses", },
	{ "disk cache lookups", },
	{ "disk cache reads", },
	{ "disk cache writes", },
	{ "disk cache dirty", },
	{ "disk cache size", },
	{ "disk cache flushes", },
	{ "disk cache stalls", },
	{ "disk cache lookup time", },

	{ "disk block stalls", },
	{ "lump stalls", },

	{ "index cache hits", },
	{ "index cache misses", },
	{ "index cache reads", },
	{ "index cache writes", },
	{ "index cache fills", },
	{ "index cache prefetches", },
	{ "index cache dirty", },
	{ "index cache size", },
	{ "index cache flushes", },
	{ "index cache stalls", },
	{ "index cache read time", },
	{ "index cache lookups" },
	{ "index cache summary hits" },
	{ "index cache summary prefetches" },

	{ "bloom filter hits", },
	{ "bloom filter misses", },
	{ "bloom filter false misses", },
	{ "bloom filter lookups", },
	{ "bloom filter ones", },
	{ "bloom filter bits", },

	{ "arena block reads", },
	{ "arena block read bytes", },
	{ "arena block writes", },
	{ "arena block write bytes", },

	{ "isect block reads", },
	{ "isect block read bytes", },
	{ "isect block writes", },
	{ "isect block write bytes", },

	{ "sum reads", },
	{ "sum read bytes", },

	{ "cig loads" },
	{ "cig load time" },
};

QLock statslock;
Stats stats;
Stats *stathist;
int nstathist;
ulong statind;
ulong stattime;

void
statsproc(void *v)
{
	USED(v);

	for(;;){
		stats.now = time(0);
		stathist[stattime%nstathist] = stats;
		stattime++;
		sleep(1000);
	}
}

void
statsinit(void)
{
	nstathist = 90000;
	stathist = MKNZ(Stats, nstathist);
	vtproc(statsproc, nil);
}

void
setstat(int index, long val)
{
	qlock(&statslock);
	stats.n[index] = val;
	qunlock(&statslock);
}

void
addstat(int index, int inc)
{
	if(!collectstats)
		return;
	qlock(&statslock);
	stats.n[index] += inc;
	qunlock(&statslock);
}

void
addstat2(int index, int inc, int index1, int inc1)
{
	if(!collectstats)
		return;
	qlock(&statslock);
	stats.n[index] += inc;
	stats.n[index1] += inc1;
	qunlock(&statslock);
}

void
printstats(void)
{
}

void
binstats(long (*fn)(Stats *s0, Stats *s1, void *arg), void *arg,
	long t0, long t1, Statbin *bin, int nbin)
{
	long xt0, t, te, v;
	int i, j, lo, hi, m;
	vlong tot;
	Statbin *b;

	t = stats.now;

	/* negative times mean relative to now. */
	if(t0 <= 0)
		t0 += t;
	if(t1 <= 0)
		t1 += t;
	/* ten minute range if none given */
	if(t1 <= t0)
		t0 = t1 - 60*10;
	if(0) fprint(2, "stats %ld-%ld\n", t0, t1);

	/* binary search to find t0-1 or close */
	lo = stattime;
	hi = stattime+nstathist;
	while(lo+1 < hi){
		m = (lo+hi)/2;
		if(stathist[m%nstathist].now >= t0)
			hi = m;
		else
			lo = m;
	}
	xt0 = stathist[lo%nstathist].now;
	if(xt0 >= t1){
		/* no samples */
		memset(bin, 0, nbin*sizeof bin[0]);
		return;
	}

	hi = stattime+nstathist;
	j = lo+1;
	for(i=0; i<nbin; i++){
		te = t0 + (t1-t0)*i/nbin;
		b = &bin[i];
		memset(b, 0, sizeof *b);
		tot = 0;
		for(; j<hi && stathist[j%nstathist].now<te; j++){
			v = fn(&stathist[(j-1)%nstathist], &stathist[j%nstathist], arg);
			if(b->nsamp==0 || v < b->min)
				b->min = v;
			if(b->nsamp==0 || v > b->max)
				b->max = v;
			tot += v;
			b->nsamp++;
		}
		if(b->nsamp)
			b->avg = tot / b->nsamp;
		if(b->nsamp==0 && i>0)
			*b = bin[i-1];
	}
}
