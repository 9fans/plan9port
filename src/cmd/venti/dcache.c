/*
 * The locking here is getting a little out of hand.
 */

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
	RWLock		dirtylock;		/* must be held to inspect or set b->dirty */
	u32int		flushround;
	Rendez		anydirty;
	Rendez		full;
	Rendez		flush;
	Rendez		flushdone;
	DBlock		*free;			/* list of available lumps */
	u32int		now;			/* ticks for usage timestamps */
	int		size;			/* max. size of any block; allocated to each block */
	DBlock		**heads;		/* hash table for finding address */
	int		nheap;			/* number of available victims */
	DBlock		**heap;			/* heap for locating victims */
	int		nblocks;		/* number of blocks allocated */
	DBlock		*blocks;		/* array of block descriptors */
	DBlock		**write;		/* array of block pointers to be written */
	u8int		*mem;			/* memory for all block descriptors */
	int		ndirty;			/* number of dirty blocks */
	int		maxdirty;		/* max. number of dirty blocks */
};

static DCache	dcache;

static int	downheap(int i, DBlock *b);
static int	upheap(int i, DBlock *b);
static DBlock	*bumpdblock(void);
static void	delheap(DBlock *db);
static void	fixheap(int i, DBlock *b);
static void	_flushdcache(void);
static void	flushproc(void*);
static void	flushtimerproc(void*);
static void	writeproc(void*);

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
	dcache.full.l = &dcache.lock;
	dcache.flush.l = &dcache.lock;
	dcache.anydirty.l = &dcache.lock;
	dcache.flushdone.l = &dcache.lock;
	dcache.nblocks = nblocks;
	dcache.maxdirty = (nblocks * 3) / 4;
	if(1)
		fprint(2, "initialize disk cache with %d blocks of %d bytes, maximum %d dirty blocks\n",
			nblocks, blocksize, dcache.maxdirty);
	dcache.size = blocksize;
	dcache.heads = MKNZ(DBlock*, HashSize);
	dcache.heap = MKNZ(DBlock*, nblocks);
	dcache.blocks = MKNZ(DBlock, nblocks);
	dcache.write = MKNZ(DBlock*, nblocks);
	dcache.mem = MKNZ(u8int, nblocks * blocksize);

	last = nil;
	for(i = 0; i < nblocks; i++){
		b = &dcache.blocks[i];
		b->data = &dcache.mem[i * blocksize];
		b->heap = TWID32;
		chaninit(&b->writedonechan, sizeof(void*), 1);
		b->next = last;
		last = b;
	}
	dcache.free = last;
	dcache.nheap = 0;

	vtproc(flushproc, nil);
	vtproc(flushtimerproc, nil);
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

	if(b->dirtying){
		b->dirtying = 0;
		runlock(&dcache.dirtylock);
	}
	qunlock(&b->lock);

//checkdcache();
	qlock(&dcache.lock);
	if(b->dirty)
		delheap(b);
	else if(--b->ref == 0){
		if(b->heap == TWID32)
			upheap(dcache.nheap++, b);
		rwakeupall(&dcache.full);
	}

	qunlock(&dcache.lock);
//checkdcache();
}

void
dirtydblock(DBlock *b, int dirty)
{
	int odirty;
	Part *p;


	assert(b->ref != 0);

	/*
 	 * Because we use an rlock to keep track of how
	 * many blocks are being dirtied (and a wlock to
	 * stop dirtying), we cannot try to dirty two blocks
	 * at the same time in the same thread -- if the 
	 * wlock happens between them, we get a deadlock.
	 * 
	 * The only place in the code where we need to
	 * dirty multiple blocks at once is in splitiblock, when
	 * we split an index block.  The new block has a dirty
	 * flag of DirtyIndexSplit already, because it has to get
	 * written to disk before the old block (DirtyIndex).
	 * So when we see the DirtyIndexSplit block, we don't
	 * acquire the rlock (and we don't set dirtying, so putdblock
	 * won't drop the rlock).  This kludginess means that
	 * the caller will have to be sure to putdblock on the 
	 * new block before putdblock on the old block.
	 */
	if(dirty == DirtyIndexSplit){
		/* caller should have another dirty block */
		assert(!canwlock(&dcache.dirtylock));
		/* this block should be clean */
		assert(b->dirtying == 0);
		assert(b->dirty == 0);
	}else{
		rlock(&dcache.dirtylock);
		assert(b->dirtying == 0);
		b->dirtying = 1;
	}

	qlock(&stats.lock);
	if(b->dirty)
		stats.absorbedwrites++;
	stats.dirtydblocks++;
	qunlock(&stats.lock);

	/*
	 * In general, shouldn't mark any block as more than one
	 * type, except that split index blocks are a subcase of index
	 * blocks.  Only clean blocks ever get marked DirtyIndexSplit,
	 * though, so we don't need the opposite conjunction here.
	 */
	odirty = b->dirty;
	if(b->dirty)
		assert(b->dirty == dirty
			|| (b->dirty==DirtyIndexSplit && dirty==DirtyIndex));
	else
		b->dirty = dirty;

	p = b->part;
	if(p->writechan == nil){
fprint(2, "allocate write proc for part %s\n", p->name);
		/* XXX hope this doesn't fail! */
		p->writechan = chancreate(sizeof(DBlock*), dcache.nblocks);
		vtproc(writeproc, p);
	}
	qlock(&dcache.lock);
	if(!odirty){
		dcache.ndirty++;
		rwakeupall(&dcache.anydirty);
	}
	qunlock(&dcache.lock);
}

/*
 * remove some block from use and update the free list and counters
 */
static DBlock*
bumpdblock(void)
{
	int flushed;
	DBlock *b;
	ulong h;

	b = dcache.free;
	if(b != nil){
		dcache.free = b->next;
		return b;
	}

	if(dcache.ndirty >= dcache.maxdirty)
		_flushdcache();

	/*
	 * remove blocks until we find one that is unused
	 * referenced blocks are left in the heap even though
	 * they can't be scavenged; this is simple a speed optimization
	 */
	flushed = 0;
	for(;;){
		if(dcache.nheap == 0){
			_flushdcache();
			return nil;
		}
		b = dcache.heap[0];
		delheap(b);
		if(!b->ref)
			break;
	}

	fprint(2, "bump %s at %llud\n", b->part->name, b->addr);

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
	if(db->heap == TWID32)
		return;
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

void
flushdcache(void)
{
	u32int flushround;

	qlock(&dcache.lock);
	flushround = dcache.flushround;
	rwakeupall(&dcache.flush);
	while(flushround == dcache.flushround)
		rsleep(&dcache.flushdone);
	qunlock(&dcache.lock);
}

static void
_flushdcache(void)
{
	rwakeupall(&dcache.flush);
}

static int
parallelwrites(DBlock **b, DBlock **eb, int dirty)
{
	DBlock **p;

	for(p=b; p<eb && (*p)->dirty == dirty; p++)
		sendp((*p)->part->writechan, *p);
	for(p=b; p<eb && (*p)->dirty == dirty; p++)
		recvp(&(*p)->writedonechan);

	return p-b;
}

/*
 * Sort first by dirty flag, then by partition, then by address in partition.
 */
static int
writeblockcmp(const void *va, const void *vb)
{
	DBlock *a, *b;

	a = *(DBlock**)va;
	b = *(DBlock**)vb;

	if(a->dirty != b->dirty)
		return a->dirty - b->dirty;
	if(a->part != b->part){
		if(a->part < b->part)
			return -1;
		if(a->part > b->part)
			return 1;
	}
	if(a->addr < b->addr)
		return -1;
	return 1;
}

static void
flushtimerproc(void *v)
{
	u32int round;

	for(;;){
		qlock(&dcache.lock);
		while(dcache.ndirty == 0)
			rsleep(&dcache.anydirty);
		round = dcache.flushround;
		qunlock(&dcache.lock);

		sleep(60*1000);

		qlock(&dcache.lock);
		if(round == dcache.flushround){
			rwakeupall(&dcache.flush);
			while(round == dcache.flushround)
				rsleep(&dcache.flushdone);
		}
		qunlock(&dcache.lock);
	}
}

static void
flushproc(void *v)
{
	int i, j, n;
	ulong t0;
	DBlock *b, **write;

	USED(v);
	for(;;){
		qlock(&dcache.lock);
		dcache.flushround++;
		rwakeupall(&dcache.flushdone);
		rsleep(&dcache.flush);
		qunlock(&dcache.lock);

		fprint(2, "flushing dcache: t=0 ms\n");
		t0 = nsec()/1000;

		/*
		 * Because we don't record any dependencies at all, we must write out
		 * all blocks currently dirty.  Thus we must lock all the blocks that 
		 * are currently dirty.
		 *
		 * We grab dirtylock to stop the dirtying of new blocks.
		 * Then we wait until all the current blocks finish being dirtied.
		 * Now all the dirty blocks in the system are immutable (clean blocks
		 * might still get recycled), so we can plan our disk writes.
		 * 
		 * In a better scheme, dirtiers might lock the block for writing in getdblock,
		 * so that flushproc could lock all the blocks here and then unlock them as it
		 * finishes with them.
		 */ 
	
		fprint(2, "flushproc: wlock at t=%lud\n", (ulong)(nsec()/1000) - t0);
		wlock(&dcache.dirtylock);

		fprint(2, "flushproc: build list at t=%lud\n", (ulong)(nsec()/1000) - t0);
		write = dcache.write;
		n = 0;
		for(i=0; i<dcache.nblocks; i++){
			b = &dcache.blocks[i];
			if(b->dirty)
				write[n++] = b;
		}

		qsort(write, n, sizeof(write[0]), writeblockcmp);

		/* Write each stage of blocks out. */
		fprint(2, "flushproc: write blocks at t=%lud\n", (ulong)(nsec()/1000) - t0);
		i = 0;
		for(j=1; j<DirtyMax; j++){
			fprint(2, "flushproc: flush dirty %d at t=%lud\n", j, (ulong)(nsec()/1000) - t0);
			i += parallelwrites(write+i, write+n, j);
		}
		assert(i == n);

		fprint(2, "flushproc: update dirty bits at t=%lud\n", (ulong)(nsec()/1000) - t0);
		qlock(&dcache.lock);
		for(i=0; i<n; i++){
			b = write[i];
			b->dirty = 0;
			--dcache.ndirty;
			if(b->ref == 0 && b->heap == TWID32){
				upheap(dcache.nheap++, b);
				rwakeupall(&dcache.full);
			}
		}
		qunlock(&dcache.lock);
		wunlock(&dcache.dirtylock);

		fprint(2, "flushproc: done at t=%lud\n", (ulong)(nsec()/1000) - t0);

		qlock(&stats.lock);
		stats.dcacheflushes++;
		stats.dcacheflushwrites += n;
		qunlock(&stats.lock);
	}
}

static void
writeproc(void *v)
{
	DBlock *b;
	Part *p;

	p = v;

	for(;;){
		b = recvp(p->writechan);
		if(writepart(p, b->addr, b->data, b->size) < 0)
			fprint(2, "write error: %r\n"); /* XXX details! */
		sendp(&b->writedonechan, b);
	}
}
