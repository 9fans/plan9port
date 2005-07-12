#include "stdinc.h"
#include "dat.h"
#include "fns.h"

typedef struct ICache ICache;
struct ICache
{
	QLock	lock;			/* locks hash table & all associated data */
	Rendez	full;
	IEntry	**heads;		/* heads of all the hash chains */
	int	bits;			/* bits to use for indexing heads */
	u32int	size;			/* number of heads; == 1 << bits, should be < entries */
	IEntry	*base;			/* all allocated hash table entries */
	u32int	entries;		/* elements in base */
	IEntry	*dirty;		/* chain of dirty elements */
	u32int	ndirty;
	u32int	maxdirty;
	u32int	unused;			/* index of first unused element in base */
	u32int	stolen;			/* last head from which an element was stolen */

	Arena	*last[4];
	Arena	*lastload;
	int		nlast;
};

static ICache icache;

static IEntry	*icachealloc(IAddr *ia, u8int *score);

/*
 * bits is the number of bits in the icache hash table
 * depth is the average depth
 * memory usage is about (1<<bits) * depth * sizeof(IEntry) + (1<<bits) * sizeof(IEntry*)
 */
void
initicache(int bits, int depth)
{
	icache.bits = bits;
	icache.size = 1 << bits;
	icache.entries = depth * icache.size;
	icache.maxdirty = icache.entries/2;
	icache.base = MKNZ(IEntry, icache.entries);
	icache.heads = MKNZ(IEntry*, icache.size);
	icache.full.l = &icache.lock;
	setstat(StatIcacheSize, icache.entries);
}

u32int
hashbits(u8int *sc, int bits)
{
	u32int v;

	v = (sc[0] << 24) | (sc[1] << 16) | (sc[2] << 8) | sc[3];
	if(bits < 32)
		 v >>= (32 - bits);
	return v;
}

static void
loadarenaclumps(Arena *arena, u64int aa)
{
	ulong i;
	ClumpInfo ci;
	IAddr ia;

fprint(2, "seed index cache with arena @%llud, (map %llud), %d clumps\n", arena->base, aa, arena->memstats.clumps);
	for(i=0; i<arena->memstats.clumps; i++){
		if(readclumpinfo(arena, i, &ci) < 0)
			break;
		ia.type = ci.type;
		ia.size = ci.uncsize;
		ia.blocks = (ci.size + ClumpSize + (1 << ABlockLog) - 1) >> ABlockLog;
		ia.addr = aa;
		aa += ClumpSize + ci.size;
		if(ia.type != VtCorruptType)
			insertscore(ci.score, &ia, 0);
	}
}

/*
ZZZ need to think about evicting the correct IEntry,
and writing back the wtime.
 * look up data score in the index cache
 * if this fails, pull it in from the disk index table, if it exists.
 *
 * must be called with the lump for this score locked
 */
int
lookupscore(u8int *score, int type, IAddr *ia, int *rac)
{
	IEntry d, *ie, *last;
	u32int h;
	u64int aa;
	Arena *load;
	int i;
	uint ms;

	load = nil;
	aa = 0;
	ms = msec();
	
	trace(TraceLump, "lookupscore %V.%d", score, type);

	qlock(&icache.lock);
	h = hashbits(score, icache.bits);
	last = nil;
	for(ie = icache.heads[h]; ie != nil; ie = ie->next){
		if(ie->ia.type == type && scorecmp(ie->score, score)==0){
			if(last != nil)
				last->next = ie->next;
			else
				icache.heads[h] = ie->next;
			addstat(StatIcacheHit, 1);
			ie->rac = 1;
			trace(TraceLump, "lookupscore incache");
			goto found;
		}
		last = ie;
	}
	addstat(StatIcacheMiss, 1);
	qunlock(&icache.lock);

	if(loadientry(mainindex, score, type, &d) < 0){
		ms = msec() - ms;
		addstat2(StatIcacheRead, 1, StatIcacheReadTime, ms);
		return -1;
	}

	addstat(StatIcacheFill, 1);

	trace(TraceLump, "lookupscore loaded");

	/*
	 * no one else can load an entry for this score,
	 * since we have the overall score lock.
	 */
	qlock(&icache.lock);

	/*
	 * If we notice that all the hits are coming from one arena,
	 * load the table of contents for that arena into the cache.
	 */
	ie = icachealloc(&d.ia, score);
	icache.last[icache.nlast++%nelem(icache.last)] = amapitoa(mainindex, ie->ia.addr, &aa);
	aa = ie->ia.addr - aa;	/* compute base addr of arena */
	for(i=0; i<nelem(icache.last); i++)
		if(icache.last[i] != icache.last[0])
			break;
	if(i==nelem(icache.last) && icache.lastload != icache.last[0]){
		load = icache.last[0];
		icache.lastload = load;
	}

found:
	ie->next = icache.heads[h];
	icache.heads[h] = ie;

	*ia = ie->ia;
	*rac = ie->rac;

	qunlock(&icache.lock);

	if(load){
		trace(TraceProc, "preload 0x%llux", aa);
		loadarenaclumps(load, aa);
	}
	ms = msec() - ms;
	addstat2(StatIcacheRead, 1, StatIcacheReadTime, ms);

	return 0;
}

/*
 * insert a new element in the hash table.
 */
int
insertscore(u8int *score, IAddr *ia, int write)
{
	IEntry *ie, se;
	u32int h;

	trace(TraceLump, "insertscore enter");
	if(write)
		addstat(StatIcacheWrite, 1);
	else
		addstat(StatIcachePrefetch, 1);

	qlock(&icache.lock);
	h = hashbits(score, icache.bits);

	ie = icachealloc(ia, score);
	if(write){
		icache.ndirty++;
		setstat(StatIcacheDirty, icache.ndirty);
		delaykickicache();
		ie->dirty = 1;
	}
	ie->next = icache.heads[h];
	icache.heads[h] = ie;

	se = *ie;
	qunlock(&icache.lock);

	if(write && icache.ndirty >= icache.maxdirty)
		kickicache();

	/*
	 * It's okay not to do this under icache.lock.
	 * Calling insertscore only happens when we hold
	 * the lump, meaning any searches for this block
	 * will hit in the lump cache until after we return.
	 */
	markbloomfilter(mainindex->bloom, score);

	return 0;
}

/*
 * allocate a index cache entry which hasn't been used in a while.
 * must be called with icache.lock locked
 * if the score is already in the table, update the entry.
 */
static IEntry *
icachealloc(IAddr *ia, u8int *score)
{
	int i;
	IEntry *ie, *last, *clean, *lastclean;
	u32int h;

	h = hashbits(score, icache.bits);
	last = nil;
	for(ie = icache.heads[h]; ie != nil; ie = ie->next){
		if(ie->ia.type == ia->type && scorecmp(ie->score, score)==0){
			if(last != nil)
				last->next = ie->next;
			else
				icache.heads[h] = ie->next;
			trace(TraceLump, "icachealloc hit");
			ie->rac = 1;
			return ie;
		}
		last = ie;
	}

	h = icache.unused;
	if(h < icache.entries){
		ie = &icache.base[h++];
		icache.unused = h;
		trace(TraceLump, "icachealloc unused");
		goto Found;
	}

	h = icache.stolen;
	for(i=0;; i++){
		h++;
		if(h >= icache.size)
			h = 0;
		if(i == icache.size){
			trace(TraceLump, "icachealloc sleep");
			addstat(StatIcacheStall, 1);
			while(icache.ndirty == icache.entries){
				/*
				 * This is a bit suspect.  Kickicache will wake up the
				 * icachewritecoord, but if all the index entries are for
				 * unflushed disk blocks, icachewritecoord won't be
				 * able to do much.  It always rewakes everyone when
				 * it thinks it is done, though, so at least we'll go around
				 * the while loop again.  Also, if icachewritecoord sees
				 * that the disk state hasn't change at all since the last
				 * time around, it kicks the disk.  This needs to be
				 * rethought, but it shouldn't deadlock anymore.
				 */
				kickicache();
				rsleep(&icache.full);
			}
			addstat(StatIcacheStall, -1);
			i = 0;
		}
		lastclean = nil;
		clean = nil;
		last = nil;
		for(ie=icache.heads[h]; ie; last=ie, ie=ie->next){
			if(!ie->dirty){
				clean = ie;
				lastclean = last;
			}
		}
		if(clean){
			if(lastclean)
				lastclean->next = clean->next;
			else
				icache.heads[h] = clean->next;
			clean->next = nil;
			icache.stolen = h;
			ie = clean;
			trace(TraceLump, "icachealloc steal");
			goto Found;
		}
	}

Found:
	ie->ia = *ia;
	scorecp(ie->score, score);
	ie->rac = 0;	
	return ie;
}

IEntry*
icachedirty(u32int lo, u32int hi, u64int limit)
{
	int i;
	u32int h;
	IEntry *ie, *dirty;

	dirty = nil;
	trace(TraceProc, "icachedirty enter");
	qlock(&icache.lock);
	for(i=0; i<icache.size; i++)
	for(ie = icache.heads[i]; ie; ie=ie->next)
		if(ie->dirty && ie->ia.addr != 0 && ie->ia.addr < limit){
			h = hashbits(ie->score, 32);
			if(lo <= h && h <= hi){
				ie->nextdirty = dirty;
				dirty = ie;
			}
		}
	qunlock(&icache.lock);
	trace(TraceProc, "icachedirty exit");
	if(dirty == nil)
		flushdcache();
	return dirty;
}

void
icacheclean(IEntry *ie)
{
	trace(TraceProc, "icachedirty enter");
	qlock(&icache.lock);
	for(; ie; ie=ie->nextdirty){
		icache.ndirty--;
		ie->dirty = 0;
	}
	setstat(StatIcacheDirty, icache.ndirty);
	rwakeupall(&icache.full);
	qunlock(&icache.lock);
	trace(TraceProc, "icachedirty exit");
}

