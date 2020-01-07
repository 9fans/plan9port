#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int icacheprefetch = 1;

typedef struct ICache ICache;
typedef struct IHash IHash;
typedef struct ISum ISum;

struct ICache
{
	QLock	lock;
	Rendez	full;
	IHash	*hash;
	IEntry	*entries;
	int		nentries;

	/*
	* gcc 4.3 inlines the pushfirst loop in initicache,
	* but the inliner incorrectly deduces that
	* icache.free.next has a constant value
	* throughout the loop.  (In fact, pushfirst
	* assigns to it as ie->prev->next.)
	* Marking it volatile should avoid this bug.
	* The speed of linked list operations is dwarfed
	* by the disk i/o anyway.
	*/
	volatile IEntry free;

	IEntry	clean;
	IEntry	dirty;
	u32int	maxdirty;
	u32int	ndirty;
	AState	as;

	ISum	**sum;
	int		nsum;
	IHash	*shash;
	IEntry	*sentries;
	int		nsentries;
};

static ICache icache;

/*
 * Hash table of IEntries
 */

struct IHash
{
	int bits;
	u32int size;
	IEntry **table;
};

static IHash*
mkihash(int size1)
{
	u32int size;
	int bits;
	IHash *ih;

	bits = 0;
	size = 1;
	while(size < size1){
		bits++;
		size <<= 1;
	}

	ih = vtmallocz(sizeof(IHash));
	ih->table = vtmallocz(size * sizeof(ih->table[0]));
	ih->bits = bits;
	ih->size = size;
	return ih;
}

static IEntry*
ihashlookup(IHash *ih, u8int score[VtScoreSize], int type)
{
	u32int h;
	IEntry *ie;

	h = hashbits(score, ih->bits);
	for(ie=ih->table[h]; ie; ie=ie->nexthash)
		if((type == -1 || type == ie->ia.type) && scorecmp(score, ie->score) == 0)
			return ie;
	return nil;
}

static void
ihashdelete(IHash *ih, IEntry *ie, char *what)
{
	u32int h;
	IEntry **l;

	h = hashbits(ie->score, ih->bits);
	for(l=&ih->table[h]; *l; l=&(*l)->nexthash)
		if(*l == ie){
			*l = ie->nexthash;
			return;
		}
	fprint(2, "warning: %s %V not found in ihashdelete\n", what, ie->score);
}

static void
ihashinsert(IHash *ih, IEntry *ie)
{
	u32int h;

	h = hashbits(ie->score, ih->bits);
	ie->nexthash = ih->table[h];
	ih->table[h] = ie;
}


/*
 * IEntry lists.
 */

static IEntry*
popout(IEntry *ie)
{
	if(ie->prev == nil && ie->next == nil)
		return ie;
	ie->prev->next = ie->next;
	ie->next->prev = ie->prev;
	ie->next = nil;
	ie->prev = nil;
	return ie;
}

static IEntry*
poplast(volatile IEntry *list)
{
	if(list->prev == list)
		return nil;
	return popout(list->prev);
}

static IEntry*
pushfirst(volatile IEntry *list, IEntry *ie)
{
	popout(ie);
	ie->prev = (IEntry*)list;
	ie->next = list->next;
	ie->prev->next = ie;
	ie->next->prev = ie;
	return ie;
}

/*
 * Arena summary cache.
 */
struct ISum
{
	QLock	lock;
	IEntry	*entries;
	int	nentries;
	int	loaded;
	u64int addr;
	u64int limit;
	Arena *arena;
	int g;
};

static ISum*
scachelookup(u64int addr)
{
	int i;
	ISum *s;

	for(i=0; i<icache.nsum; i++){
		s = icache.sum[i];
		if(s->addr <= addr && addr < s->limit){
			if(i > 0){
				memmove(icache.sum+1, icache.sum, i*sizeof icache.sum[0]);
				icache.sum[0] = s;
			}
			return s;
		}
	}
	return nil;
}

static void
sumclear(ISum *s)
{
	int i;

	for(i=0; i<s->nentries; i++)
		ihashdelete(icache.shash, &s->entries[i], "scache");
	s->nentries = 0;
	s->loaded = 0;
	s->addr = 0;
	s->limit = 0;
	s->arena = nil;
	s->g = 0;
}

static ISum*
scacheevict(void)
{
	ISum *s;
	int i;

	for(i=icache.nsum-1; i>=0; i--){
		s = icache.sum[i];
		if(canqlock(&s->lock)){
			if(i > 0){
				memmove(icache.sum+1, icache.sum, i*sizeof icache.sum[0]);
				icache.sum[0] = s;
			}
			sumclear(s);
			return s;
		}
	}
	return nil;
}

static void
scachehit(u64int addr)
{
	scachelookup(addr);	/* for move-to-front */
}

static void
scachesetup(ISum *s, u64int addr)
{
	u64int addr0, limit;
	int g;

	s->arena = amapitoag(mainindex, addr, &addr0, &limit, &g);
	s->addr = addr0;
	s->limit = limit;
	s->g = g;
}

static void
scacheload(ISum *s)
{
	int i, n;

	s->loaded = 1;
	n = asumload(s->arena, s->g, s->entries, ArenaCIGSize);
	/*
	 * n can be less then ArenaCIGSize, either if the clump group
	 * is the last in the arena and is only partially filled, or if there
	 * are corrupt clumps in the group -- those are not returned.
	 */
	for(i=0; i<n; i++){
		s->entries[i].ia.addr += s->addr;
		ihashinsert(icache.shash, &s->entries[i]);
	}
//fprint(2, "%T scacheload %s %d - %d entries\n", s->arena->name, s->g, n);
	addstat(StatScachePrefetch, n);
	s->nentries = n;
}

static ISum*
scachemiss(u64int addr)
{
	ISum *s;

	if(!icacheprefetch)
		return nil;
	s = scachelookup(addr);
	if(s == nil){
		/* first time: make an entry in the cache but don't populate it yet */
		s = scacheevict();
		if(s == nil)
			return nil;
		scachesetup(s, addr);
		qunlock(&s->lock);
		return nil;
	}

	/* second time: load from disk */
	qlock(&s->lock);
	if(s->loaded || !icacheprefetch){
		qunlock(&s->lock);
		return nil;
	}

	return s;	/* locked */
}

/*
 * Index cache.
 */

void
initicache(u32int mem0)
{
	u32int mem;
	int i, entries, scache;

	icache.full.l = &icache.lock;

	mem = mem0;
	entries = mem / (sizeof(IEntry)+sizeof(IEntry*));
	scache = (entries/8) / ArenaCIGSize;
	entries -= entries/8;
	if(scache < 4)
		scache = 4;
	if(scache > 16)
		scache = 16;
	if(entries < 1000)
		entries = 1000;
fprint(2, "icache %,d bytes = %,d entries; %d scache\n", mem0, entries, scache);

	icache.clean.prev = icache.clean.next = &icache.clean;
	icache.dirty.prev = icache.dirty.next = &icache.dirty;
	icache.free.prev = icache.free.next = (IEntry*)&icache.free;

	icache.hash = mkihash(entries);
	icache.nentries = entries;
	setstat(StatIcacheSize, entries);
	icache.entries = vtmallocz(entries*sizeof icache.entries[0]);
	icache.maxdirty = entries / 2;
	for(i=0; i<entries; i++)
		pushfirst(&icache.free, &icache.entries[i]);

	icache.nsum = scache;
	icache.sum = vtmallocz(scache*sizeof icache.sum[0]);
	icache.sum[0] = vtmallocz(scache*sizeof icache.sum[0][0]);
	icache.nsentries = scache * ArenaCIGSize;
	icache.sentries = vtmallocz(scache*ArenaCIGSize*sizeof icache.sentries[0]);
	icache.shash = mkihash(scache*ArenaCIGSize);
	for(i=0; i<scache; i++){
		icache.sum[i] = icache.sum[0] + i;
		icache.sum[i]->entries = icache.sentries + i*ArenaCIGSize;
	}
}


static IEntry*
evictlru(void)
{
	IEntry *ie;

	ie = poplast(&icache.clean);
	if(ie == nil)
		return nil;
	ihashdelete(icache.hash, ie, "evictlru");
	return ie;
}

static void
icacheinsert(u8int score[VtScoreSize], IAddr *ia, int state)
{
	IEntry *ie;

	if((ie = poplast(&icache.free)) == nil && (ie = evictlru()) == nil){
		addstat(StatIcacheStall, 1);
		while((ie = poplast(&icache.free)) == nil && (ie = evictlru()) == nil){
			// Could safely return here if state == IEClean.
			// But if state == IEDirty, have to wait to make
			// sure we don't lose an index write.
			// Let's wait all the time.
			flushdcache();
			kickicache();
			rsleep(&icache.full);
		}
		addstat(StatIcacheStall, -1);
	}

	memmove(ie->score, score, VtScoreSize);
	ie->state = state;
	ie->ia = *ia;
	if(state == IEClean){
		addstat(StatIcachePrefetch, 1);
		pushfirst(&icache.clean, ie);
	}else{
		addstat(StatIcacheWrite, 1);
		assert(state == IEDirty);
		icache.ndirty++;
		setstat(StatIcacheDirty, icache.ndirty);
		delaykickicache();
		pushfirst(&icache.dirty, ie);
	}
	ihashinsert(icache.hash, ie);
}

int
icachelookup(u8int score[VtScoreSize], int type, IAddr *ia)
{
	IEntry *ie;

	if(bootstrap)
		return -1;

	qlock(&icache.lock);
	addstat(StatIcacheLookup, 1);
	if((ie = ihashlookup(icache.hash, score, type)) != nil){
		*ia = ie->ia;
		if(ie->state == IEClean)
			pushfirst(&icache.clean, ie);
		addstat(StatIcacheHit, 1);
		qunlock(&icache.lock);
		return 0;
	}

	if((ie = ihashlookup(icache.shash, score, type)) != nil){
		*ia = ie->ia;
		icacheinsert(score, &ie->ia, IEClean);
		scachehit(ie->ia.addr);
		addstat(StatScacheHit, 1);
		qunlock(&icache.lock);
		return 0;
	}
	addstat(StatIcacheMiss, 1);
	qunlock(&icache.lock);

	return -1;
}

int
insertscore(u8int score[VtScoreSize], IAddr *ia, int state, AState *as)
{
	ISum *toload;

	if(bootstrap)
		return -1;

	qlock(&icache.lock);
	icacheinsert(score, ia, state);
	if(state == IEClean)
		toload = scachemiss(ia->addr);
	else{
		assert(state == IEDirty);
		toload = nil;
		if(as == nil)
			fprint(2, "%T insertscore IEDirty without as; called from %#p\n",
				getcallerpc(&score));
		else{
			if(icache.as.aa > as->aa)
				fprint(2, "%T insertscore: aa moving backward: %#llux -> %#llux\n", icache.as.aa, as->aa);
			icache.as = *as;
		}
	}
	qunlock(&icache.lock);
	if(toload){
		scacheload(toload);
		qunlock(&toload->lock);
	}

	if(icache.ndirty >= icache.maxdirty)
		kickicache();

	/*
	 * It's okay not to do this under icache.lock.
	 * Calling insertscore only happens when we hold
	 * the lump, meaning any searches for this block
	 * will hit in the lump cache until after we return.
	 */
	if(state == IEDirty)
		markbloomfilter(mainindex->bloom, score);

	return 0;
}

int
lookupscore(u8int score[VtScoreSize], int type, IAddr *ia)
{
	int ms, ret;
	IEntry d;

	if(icachelookup(score, type, ia) >= 0){
		addstat(StatIcacheRead, 1);
		return 0;
	}

	ms = msec();
	addstat(StatIcacheFill, 1);
	if(loadientry(mainindex, score, type, &d) < 0)
		ret = -1;
	else{
		ret = 0;
		insertscore(score, &d.ia, IEClean, nil);
		*ia = d.ia;
	}
	addstat2(StatIcacheRead, 1, StatIcacheReadTime, msec() - ms);
	return ret;
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

ulong
icachedirtyfrac(void)
{
	return (vlong)icache.ndirty*IcacheFrac / icache.nentries;
}

/*
 * Return a singly-linked list of dirty index entries.
 * with 32-bit hash numbers between lo and hi
 * and address < limit.
 */
IEntry*
icachedirty(u32int lo, u32int hi, u64int limit)
{
	u32int h;
	IEntry *ie, *dirty;

	dirty = nil;
	trace(TraceProc, "icachedirty enter");
	qlock(&icache.lock);
	for(ie = icache.dirty.next; ie != &icache.dirty; ie=ie->next){
		if(ie->state == IEDirty && ie->ia.addr <= limit){
			h = hashbits(ie->score, 32);
			if(lo <= h && h <= hi){
				ie->nextdirty = dirty;
				dirty = ie;
			}
		}
	}
	qunlock(&icache.lock);
	trace(TraceProc, "icachedirty exit");
	if(dirty == nil)
		flushdcache();
	return dirty;
}

AState
icachestate(void)
{
	AState as;

	qlock(&icache.lock);
	as = icache.as;
	qunlock(&icache.lock);
	return as;
}

/*
 * The singly-linked non-circular list of index entries ie
 * has been written to disk.  Move them to the clean list.
 */
void
icacheclean(IEntry *ie)
{
	IEntry *next;

	trace(TraceProc, "icacheclean enter");
	qlock(&icache.lock);
	for(; ie; ie=next){
		assert(ie->state == IEDirty);
		next = ie->nextdirty;
		ie->nextdirty = nil;
		popout(ie); /* from icache.dirty */
		icache.ndirty--;
		ie->state = IEClean;
		pushfirst(&icache.clean, ie);
	}
	setstat(StatIcacheDirty, icache.ndirty);
	rwakeupall(&icache.full);
	qunlock(&icache.lock);
	trace(TraceProc, "icacheclean exit");
}

void
emptyicache(void)
{
	int i;
	IEntry *ie;
	ISum *s;

	qlock(&icache.lock);
	while((ie = evictlru()) != nil)
		pushfirst(&icache.free, ie);
	for(i=0; i<icache.nsum; i++){
		s = icache.sum[i];
		qlock(&s->lock);
		sumclear(s);
		qunlock(&s->lock);
	}
	qunlock(&icache.lock);
}
