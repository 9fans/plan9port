#include "stdinc.h"
#include "dat.h"
#include "fns.h"

typedef struct DCache	DCache;

enum
{
	HashLog		= 9,
	HashSize	= 1<<HashLog,
	HashMask	= HashSize - 1,
};

struct DCache
{
	QLock		lock;
	Rendez		full;
	DBlock		*free;			/* list of available lumps */
	u32int		now;			/* ticks for usage timestamps */
	int		size;			/* max. size of any block; allocated to each block */
	DBlock		**heads;		/* hash table for finding address */
	int		nheap;			/* number of available victims */
	DBlock		**heap;			/* heap for locating victims */
	int		nblocks;		/* number of blocks allocated */
	DBlock		*blocks;		/* array of block descriptors */
	u8int		*mem;			/* memory for all block descriptors */
};

static DCache	dcache;

static int	downheap(int i, DBlock *b);
static int	upheap(int i, DBlock *b);
static DBlock	*bumpdblock(void);
static void	delheap(DBlock *db);
static void	fixheap(int i, DBlock *b);

void
initdcache(u32int mem)
{
	DBlock *b, *last;
	u32int nblocks, blocksize;
	int i;

	if(mem < maxblocksize * 2)
		sysfatal("need at least %d bytes for the disk cache", maxblocksize * 2);
	if(maxblocksize == 0)
		sysfatal("no max. block size given for disk cache");
	blocksize = maxblocksize;
	nblocks = mem / blocksize;
	if(0)
		fprint(2, "initialize disk cache with %d blocks of %d bytes\n", nblocks, blocksize);
	dcache.full.l = &dcache.lock;
	dcache.nblocks = nblocks;
	dcache.size = blocksize;
	dcache.heads = MKNZ(DBlock*, HashSize);
	dcache.heap = MKNZ(DBlock*, nblocks);
	dcache.blocks = MKNZ(DBlock, nblocks);
	dcache.mem = MKNZ(u8int, nblocks * blocksize);

	last = nil;
	for(i = 0; i < nblocks; i++){
		b = &dcache.blocks[i];
		b->data = &dcache.mem[i * blocksize];
		b->heap = TWID32;
		b->next = last;
		last = b;
	}
	dcache.free = last;
	dcache.nheap = 0;
}

static u32int
pbhash(u64int addr)
{
	u32int h;

#define hashit(c)	((((c) * 0x6b43a9b5) >> (32 - HashLog)) & HashMask)
	h = (addr >> 32) ^ addr;
	return hashit(h);
}

DBlock*
getdblock(Part *part, u64int addr, int read)
{
	DBlock *b;
	u32int h, size;

	size = part->blocksize;
	if(size > dcache.size){
		seterr(EAdmin, "block size %d too big for cache", size);
		return nil;
	}
	h = pbhash(addr);

	/*
	 * look for the block in the cache
	 */
//checkdcache();
	qlock(&dcache.lock);
again:
	for(b = dcache.heads[h]; b != nil; b = b->next){
		if(b->part == part && b->addr == addr){
			qlock(&stats.lock);
			stats.pchit++;
			qunlock(&stats.lock);
			goto found;
		}
	}
	qlock(&stats.lock);
	stats.pcmiss++;
	qunlock(&stats.lock);

	/*
	 * missed: locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	b = bumpdblock();
	if(b == nil){
		logerr(EAdmin, "all disk cache blocks in use");
		rsleep(&dcache.full);
		goto again;
	}

	/*
	 * the new block has no last use, so assume it happens sometime in the middle
ZZZ this is not reasonable
	 */
	b->used = (b->used2 + dcache.now) / 2;

	/*
	 * rechain the block on the correct hash chain
	 */
	b->next = dcache.heads[h];
	dcache.heads[h] = b;
	if(b->next != nil)
		b->next->prev = b;
	b->prev = nil;

	b->addr = addr;
	b->part = part;
	b->size = 0;

found:
	b->ref++;
	b->used2 = b->used;
	b->used = dcache.now++;
	if(b->heap != TWID32)
		fixheap(b->heap, b);

	qunlock(&dcache.lock);
//checkdcache();

	qlock(&b->lock);
	if(b->size != size){
		if(b->size < size){
			if(!read)
				memset(&b->data[b->size], 0, size - b->size);
			else{
				if(readpart(part, addr + b->size, &b->data[b->size], size - b->size) < 0){
					putdblock(b);
					return nil;
				}
				qlock(&stats.lock);
				stats.pcreads++;
				stats.pcbreads += size - b->size;
				qunlock(&stats.lock);
			}
		}
		b->size = size;
	}

	return b;
}

void
putdblock(DBlock *b)
{
	if(b == nil)
		return;

	qunlock(&b->lock);
//checkdcache();
	qlock(&dcache.lock);
	if(--b->ref == 0){
		if(b->heap == TWID32)
			upheap(dcache.nheap++, b);
		rwakeup(&dcache.full);
	}

	qunlock(&dcache.lock);
//checkdcache();
}

/*
 * remove some block from use and update the free list and counters
 */
static DBlock*
bumpdblock(void)
{
	DBlock *b;
	ulong h;

	b = dcache.free;
	if(b != nil){
		dcache.free = b->next;
		return b;
	}

	/*
	 * remove blocks until we find one that is unused
	 * referenced blocks are left in the heap even though
	 * they can't be scavenged; this is simple a speed optimization
	 */
	for(;;){
		if(dcache.nheap == 0)
			return nil;
		b = dcache.heap[0];
		delheap(b);
		if(!b->ref)
			break;
	}

	/*
	 * unchain the block
	 */
	if(b->prev == nil){
		h = pbhash(b->addr);
		if(dcache.heads[h] != b)
			sysfatal("bad hash chains in disk cache");
		dcache.heads[h] = b->next;
	}else
		b->prev->next = b->next;
	if(b->next != nil)
		b->next->prev = b->prev;

	return b;
}

/*
 * delete an arbitrary block from the heap
 */
static void
delheap(DBlock *db)
{
	fixheap(db->heap, dcache.heap[--dcache.nheap]);
	db->heap = TWID32;
}

/*
 * push an element up or down to it's correct new location
 */
static void
fixheap(int i, DBlock *b)
{
	if(upheap(i, b) == i)
		downheap(i, b);
}

static int
upheap(int i, DBlock *b)
{
	DBlock *bb;
	u32int now;
	int p;

	now = dcache.now;
	for(; i != 0; i = p){
		p = (i - 1) >> 1;
		bb = dcache.heap[p];
		if(b->used2 - now >= bb->used2 - now)
			break;
		dcache.heap[i] = bb;
		bb->heap = i;
	}

	dcache.heap[i] = b;
	b->heap = i;
	return i;
}

static int
downheap(int i, DBlock *b)
{
	DBlock *bb;
	u32int now;
	int k;

	now = dcache.now;
	for(; ; i = k){
		k = (i << 1) + 1;
		if(k >= dcache.nheap)
			break;
		if(k + 1 < dcache.nheap && dcache.heap[k]->used2 - now > dcache.heap[k + 1]->used2 - now)
			k++;
		bb = dcache.heap[k];
		if(b->used2 - now <= bb->used2 - now)
			break;
		dcache.heap[i] = bb;
		bb->heap = i;
	}

	dcache.heap[i] = b;
	b->heap = i;
	return i;
}

static void
findblock(DBlock *bb)
{
	DBlock *b, *last;
	int h;

	last = nil;
	h = pbhash(bb->addr);
	for(b = dcache.heads[h]; b != nil; b = b->next){
		if(last != b->prev)
			sysfatal("bad prev link");
		if(b == bb)
			return;
		last = b;
	}
	sysfatal("block missing from hash table");
}

void
checkdcache(void)
{
	DBlock *b;
	u32int size, now;
	int i, k, refed, nfree;

	qlock(&dcache.lock);
	size = dcache.size;
	now = dcache.now;
	for(i = 0; i < dcache.nheap; i++){
		if(dcache.heap[i]->heap != i)
			sysfatal("dc: mis-heaped at %d: %d", i, dcache.heap[i]->heap);
		if(i > 0 && dcache.heap[(i - 1) >> 1]->used2 - now > dcache.heap[i]->used2 - now)
			sysfatal("dc: bad heap ordering");
		k = (i << 1) + 1;
		if(k < dcache.nheap && dcache.heap[i]->used2 - now > dcache.heap[k]->used2 - now)
			sysfatal("dc: bad heap ordering");
		k++;
		if(k < dcache.nheap && dcache.heap[i]->used2 - now > dcache.heap[k]->used2 - now)
			sysfatal("dc: bad heap ordering");
	}

	refed = 0;
	for(i = 0; i < dcache.nblocks; i++){
		b = &dcache.blocks[i];
		if(b->data != &dcache.mem[i * size])
			sysfatal("dc: mis-blocked at %d", i);
		if(b->ref && b->heap == TWID32)
			refed++;
		if(b->addr)
			findblock(b);
		if(b->heap != TWID32
		&& dcache.heap[b->heap] != b)
			sysfatal("dc: spurious heap value");
	}

	nfree = 0;
	for(b = dcache.free; b != nil; b = b->next){
		if(b->addr != 0 || b->heap != TWID32)
			sysfatal("dc: bad free list");
		nfree++;
	}

	if(dcache.nheap + nfree + refed != dcache.nblocks)
		sysfatal("dc: missing blocks: %d %d %d", dcache.nheap, refed, dcache.nblocks);
	qunlock(&dcache.lock);
}
