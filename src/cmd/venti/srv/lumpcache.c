#include "stdinc.h"
#include "dat.h"
#include "fns.h"

/* #define CHECK(x)	x */
#define CHECK(x)

typedef struct LumpCache	LumpCache;

enum
{
	HashLog		= 9,
	HashSize	= 1<<HashLog,
	HashMask	= HashSize - 1,
};

struct LumpCache
{
	QLock		lock;
	Rendez		full;
	Lump		*free;			/* list of available lumps */
	u32int		allowed;		/* total allowable space for packets */
	u32int		avail;			/* remaining space for packets */
	u32int		now;			/* ticks for usage timestamps */
	Lump		**heads;		/* hash table for finding address */
	int		nheap;			/* number of available victims */
	Lump		**heap;			/* heap for locating victims */
	int		nblocks;		/* number of blocks allocated */
	Lump		*blocks;		/* array of block descriptors */
};

static LumpCache	lumpcache;

static void	delheap(Lump *db);
static int	downheap(int i, Lump *b);
static void	fixheap(int i, Lump *b);
static int	upheap(int i, Lump *b);
static Lump	*bumplump(void);

void
initlumpcache(u32int size, u32int nblocks)
{
	Lump *last, *b;
	int i;

	lumpcache.full.l = &lumpcache.lock;
	lumpcache.nblocks = nblocks;
	lumpcache.allowed = size;
	lumpcache.avail = size;
	lumpcache.heads = MKNZ(Lump*, HashSize);
	lumpcache.heap = MKNZ(Lump*, nblocks);
	lumpcache.blocks = MKNZ(Lump, nblocks);
	setstat(StatLcacheSize, lumpcache.nblocks);

	last = nil;
	for(i = 0; i < nblocks; i++){
		b = &lumpcache.blocks[i];
		b->type = TWID8;
		b->heap = TWID32;
		b->next = last;
		last = b;
	}
	lumpcache.free = last;
	lumpcache.nheap = 0;
}

Lump*
lookuplump(u8int *score, int type)
{
	uint ms;
	Lump *b;
	u32int h;

	ms = 0;
	trace(TraceLump, "lookuplump enter");

	h = hashbits(score, HashLog);

	/*
	 * look for the block in the cache
	 */
	qlock(&lumpcache.lock);
	CHECK(checklumpcache());
again:
	for(b = lumpcache.heads[h]; b != nil; b = b->next){
		if(scorecmp(score, b->score)==0 && type == b->type){
			addstat(StatLcacheHit, 1);
			trace(TraceLump, "lookuplump hit");
			goto found;
		}
	}

	trace(TraceLump, "lookuplump miss");

	/*
	 * missed: locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	while(lumpcache.free == nil){
		trace(TraceLump, "lookuplump bump");
		CHECK(checklumpcache());
		if(bumplump() == nil){
			CHECK(checklumpcache());
			logerr(EAdmin, "all lump cache blocks in use");
			addstat(StatLcacheStall, 1);
			CHECK(checklumpcache());
			rsleep(&lumpcache.full);
			CHECK(checklumpcache());
			addstat(StatLcacheStall, -1);
			goto again;
		}
		CHECK(checklumpcache());
	}

	/* start timer on cache miss to avoid system call on cache hit */
	ms = msec();

	addstat(StatLcacheMiss, 1);
	b = lumpcache.free;
	lumpcache.free = b->next;

	/*
	 * the new block has no last use, so assume it happens sometime in the middle
ZZZ this is not reasonable
	 */
	b->used = (b->used2 + lumpcache.now) / 2;

	/*
	 * rechain the block on the correct hash chain
	 */
	b->next = lumpcache.heads[h];
	lumpcache.heads[h] = b;
	if(b->next != nil)
		b->next->prev = b;
	b->prev = nil;

	scorecp(b->score, score);
	b->type = type;
	b->size = 0;
	b->data = nil;

found:
	b->ref++;
	b->used2 = b->used;
	b->used = lumpcache.now++;
	if(b->heap != TWID32)
		fixheap(b->heap, b);
	CHECK(checklumpcache());
	qunlock(&lumpcache.lock);


	addstat(StatLumpStall, 1);
	qlock(&b->lock);
	addstat(StatLumpStall, -1);

	trace(TraceLump, "lookuplump exit");
	addstat2(StatLcacheRead, 1, StatLcacheReadTime, ms ? msec()-ms : 0);
	return b;
}

void
insertlump(Lump *b, Packet *p)
{
	u32int size;

	/*
	 * look for the block in the cache
	 */
	trace(TraceLump, "insertlump enter");
	qlock(&lumpcache.lock);
	CHECK(checklumpcache());
again:

	addstat(StatLcacheWrite, 1);

	/*
	 * missed: locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	size = packetasize(p);
	while(lumpcache.avail < size){
		trace(TraceLump, "insertlump bump");
		CHECK(checklumpcache());
		if(bumplump() == nil){
			logerr(EAdmin, "all lump cache blocks in use");
			addstat(StatLcacheStall, 1);
			CHECK(checklumpcache());
			rsleep(&lumpcache.full);
			CHECK(checklumpcache());
			addstat(StatLcacheStall, -1);
			goto again;
		}
		CHECK(checklumpcache());
	}
	b->data = p;
	b->size = size;
	lumpcache.avail -= size;
	CHECK(checklumpcache());
	qunlock(&lumpcache.lock);
	trace(TraceLump, "insertlump exit");
}

void
putlump(Lump *b)
{
	if(b == nil)
		return;

	trace(TraceLump, "putlump");
	qunlock(&b->lock);
	qlock(&lumpcache.lock);
	CHECK(checklumpcache());
	if(--b->ref == 0){
		if(b->heap == TWID32)
			upheap(lumpcache.nheap++, b);
		trace(TraceLump, "putlump wakeup");
		rwakeupall(&lumpcache.full);
	}
	CHECK(checklumpcache());
	qunlock(&lumpcache.lock);
}

/*
 * remove some lump from use and update the free list and counters
 */
static Lump*
bumplump(void)
{
	Lump *b;
	u32int h;

	/*
	 * remove blocks until we find one that is unused
	 * referenced blocks are left in the heap even though
	 * they can't be scavenged; this is simple a speed optimization
	 */
	CHECK(checklumpcache());
	for(;;){
		if(lumpcache.nheap == 0){
			trace(TraceLump, "bumplump emptyheap");
			return nil;
		}
		b = lumpcache.heap[0];
		delheap(b);
		if(!b->ref){
			trace(TraceLump, "bumplump wakeup");
			rwakeupall(&lumpcache.full);
			break;
		}
	}

	/*
	 * unchain the block
	 */
	trace(TraceLump, "bumplump unchain");
	if(b->prev == nil){
		h = hashbits(b->score, HashLog);
		if(lumpcache.heads[h] != b)
			sysfatal("bad hash chains in lump cache");
		lumpcache.heads[h] = b->next;
	}else
		b->prev->next = b->next;
	if(b->next != nil)
		b->next->prev = b->prev;

	if(b->data != nil){
		packetfree(b->data);
		b->data = nil;
		lumpcache.avail += b->size;
		b->size = 0;
	}
	b->type = TWID8;

	b->next = lumpcache.free;
	lumpcache.free = b;

	CHECK(checklumpcache());
	trace(TraceLump, "bumplump exit");
	return b;
}

void
emptylumpcache(void)
{
	qlock(&lumpcache.lock);
	while(bumplump())
		;
	qunlock(&lumpcache.lock);
}

/*
 * delete an arbitrary block from the heap
 */
static void
delheap(Lump *db)
{
	fixheap(db->heap, lumpcache.heap[--lumpcache.nheap]);
	db->heap = TWID32;
}

/*
 * push an element up or down to it's correct new location
 */
static void
fixheap(int i, Lump *b)
{
	if(upheap(i, b) == i)
		downheap(i, b);
}

static int
upheap(int i, Lump *b)
{
	Lump *bb;
	u32int now;
	int p;

	now = lumpcache.now;
	for(; i != 0; i = p){
		p = (i - 1) >> 1;
		bb = lumpcache.heap[p];
		if(b->used2 - now >= bb->used2 - now)
			break;
		lumpcache.heap[i] = bb;
		bb->heap = i;
	}

	lumpcache.heap[i] = b;
	b->heap = i;
	return i;
}

static int
downheap(int i, Lump *b)
{
	Lump *bb;
	u32int now;
	int k;

	now = lumpcache.now;
	for(; ; i = k){
		k = (i << 1) + 1;
		if(k >= lumpcache.nheap)
			break;
		if(k + 1 < lumpcache.nheap && lumpcache.heap[k]->used2 - now > lumpcache.heap[k + 1]->used2 - now)
			k++;
		bb = lumpcache.heap[k];
		if(b->used2 - now <= bb->used2 - now)
			break;
		lumpcache.heap[i] = bb;
		bb->heap = i;
	}

	lumpcache.heap[i] = b;
	b->heap = i;
	return i;
}

static void
findblock(Lump *bb)
{
	Lump *b, *last;
	int h;

	last = nil;
	h = hashbits(bb->score, HashLog);
	for(b = lumpcache.heads[h]; b != nil; b = b->next){
		if(last != b->prev)
			sysfatal("bad prev link");
		if(b == bb)
			return;
		last = b;
	}
	sysfatal("block score=%V type=%#x missing from hash table", bb->score, bb->type);
}

void
checklumpcache(void)
{
	Lump *b;
	u32int size, now, nfree;
	int i, k, refed;

	now = lumpcache.now;
	for(i = 0; i < lumpcache.nheap; i++){
		if(lumpcache.heap[i]->heap != i)
			sysfatal("lc: mis-heaped at %d: %d", i, lumpcache.heap[i]->heap);
		if(i > 0 && lumpcache.heap[(i - 1) >> 1]->used2 - now > lumpcache.heap[i]->used2 - now)
			sysfatal("lc: bad heap ordering");
		k = (i << 1) + 1;
		if(k < lumpcache.nheap && lumpcache.heap[i]->used2 - now > lumpcache.heap[k]->used2 - now)
			sysfatal("lc: bad heap ordering");
		k++;
		if(k < lumpcache.nheap && lumpcache.heap[i]->used2 - now > lumpcache.heap[k]->used2 - now)
			sysfatal("lc: bad heap ordering");
	}

	refed = 0;
	size = 0;
	for(i = 0; i < lumpcache.nblocks; i++){
		b = &lumpcache.blocks[i];
		if(b->data == nil && b->size != 0)
			sysfatal("bad size: %d data=%p", b->size, b->data);
		if(b->ref && b->heap == TWID32)
			refed++;
		if(b->type != TWID8){
			findblock(b);
			size += b->size;
		}
		if(b->heap != TWID32
		&& lumpcache.heap[b->heap] != b)
			sysfatal("lc: spurious heap value");
	}
	if(lumpcache.avail != lumpcache.allowed - size){
		fprint(2, "mismatched available=%d and allowed=%d - used=%d space", lumpcache.avail, lumpcache.allowed, size);
		*(volatile int*)0=0;
	}

	nfree = 0;
	for(b = lumpcache.free; b != nil; b = b->next){
		if(b->type != TWID8 || b->heap != TWID32)
			sysfatal("lc: bad free list");
		nfree++;
	}

	if(lumpcache.nheap + nfree + refed != lumpcache.nblocks)
		sysfatal("lc: missing blocks: %d %d %d %d", lumpcache.nheap, refed, nfree, lumpcache.nblocks);
}
