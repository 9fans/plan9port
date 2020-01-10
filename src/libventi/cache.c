/*
 * Memory-only VtBlock cache.
 *
 * The cached Venti blocks are in the hash chains.
 * The cached local blocks are only in the blocks array.
 * The free blocks are in the heap, which is supposed to
 * be indexed by second-to-last use but actually
 * appears to be last use.
 */

#include <u.h>
#include <libc.h>
#include <venti.h>

int vtcachenread;
int vtcachencopy;
int vtcachenwrite;
int vttracelevel;

enum {
	BioLocal = 1,
	BioVenti,
	BioReading,
	BioWriting,
	BioEmpty,
	BioVentiError
};
enum {
	BadHeap = ~0
};
struct VtCache
{
	QLock	lk;
	VtConn	*z;
	u32int	now;		/* ticks for usage time stamps */
	VtBlock	**hash;	/* hash table for finding addresses */
	int		nhash;
	VtBlock	**heap;	/* heap for finding victims */
	int		nheap;
	VtBlock	*block;	/* all allocated blocks */
	int		nblock;
	int		(*write)(VtConn*, uchar[VtScoreSize], uint, uchar*, int);
	VtBlock	*dead;	/* blocks we don't have memory for */
	ulong	mem;
	ulong	maxmem;
};

static void cachecheck(VtCache*);

VtCache*
vtcachealloc(VtConn *z, ulong maxmem)
{
	VtCache *c;
	int i;
	int nblock;
	VtBlock *b;
	ulong maxmem0;

	maxmem0 = maxmem;
	c = vtmallocz(sizeof(VtCache));
	nblock = maxmem/100/(sizeof(VtBlock)+2*sizeof(VtBlock*));
	c->z = z;
	c->nblock = nblock;
	c->nhash = nblock;
	c->hash = vtmallocz(nblock*sizeof(VtBlock*));
	c->heap = vtmallocz(nblock*sizeof(VtBlock*));
	c->block = vtmallocz(nblock*sizeof(VtBlock));
	c->write = vtwrite;
	maxmem -= nblock*(sizeof(VtBlock) + 2*sizeof(VtBlock*));
	maxmem -= sizeof(VtCache);
	if((long)maxmem < 0)
		sysfatal("cache size far too small: %lud", maxmem0);
	c->mem = maxmem;

	for(i=0; i<nblock; i++){
		b = &c->block[i];
		b->addr = NilBlock;
		b->c = c;
		b->heap = i;
		c->heap[i] = b;
	}
	c->nheap = nblock;
	cachecheck(c);
	return c;
}

/*
 * BUG This is here so that vbackup can override it and do some
 * pipelining of writes.  Arguably vtwrite or vtwritepacket or the
 * cache itself should be providing this functionality.
 */
void
vtcachesetwrite(VtCache *c, int (*write)(VtConn*, uchar[VtScoreSize], uint, uchar*, int))
{
	if(write == nil)
		write = vtwrite;
	c->write = write;
}

void
vtcachefree(VtCache *c)
{
	int i;

	qlock(&c->lk);

	cachecheck(c);
	for(i=0; i<c->nblock; i++) {
		assert(c->block[i].data == nil || c->block[i].ref == 0);
		vtfree(c->block[i].data);
	}

	vtfree(c->hash);
	vtfree(c->heap);
	vtfree(c->block);
	vtfree(c);
}

static void
vtcachedump(VtCache *c)
{
	int i;
	VtBlock *b;

	for(i=0; i<c->nblock; i++){
		b = &c->block[i];
		print("cache block %d: type %d score %V iostate %d addr %d ref %d nlock %d\n",
			i, b->type, b->score, b->iostate, b->addr, b->ref, b->nlock);
	}
}

static void
cachecheck(VtCache *c)
{
	u32int now;
	int i, k, refed;
	VtBlock *b;

	now = c->now;

	for(i = 0; i < c->nheap; i++){
		if(c->heap[i]->heap != i)
			sysfatal("mis-heaped at %d: %d", i, c->heap[i]->heap);
		if(i > 0 && c->heap[(i - 1) >> 1]->used - now > c->heap[i]->used - now)
			sysfatal("bad heap ordering");
		k = (i << 1) + 1;
		if(k < c->nheap && c->heap[i]->used - now > c->heap[k]->used - now)
			sysfatal("bad heap ordering");
		k++;
		if(k < c->nheap && c->heap[i]->used - now > c->heap[k]->used - now)
			sysfatal("bad heap ordering");
	}

	refed = 0;
	for(i = 0; i < c->nblock; i++){
		b = &c->block[i];
		if(b->ref && b->heap == BadHeap)
			refed++;
		else if(b->addr != NilBlock)
			refed++;
	}
	assert(c->nheap + refed == c->nblock);
	refed = 0;
	for(i = 0; i < c->nblock; i++){
		b = &c->block[i];
		if(b->ref){
			refed++;
		}
	}
}

static int
upheap(int i, VtBlock *b)
{
	VtBlock *bb;
	u32int now;
	int p;
	VtCache *c;

	c = b->c;
	now = c->now;
	for(; i != 0; i = p){
		p = (i - 1) >> 1;
		bb = c->heap[p];
		if(b->used - now >= bb->used - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;

	return i;
}

static int
downheap(int i, VtBlock *b)
{
	VtBlock *bb;
	u32int now;
	int k;
	VtCache *c;

	c = b->c;
	now = c->now;
	for(; ; i = k){
		k = (i << 1) + 1;
		if(k >= c->nheap)
			break;
		if(k + 1 < c->nheap && c->heap[k]->used - now > c->heap[k + 1]->used - now)
			k++;
		bb = c->heap[k];
		if(b->used - now <= bb->used - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;
	return i;
}

/*
 * Delete a block from the heap.
 * Called with c->lk held.
 */
static void
heapdel(VtBlock *b)
{
	int i, si;
	VtCache *c;

	c = b->c;

	si = b->heap;
	if(si == BadHeap)
		return;
	b->heap = BadHeap;
	c->nheap--;
	if(si == c->nheap)
		return;
	b = c->heap[c->nheap];
	i = upheap(si, b);
	if(i == si)
		downheap(i, b);
}

/*
 * Insert a block into the heap.
 * Called with c->lk held.
 */
static void
heapins(VtBlock *b)
{
	assert(b->heap == BadHeap);
	upheap(b->c->nheap++, b);
}

/*
 * locate the vtBlock with the oldest second to last use.
 * remove it from the heap, and fix up the heap.
 */
/* called with c->lk held */
static VtBlock*
vtcachebumpblock(VtCache *c)
{
	VtBlock *b;

	/*
	 * locate the vtBlock with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	if(c->nheap == 0){
		vtcachedump(c);
		fprint(2, "vtcachebumpblock: no free blocks in vtCache");
		abort();
	}
	b = c->heap[0];
	heapdel(b);

	assert(b->heap == BadHeap);
	assert(b->ref == 0);

	/*
	 * unchain the vtBlock from hash chain if any
	 */
	if(b->prev){
		*(b->prev) = b->next;
		if(b->next)
			b->next->prev = b->prev;
		b->prev = nil;
	}


if(0)fprint(2, "droping %x:%V\n", b->addr, b->score);
	/* set vtBlock to a reasonable state */
	b->ref = 1;
	b->iostate = BioEmpty;
	return b;
}

/*
 * evict blocks until there is enough memory for size bytes.
 */
static VtBlock*
vtcacheevict(VtCache *c, ulong size)
{
	VtBlock *b;

	/*
	 * If we were out of memory and put some blocks
	 * to the side but now we have memory, grab one.
	 */
	if(c->mem >= size && c->dead) {
		b = c->dead;
		c->dead = b->next;
		b->next = nil;
		goto alloc;
	}

	/*
	 * Otherwise, evict until we have memory.
	 */
	for(;;) {
		b = vtcachebumpblock(c);
		if(c->mem+b->size >= size)
			break;
		/*
		 * chain b onto dead list
		 */
		free(b->data);
		b->data = nil;
		c->mem += b->size;
		b->size = 0;
		b->next = c->dead;
		c->dead = b;
	}

	/*
	 * Allocate memory for block.
	 */
alloc:
	if(size > b->size || size <= b->size/2) {
		free(b->data);
		c->mem += b->size;
		c->mem -= size;
		b->size = size;
		b->data = vtmalloc(size);
	}
	return b;
}

/*
 * fetch a local block from the memory cache.
 * if it's not there, load it, bumping some other Block.
 * if we're out of free blocks, we're screwed.
 */
VtBlock*
vtcachelocal(VtCache *c, u32int addr, int type)
{
	VtBlock *b;

	if(addr == 0)
		sysfatal("vtcachelocal: asked for nonexistent block 0");
	if(addr > c->nblock)
		sysfatal("vtcachelocal: asked for block #%ud; only %d blocks",
			(uint)addr, c->nblock);

	b = &c->block[addr-1];
	if(b->addr == NilBlock || b->iostate != BioLocal)
		sysfatal("vtcachelocal: block is not local");

	if(b->type != type)
		sysfatal("vtcachelocal: block has wrong type %d != %d", b->type, type);

	qlock(&c->lk);
	b->ref++;
	qunlock(&c->lk);

	qlock(&b->lk);
	b->nlock = 1;
	b->pc = getcallerpc(&c);
	return b;
}

VtBlock*
vtcacheallocblock(VtCache *c, int type, ulong size)
{
	VtBlock *b;

	qlock(&c->lk);
	b = vtcacheevict(c, size);
	b->iostate = BioLocal;
	b->type = type;
	b->addr = (b - c->block)+1;
	vtzeroextend(type, b->data, 0, size);
	vtlocaltoglobal(b->addr, b->score);
	qunlock(&c->lk);

	qlock(&b->lk);
	b->nlock = 1;
	b->pc = getcallerpc(&c);
	return b;
}

/*
 * fetch a global (Venti) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 */
VtBlock*
vtcacheglobal(VtCache *c, uchar score[VtScoreSize], int type, ulong size)
{
	VtBlock *b;
	ulong h;
	int n;
	u32int addr;

	if(vttracelevel)
		fprint(2, "vtcacheglobal %V %d from %p\n", score, type, getcallerpc(&c));
	addr = vtglobaltolocal(score);
	if(addr != NilBlock){
		if(vttracelevel)
			fprint(2, "vtcacheglobal %V %d => local\n", score, type);
		b = vtcachelocal(c, addr, type);
		if(b)
			b->pc = getcallerpc(&c);
		return b;
	}

	h = (u32int)(score[0]|(score[1]<<8)|(score[2]<<16)|(score[3]<<24)) % c->nhash;

	/*
	 * look for the block in the cache
	 */
	qlock(&c->lk);
	for(b = c->hash[h]; b != nil; b = b->next){
		if(b->addr != NilBlock || memcmp(b->score, score, VtScoreSize) != 0 || b->type != type)
			continue;
		heapdel(b);
		b->ref++;
		qunlock(&c->lk);
		if(vttracelevel)
			fprint(2, "vtcacheglobal %V %d => found in cache %p; locking\n", score, type, b);
		qlock(&b->lk);
		b->nlock = 1;
		if(b->iostate == BioVentiError){
			if(chattyventi)
				fprint(2, "cached read error for %V\n", score);
			if(vttracelevel)
				fprint(2, "vtcacheglobal %V %d => cache read error\n", score, type);
			vtblockput(b);
			werrstr("venti i/o error");
			return nil;
		}
		if(vttracelevel)
			fprint(2, "vtcacheglobal %V %d => found in cache; returning\n", score, type);
		b->pc = getcallerpc(&c);
		return b;
	}

	/*
	 * not found
	 */
	b = vtcacheevict(c, size);
	b->addr = NilBlock;
	b->type = type;
	memmove(b->score, score, VtScoreSize);
	/* chain onto correct hash */
	b->next = c->hash[h];
	c->hash[h] = b;
	if(b->next != nil)
		b->next->prev = &b->next;
	b->prev = &c->hash[h];

	/*
	 * Lock b before unlocking c, so that others wait while we read.
	 *
	 * You might think there is a race between this qlock(b) before qunlock(c)
	 * and the qlock(c) while holding a qlock(b) in vtblockwrite.  However,
	 * the block here can never be the block in a vtblockwrite, so we're safe.
	 * We're certainly living on the edge.
	 */
	if(vttracelevel)
		fprint(2, "vtcacheglobal %V %d => bumped; locking %p\n", score, type, b);
	qlock(&b->lk);
	b->nlock = 1;
	qunlock(&c->lk);

	vtcachenread++;
	n = vtread(c->z, score, type, b->data, size);
	if(n < 0){
		if(chattyventi)
			fprint(2, "read %V: %r\n", score);
		if(vttracelevel)
			fprint(2, "vtcacheglobal %V %d => bumped; read error\n", score, type);
		b->iostate = BioVentiError;
		vtblockput(b);
		return nil;
	}
	vtzeroextend(type, b->data, n, size);
	b->iostate = BioVenti;
	b->nlock = 1;
	if(vttracelevel)
		fprint(2, "vtcacheglobal %V %d => loaded into cache; returning\n", score, type);
	b->pc = getcallerpc(&b);
	return b;
}

/*
 * The thread that has locked b may refer to it by
 * multiple names.  Nlock counts the number of
 * references the locking thread holds.  It will call
 * vtblockput once per reference.
 */
void
vtblockduplock(VtBlock *b)
{
	assert(b->nlock > 0);
	b->nlock++;
}

/*
 * we're done with the block.
 * unlock it.  can't use it after calling this.
 */
void
vtblockput(VtBlock* b)
{
	VtCache *c;

	if(b == nil)
		return;

if(0)fprint(2, "vtblockput: %d: %x %d %d\n", getpid(), b->addr, c->nheap, b->iostate);
	if(vttracelevel)
		fprint(2, "vtblockput %p from %p\n", b, getcallerpc(&b));

	if(--b->nlock > 0)
		return;

	/*
	 * b->nlock should probably stay at zero while
	 * the vtBlock is unlocked, but diskThread and vtSleep
	 * conspire to assume that they can just qlock(&b->lk); vtblockput(b),
	 * so we have to keep b->nlock set to 1 even
	 * when the vtBlock is unlocked.
	 */
	assert(b->nlock == 0);
	b->nlock = 1;

	qunlock(&b->lk);
	c = b->c;
	qlock(&c->lk);

	if(--b->ref > 0){
		qunlock(&c->lk);
		return;
	}

	assert(b->ref == 0);
	switch(b->iostate){
	case BioVenti:
/*if(b->addr != NilBlock) print("blockput %d\n", b->addr); */
		b->used = c->now++;
		/* fall through */
	case BioVentiError:
		heapins(b);
		break;
	case BioLocal:
		break;
	}
	qunlock(&c->lk);
}

int
vtblockwrite(VtBlock *b)
{
	uchar score[VtScoreSize];
	VtCache *c;
	uint h;
	int n;

	if(b->iostate != BioLocal){
		werrstr("vtblockwrite: not a local block");
		return -1;
	}

	c = b->c;
	n = vtzerotruncate(b->type, b->data, b->size);
	vtcachenwrite++;
	if(c->write(c->z, score, b->type, b->data, n) < 0)
		return -1;

	memmove(b->score, score, VtScoreSize);

	qlock(&c->lk);
	b->addr = NilBlock;	/* now on venti */
	b->iostate = BioVenti;
	h = (u32int)(score[0]|(score[1]<<8)|(score[2]<<16)|(score[3]<<24)) % c->nhash;
	b->next = c->hash[h];
	c->hash[h] = b;
	if(b->next != nil)
		b->next->prev = &b->next;
	b->prev = &c->hash[h];
	qunlock(&c->lk);
	return 0;
}

VtBlock*
vtblockcopy(VtBlock *b)
{
	VtBlock *bb;

	vtcachencopy++;
	bb = vtcacheallocblock(b->c, b->type, b->size);
	if(bb == nil){
		vtblockput(b);
		return nil;
	}
	memmove(bb->data, b->data, b->size);
	vtblockput(b);
	bb->pc = getcallerpc(&b);
	return bb;
}

void
vtlocaltoglobal(u32int addr, uchar score[VtScoreSize])
{
	memset(score, 0, 16);
	score[16] = addr>>24;
	score[17] = addr>>16;
	score[18] = addr>>8;
	score[19] = addr;
}


u32int
vtglobaltolocal(uchar score[VtScoreSize])
{
	static uchar zero[16];
	if(memcmp(score, zero, 16) != 0)
		return NilBlock;
	return (score[16]<<24)|(score[17]<<16)|(score[18]<<8)|score[19];
}
