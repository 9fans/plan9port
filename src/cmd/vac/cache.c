#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

typedef struct Label Label;

enum {
	BadHeap = ~0,
};

/*
 * the plan is to store data to the cache in c->size blocks
 * with the block zero extended to fill it out.  When writing to
 * venti, the block will be zero truncated.  The walker will also check
 * that the block fits within psize or dsize as the case may be.
 */

struct Cache
{
	VtLock	*lk;
	VtSession *z;
	u32int	now;			/* ticks for usage timestamps */
	int	size;			/* max. size of any block; allocated to each block */
	Lump	**heads;		/* hash table for finding address */
	int	nheap;			/* number of available victims */
	Lump	**heap;			/* heap for locating victims */
	long	nblocks;		/* number of blocks allocated */
	Lump	*blocks;		/* array of block descriptors */
	u8int	*mem;			/* memory for all block descriptors */
	Lump	*free;			/* free list of lumps */

	long hashSize;
};

/*
 * the tag for a block is hash(index, parent tag)
 */

struct Label {
	uchar gen[4];
	uchar state;
	uchar type;		/* top bit indicates it is part of a directory */
	uchar tag[4];		/* tag of file it is in */
};


static char ENoDir[] = "directory entry is not allocated";

static void fixHeap(int si, Lump *b);
static int upHeap(int i, Lump *b);
static int downHeap(int i, Lump *b);
static char	*lumpState(int);
static void	lumpSetState(Lump *u, int state);

Cache *
cacheAlloc(VtSession *z, int blockSize, long nblocks)
{
	int i;
	Cache *c;
	Lump *b;

	c = vtMemAllocZ(sizeof(Cache));
	
	c->lk = vtLockAlloc();
	c->z = z;
	c->size = blockSize;
	c->nblocks = nblocks;
	c->hashSize = nblocks;
	c->heads = vtMemAllocZ(c->hashSize*sizeof(Lump*));
	c->heap = vtMemAllocZ(nblocks*sizeof(Lump*));
	c->blocks = vtMemAllocZ(nblocks*sizeof(Lump));
	c->mem = vtMemAllocZ(nblocks * blockSize);
	for(i = 0; i < nblocks; i++){
		b = &c->blocks[i];
		b->lk = vtLockAlloc();
		b->c = c;
		b->data = &c->mem[i * blockSize];
		b->addr = i+1;
		b->state = LumpFree;
		b->heap = BadHeap;
		b->next = c->free;
		c->free = b;
	}
	c->nheap = 0;

	return c;
}

long
cacheGetSize(Cache *c)
{
	return c->nblocks;
}

int
cacheGetBlockSize(Cache *c)
{
	return c->size;
}

int
cacheSetSize(Cache *c, long nblocks)
{
	USED(c);
	USED(nblocks);
	return 0;
}

void
cacheFree(Cache *c)
{
	int i;

	for(i = 0; i < c->nblocks; i++){
		assert(c->blocks[i].ref == 0);
		vtLockFree(c->blocks[i].lk);
	}
	vtMemFree(c->heads);
	vtMemFree(c->blocks);
	vtMemFree(c->mem);
	vtMemFree(c);
}

static u32int
hash(Cache *c, uchar score[VtScoreSize], int type)
{
	u32int h;
	uchar *p = score + VtScoreSize-4;

	h = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	h += type;
	return h % c->hashSize;
}

static void
findLump(Cache *c, Lump *bb)
{
	Lump *b, *last;
	int h;

	last = nil;
	h = hash(c, bb->score, bb->type);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(last != b->prev)
			vtFatal("bad prev link");
		if(b == bb)
			return;
		last = b;
	}
	vtFatal("block missing from hash table");
}

void
cacheCheck(Cache *c)
{
	u32int size, now;
	int i, k, refed, free;
	static uchar zero[VtScoreSize];
	Lump *p;

	size = c->size;
	now = c->now;

	free = 0;
	for(p=c->free; p; p=p->next)
		free++;
	for(i = 0; i < c->nheap; i++){
		if(c->heap[i]->heap != i)
			vtFatal("mis-heaped at %d: %d", i, c->heap[i]->heap);
		if(i > 0 && c->heap[(i - 1) >> 1]->used2 - now > c->heap[i]->used2 - now)
			vtFatal("bad heap ordering");
		k = (i << 1) + 1;
		if(k < c->nheap && c->heap[i]->used2 - now > c->heap[k]->used2 - now)
			vtFatal("bad heap ordering");
		k++;
		if(k < c->nheap && c->heap[i]->used2 - now > c->heap[k]->used2 - now)
			vtFatal("bad heap ordering");
	}

	refed = 0;
	for(i = 0; i < c->nblocks; i++){
		if(c->blocks[i].data != &c->mem[i * size])
			vtFatal("mis-blocked at %d", i);
		if(c->blocks[i].ref && c->blocks[i].heap == BadHeap){
			refed++;
		}
		if(memcmp(zero, c->blocks[i].score, VtScoreSize))
			findLump(c, &c->blocks[i]);
	}
if(refed > 0)fprint(2, "cacheCheck: nheap %d refed %d free %d\n", c->nheap, refed, free);
	assert(c->nheap + refed + free == c->nblocks);
	refed = 0;
	for(i = 0; i < c->nblocks; i++){
		if(c->blocks[i].ref) {
if(1)fprint(2, "%d %V %d %s\n", c->blocks[i].type, c->blocks[i].score, c->blocks[i].ref, lumpState(c->blocks[i].state));
			refed++;
		}
	}
if(refed > 0)fprint(2, "cacheCheck: in used %d\n", refed);
}

/*
 * delete an arbitrary block from the heap
 */
static void
delHeap(Lump *db)
{
	fixHeap(db->heap, db->c->heap[--db->c->nheap]);
	db->heap = BadHeap;
}

static void
fixHeap(int si, Lump *b)
{
	int i;

	i = upHeap(si, b);
	if(i == si)
		downHeap(i, b);
}

static int
upHeap(int i, Lump *b)
{
	Lump *bb;
	u32int now;
	int p;
	Cache *c;
	
	c = b->c;
	now = c->now;
	for(; i != 0; i = p){
		p = (i - 1) >> 1;
		bb = c->heap[p];
		if(b->used2 - now >= bb->used2 - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;

	return i;
}

static int
downHeap(int i, Lump *b)
{
	Lump *bb;
	u32int now;
	int k;
	Cache *c;
	
	c = b->c;
	now = c->now;
	for(; ; i = k){
		k = (i << 1) + 1;
		if(k >= c->nheap)
			break;
		if(k + 1 < c->nheap && c->heap[k]->used2 - now > c->heap[k + 1]->used2 - now)
			k++;
		bb = c->heap[k];
		if(b->used2 - now <= bb->used2 - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;
	return i;
}


/* called with c->lk held */
Lump *
cacheBumpLump(Cache *c)
{
	Lump *b;

	/*
	 * missed: locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	if(c->free) {
		b = c->free;
		c->free = b->next;
	} else {
		for(;;){
			if(c->nheap == 0) {
				cacheCheck(c);
				assert(0);
				return nil;
			}
			b = c->heap[0];
			delHeap(b);
			if(b->ref == 0)
				break;
		}

		/*
		 * unchain the block from hash chain
		 */
		if(b->prev == nil)
			c->heads[hash(c, b->score, b->type)] = b->next;
		else
			b->prev->next = b->next;
		if(b->next != nil)
			b->next->prev = b->prev;

	}

	/*
	 * the new block has no last use, so assume it happens sometime in the middle
	 */
	b->used = (b->used2 + c->now) / 2;
	b->asize = 0;

	return b;
}

Lump *
cacheAllocLump(Cache *c, int type, int size, int dir)
{
	Lump *b;
	ulong h;

	assert(size <= c->size);

again:
	vtLock(c->lk);
	b = cacheBumpLump(c);
	if(b == nil) {
		vtUnlock(c->lk);
fprint(2, "cache is full\n");
		/* XXX should be better */
		sleep(100);
		goto again;
	}

	vtLock(b->lk);

	assert(b->ref == 0);
	b->ref++;
	b->used2 = b->used;
	b->used = c->now++;

	/* convert addr into score */
	memset(b->score, 0, VtScoreSize-4);
	b->score[VtScoreSize-4] = b->addr>>24;
	b->score[VtScoreSize-3] = b->addr>>16;
	b->score[VtScoreSize-2] = b->addr>>8;
	b->score[VtScoreSize-1] = b->addr;
	
	b->dir = dir;
	b->type = type;
	b->gen = 0;
	b->asize = size;
	b->state = LumpFree;

	h = hash(c, b->score, b->type);

	/* chain onto correct hash */
	b->next = c->heads[h];
	c->heads[h] = b;
	if(b->next != nil)
		b->next->prev = b;
	b->prev = nil;

	vtUnlock(c->lk);

	vtZeroExtend(type, b->data, 0, size);
	lumpSetState(b, LumpActive);

	return b;
}

int
scoreIsLocal(uchar score[VtScoreSize])
{
	static uchar zero[VtScoreSize];
	
	return memcmp(score, zero, VtScoreSize-4) == 0;
}

Lump *
cacheGetLump(Cache *c, uchar score[VtScoreSize], int type, int size)
{
	Lump *b;
	ulong h;
	int n;
	static uchar zero[VtScoreSize];

	assert(size <= c->size);

	h = hash(c, score, type);

again:
	/*
	 * look for the block in the cache
	 */
	vtLock(c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(memcmp(b->score, score, VtScoreSize) == 0 && b->type == type)
			goto found;
	}

	/* should not be looking for a temp block */
	if(scoreIsLocal(score)) {
		if(memcmp(score, zero, VtScoreSize) == 0)
			vtSetError("looking for zero score");
		else
			vtSetError("missing local block");
		vtUnlock(c->lk);
		return nil;
	}

	b = cacheBumpLump(c);
	if(b == nil) {
		vtUnlock(c->lk);
		sleep(100);
		goto again;
	}

	/* chain onto correct hash */
	b->next = c->heads[h];
	c->heads[h] = b;
	if(b->next != nil)
		b->next->prev = b;
	b->prev = nil;

	memmove(b->score, score, VtScoreSize);	
	b->type = type;
	b->state = LumpFree;

found:
	b->ref++;
	b->used2 = b->used;
	b->used = c->now++;
	if(b->heap != BadHeap)
		fixHeap(b->heap, b);

	vtUnlock(c->lk);

	vtLock(b->lk);
	if(b->state != LumpFree)
		return b;
	
	n = vtRead(c->z, score, type, b->data, size);
	if(n < 0) {
		lumpDecRef(b, 1);
		return nil;
	}
	if(!vtSha1Check(score, b->data, n)) {
		vtSetError("vtSha1Check failed");
		lumpDecRef(b, 1);
		return nil;
	}
	vtZeroExtend(type, b->data, n, size);
	b->asize = size;
	lumpSetState(b, LumpVenti);

	return b;
}

static char *
lumpState(int state)
{
	switch(state) {
	default:
		return "Unknown!!";
	case LumpFree:
		return "Free";
	case LumpActive:
		return "Active";
	case LumpSnap:
		return "Snap";
	case LumpZombie:
		return "Zombie";
	case LumpVenti:
		return "Venti";
	}
}

static void
lumpSetState(Lump *u, int state)
{
//	if(u->state != LumpFree)
//		fprint(2, "%V: %s -> %s\n", u->score, lumpState(u->state), lumpState(state));
	u->state = state;
}
	
int
lumpGetScore(Lump *u, int offset, uchar score[VtScoreSize])
{
	uchar *sp;
	VtRoot root;
	VtEntry dir;

	vtLock(u->lk);

	switch(u->type) {
	default:
		vtSetError("bad type");
		goto Err;
	case VtPointerType0:
	case VtPointerType1:
	case VtPointerType2:
	case VtPointerType3:
	case VtPointerType4:
	case VtPointerType5:
	case VtPointerType6:
		if((offset+1)*VtScoreSize > u->asize)
			sp = nil;
		else
			sp = u->data + offset*VtScoreSize;
		break;
	case VtRootType:
		if(u->asize < VtRootSize) {
			vtSetError("runt root block");
			goto Err;
		}
		if(!vtRootUnpack(&root, u->data))
			goto Err;
		sp = root.score;
		break;
	case VtDirType:
		if((offset+1)*VtEntrySize > u->asize) {
			vtSetError(ENoDir);
			goto Err;
		}
		if(!vtEntryUnpack(&dir, u->data, offset))
			goto Err;
		if(!dir.flags & VtEntryActive) {
			vtSetError(ENoDir);
			goto Err;
		}
		sp = dir.score;
		break;
	}

	if(sp == nil)
		memmove(score, vtZeroScore, VtScoreSize);
	else
		memmove(score, sp, VtScoreSize);

	vtUnlock(u->lk);
	return !scoreIsLocal(score);
Err:
	vtUnlock(u->lk);
	return 0;
}

Lump *
lumpWalk(Lump *u, int offset, int type, int size, int readOnly, int lock)
{
	Lump *v, *vv;
	Cache *c;
	uchar score[VtScoreSize], *sp;
	VtRoot root;
	VtEntry dir;
	int split, isdir;

	c = u->c;
	vtLock(u->lk);

Again:
	v = nil;
	vv = nil;

	isdir = u->dir;
	switch(u->type) {
	default:
		vtSetError("bad type");
		goto Err;
	case VtPointerType0:
	case VtPointerType1:
	case VtPointerType2:
	case VtPointerType3:
	case VtPointerType4:
	case VtPointerType5:
	case VtPointerType6:
		if((offset+1)*VtScoreSize > u->asize)
			sp = nil;
		else
			sp = u->data + offset*VtScoreSize;
		break;
	case VtRootType:
		if(u->asize < VtRootSize) {
			vtSetError("runt root block");
			goto Err;
		}
		if(!vtRootUnpack(&root, u->data))
			goto Err;
		sp = root.score;
		break;
	case VtDirType:
		if((offset+1)*VtEntrySize > u->asize) {
			vtSetError(ENoDir);
			goto Err;
		}
		if(!vtEntryUnpack(&dir, u->data, offset))
			goto Err;
		if(!(dir.flags & VtEntryActive)) {
			vtSetError(ENoDir);
			goto Err;
		}
		isdir = (dir.flags & VtEntryDir) != 0;
//		sp = dir.score;
		sp = u->data + offset*VtEntrySize + 20;
		break;
	}

	if(sp == nil)
		memmove(score, vtZeroScore, VtScoreSize);
	else
		memmove(score, sp, VtScoreSize);
	
	vtUnlock(u->lk);


if(0)fprint(2, "lumpWalk: %V:%s %d:%d-> %V:%d\n", u->score, lumpState(u->state), u->type, offset, score, type);
	v = cacheGetLump(c, score, type, size);
	if(v == nil)
		return nil;

	split = 1;
	if(readOnly)
		split = 0;

	switch(v->state) {
	default:
		assert(0);
	case LumpFree:
fprint(2, "block is free %V!\n", v->score);
		vtSetError("phase error");
		goto Err2;
	case LumpActive:	
		if(v->gen < u->gen) {
print("LumpActive gen\n");
			lumpSetState(v, LumpSnap);
			v->gen = u->gen;
		} else
			split = 0;
		break;
	case LumpSnap:
	case LumpVenti:
		break;
	}
	
	/* easy case */
	if(!split) {
		if(!lock)
			vtUnlock(v->lk);
		return v;
	}

	if(sp == nil) {
		vtSetError("bad offset");
		goto Err2;
	}

	vv = cacheAllocLump(c, v->type, size, isdir);
	/* vv is locked */
	vv->gen = u->gen;
	memmove(vv->data, v->data, v->asize);
if(0)fprint(2, "split %V into %V\n", v->score, vv->score);

	lumpDecRef(v, 1);
	v = nil;

	vtLock(u->lk);
	if(u->state != LumpActive) {
		vtSetError("bad parent state: can not happen");
		goto Err;
	}

	/* check that nothing changed underfoot */
	if(memcmp(sp, score, VtScoreSize) != 0) {
		lumpDecRef(vv, 1);
fprint(2, "lumpWalk: parent changed under foot\n");
		goto Again;
	}

	/* XXX - hold Active blocks up - will go eventually */
	lumpIncRef(vv);

	/* change the parent */
	memmove(sp, vv->score, VtScoreSize);
	
	vtUnlock(u->lk);
	
	if(!lock)
		vtUnlock(vv->lk);
	return vv;
Err:
	vtUnlock(u->lk);
	lumpDecRef(v, 0);
	lumpDecRef(vv, 1);
	return nil;
Err2:
	lumpDecRef(v, 1);
	return nil;
	
}

void
lumpFreeEntry(Lump *u, int entry)
{
	uchar score[VtScoreSize];
	int type;
	ulong gen;
	VtEntry dir;
	Cache *c;

	c = u->c;
	vtLock(u->lk);
	if(u->state == LumpVenti)
		goto Exit;

	switch(u->type) {
	default:
		fprint(2, "freeing bad lump type: %d\n", u->type);
		return;
	case VtPointerType0:
		if((entry+1)*VtScoreSize > u->asize)
			goto Exit;
		memmove(score, u->data + entry*VtScoreSize, VtScoreSize);
		memmove(u->data + entry*VtScoreSize, vtZeroScore, VtScoreSize);
		type = u->dir?VtDirType:VtDataType;
		break;
	case VtPointerType1:
	case VtPointerType2:
	case VtPointerType3:
	case VtPointerType4:
	case VtPointerType5:
	case VtPointerType6:
		if((entry+1)*VtScoreSize > u->asize)
			goto Exit;
		memmove(score, u->data + entry*VtScoreSize, VtScoreSize);
		memmove(u->data + entry*VtScoreSize, vtZeroScore, VtScoreSize);
		type = u->type-1;
		break;
	case VtDirType:
		if((entry+1)*VtEntrySize > u->asize)
			goto Exit;
		if(!vtEntryUnpack(&dir, u->data, entry))
			goto Exit;
		if(!dir.flags & VtEntryActive)
			goto Exit;
		gen = dir.gen;
		if(gen != ~0)
			gen++;
		if(dir.depth == 0)
			type = (dir.flags&VtEntryDir)?VtDirType:VtDataType;
		else
			type = VtPointerType0 + dir.depth - 1;
		memmove(score, dir.score, VtScoreSize);
		memset(&dir, 0, sizeof(dir));
		dir.gen = gen;
		vtEntryPack(&dir, u->data, entry);
		break;
	case VtDataType:
		type = VtErrType;
		break;
	}
	vtUnlock(u->lk);
	if(type == VtErrType || !scoreIsLocal(score))
		return;

	u = cacheGetLump(c, score, type, c->size);
	if(u == nil)
		return;
	lumpDecRef(u, 1);
	/* XXX remove extra reference */
	lumpDecRef(u, 0);
	return;
Exit:
	vtUnlock(u->lk);
	return;

}

void
lumpCleanup(Lump *u)
{
	int i, n;

	switch(u->type) {
	default:
		return;
	case VtPointerType0:
	case VtPointerType1:
	case VtPointerType2:
	case VtPointerType3:
	case VtPointerType4:
	case VtPointerType5:
	case VtPointerType6:
		n = u->asize/VtScoreSize;
		break;	
	case VtDirType:
		n = u->asize/VtEntrySize;
		break;
	}

	for(i=0; i<n; i++)
		lumpFreeEntry(u, i);
}


void
lumpDecRef(Lump *b, int unlock)
{
	int i;
	Cache *c;

	if(b == nil)
		return;

	if(unlock)
		vtUnlock(b->lk);

	c = b->c;
	vtLock(c->lk);
	if(--b->ref > 0) {
		vtUnlock(c->lk);
		return;
	}
	assert(b->ref == 0);

	switch(b->state) {
	default:
		fprint(2, "bad state: %s\n", lumpState(b->state));
		assert(0);
	case LumpActive:
		/* hack - but will do for now */
		b->ref++;
		vtUnlock(c->lk);
		lumpCleanup(b);
		vtLock(c->lk);
		b->ref--;
		lumpSetState(b, LumpFree);
		break;
	case LumpZombie:
		lumpSetState(b, LumpFree);
		break;
	case LumpFree:
	case LumpVenti:
		break;
	}

	/*
	 * reinsert in the free heap
	 */
	if(b->heap == BadHeap) {
		i = upHeap(c->nheap++, b);
		c->heap[i] = b;
		b->heap = i;
	}

	vtUnlock(c->lk);
}

Lump *
lumpIncRef(Lump *b)
{
	Cache *c;

	c = b->c;

	vtLock(c->lk);
	assert(b->ref > 0);
	b->ref++;
	vtUnlock(c->lk);
	return b;
}
