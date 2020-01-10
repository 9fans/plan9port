#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#include "9.h"	/* for cacheFlush */

typedef struct FreeList FreeList;
typedef struct BAddr BAddr;

enum {
	BadHeap = ~0,
};

/*
 * Store data to the memory cache in c->size blocks
 * with the block zero extended to fill it out.  When writing to
 * Venti, the block will be zero truncated.  The walker will also check
 * that the block fits within psize or dsize as the case may be.
 */

struct Cache
{
	QLock	lk;
	int 	ref;
	int	mode;

	Disk 	*disk;
	int	size;			/* block size */
	int	ndmap;		/* size of per-block dirty pointer map used in blockWrite */
	VtConn *z;
	u32int	now;			/* ticks for usage timestamps */
	Block	**heads;		/* hash table for finding address */
	int	nheap;			/* number of available victims */
	Block	**heap;			/* heap for locating victims */
	long	nblocks;		/* number of blocks allocated */
	Block	*blocks;		/* array of block descriptors */
	u8int	*mem;			/* memory for all block data & blists */

	BList	*blfree;
	Rendez	blrend;

	int 	ndirty;			/* number of dirty blocks in the cache */
	int 	maxdirty;		/* max number of dirty blocks */
	u32int	vers;

	long hashSize;

	FreeList *fl;

	Rendez die;			/* daemon threads should die when QLock != nil */

	Rendez flush;
	Rendez flushwait;
	Rendez heapwait;
	BAddr *baddr;
	int bw, br, be;
	int nflush;

	Periodic *sync;

	/* unlink daemon */
	BList *uhead;
	BList *utail;
	Rendez unlink;

	/* block counts */
	int nused;
	int ndisk;
};

struct BList {
	int part;
	u32int addr;
	uchar type;
	u32int tag;
	u32int epoch;
	u32int vers;

	int recurse;	/* for block unlink */

	/* for roll back */
	int index;			/* -1 indicates not valid */
	union {
		uchar score[VtScoreSize];
		uchar entry[VtEntrySize];
	} old;
	BList *next;
};

struct BAddr {
	int part;
	u32int addr;
	u32int vers;
};

struct FreeList {
	QLock lk;
	u32int last;		/* last block allocated */
	u32int end;		/* end of data partition */
	u32int nused;		/* number of used blocks */
	u32int epochLow;	/* low epoch when last updated nused */
};

static FreeList *flAlloc(u32int end);
static void flFree(FreeList *fl);

static Block *cacheBumpBlock(Cache *c);
static void heapDel(Block*);
static void heapIns(Block*);
static void cacheCheck(Cache*);
static void unlinkThread(void *a);
static void flushThread(void *a);
static void unlinkBody(Cache *c);
static int cacheFlushBlock(Cache *c);
static void cacheSync(void*);
static BList *blistAlloc(Block*);
static void blistFree(Cache*, BList*);
static void doRemoveLink(Cache*, BList*);

/*
 * Mapping from local block type to Venti type
 */
int vtType[BtMax] = {
	VtDataType,		/* BtData | 0  */
	VtDataType+1,		/* BtData | 1  */
	VtDataType+2,		/* BtData | 2  */
	VtDataType+3,		/* BtData | 3  */
	VtDataType+4,		/* BtData | 4  */
	VtDataType+5,		/* BtData | 5  */
	VtDataType+6,		/* BtData | 6  */
	VtDataType+7,		/* BtData | 7  */
	VtDirType,		/* BtDir | 0  */
	VtDirType+1,		/* BtDir | 1  */
	VtDirType+2,		/* BtDir | 2  */
	VtDirType+3,		/* BtDir | 3  */
	VtDirType+4,		/* BtDir | 4  */
	VtDirType+5,		/* BtDir | 5  */
	VtDirType+6,		/* BtDir | 6  */
	VtDirType+7,		/* BtDir | 7  */
};

/*
 * Allocate the memory cache.
 */
Cache *
cacheAlloc(Disk *disk, VtConn *z, ulong nblocks, int mode)
{
	int i;
	Cache *c;
	Block *b;
	BList *bl;
	u8int *p;
	int nbl;

	c = vtmallocz(sizeof(Cache));

	/* reasonable number of BList elements */
	nbl = nblocks * 4;

	c->ref = 1;
	c->disk = disk;
	c->z = z;
	c->size = diskBlockSize(disk);
bwatchSetBlockSize(c->size);
	/* round c->size up to be a nice multiple */
	c->size = (c->size + 127) & ~127;
	c->ndmap = (c->size/20 + 7) / 8;
	c->nblocks = nblocks;
	c->hashSize = nblocks;
	c->heads = vtmallocz(c->hashSize*sizeof(Block*));
	c->heap = vtmallocz(nblocks*sizeof(Block*));
	c->blocks = vtmallocz(nblocks*sizeof(Block));
	c->mem = vtmallocz(nblocks * (c->size + c->ndmap) + nbl * sizeof(BList));
	c->baddr = vtmallocz(nblocks * sizeof(BAddr));
	c->mode = mode;
	c->vers++;
	p = c->mem;
	for(i = 0; i < nblocks; i++){
		b = &c->blocks[i];
		b->c = c;
		b->data = p;
		b->heap = i;
		b->ioready.l = &b->lk;
		c->heap[i] = b;
		p += c->size;
	}
	c->nheap = nblocks;
	for(i = 0; i < nbl; i++){
		bl = (BList*)p;
		bl->next = c->blfree;
		c->blfree = bl;
		p += sizeof(BList);
	}
	/* separate loop to keep blocks and blists reasonably aligned */
	for(i = 0; i < nblocks; i++){
		b = &c->blocks[i];
		b->dmap = p;
		p += c->ndmap;
	}

	c->blrend.l = &c->lk;

	c->maxdirty = nblocks*(DirtyPercentage*0.01);

	c->fl = flAlloc(diskSize(disk, PartData));

	c->unlink.l = &c->lk;
	c->flush.l = &c->lk;
	c->flushwait.l = &c->lk;
	c->heapwait.l = &c->lk;
	c->sync = periodicAlloc(cacheSync, c, 30*1000);

	if(mode == OReadWrite){
		c->ref += 2;
		proccreate(unlinkThread, c, STACK);
		proccreate(flushThread, c, STACK);
	}
	cacheCheck(c);

	return c;
}

/*
 * Free the whole memory cache, flushing all dirty blocks to the disk.
 */
void
cacheFree(Cache *c)
{
	int i;

	/* kill off daemon threads */
	qlock(&c->lk);
	c->die.l = &c->lk;
	periodicKill(c->sync);
	rwakeup(&c->flush);
	rwakeup(&c->unlink);
	while(c->ref > 1)
		rsleep(&c->die);

	/* flush everything out */
	do {
		unlinkBody(c);
		qunlock(&c->lk);
		while(cacheFlushBlock(c))
			;
		diskFlush(c->disk);
		qlock(&c->lk);
	} while(c->uhead || c->ndirty);
	qunlock(&c->lk);

	cacheCheck(c);

	for(i = 0; i < c->nblocks; i++){
		assert(c->blocks[i].ref == 0);
	}
	flFree(c->fl);
	vtfree(c->baddr);
	vtfree(c->heads);
	vtfree(c->blocks);
	vtfree(c->mem);
	diskFree(c->disk);
	/* don't close vtSession */
	vtfree(c);
}

static void
cacheDump(Cache *c)
{
	int i;
	Block *b;

	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		fprint(2, "%d. p=%d a=%ud %V t=%d ref=%d state=%s io=%s pc=%#p\n",
			i, b->part, b->addr, b->score, b->l.type, b->ref,
			bsStr(b->l.state), bioStr(b->iostate), b->pc);
	}
}

static void
cacheCheck(Cache *c)
{
	u32int size, now;
	int i, k, refed;
	Block *b;

	size = c->size;
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
	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		if(b->data != &c->mem[i * size])
			sysfatal("mis-blocked at %d", i);
		if(b->ref && b->heap == BadHeap){
			refed++;
		}
	}
if(c->nheap + refed != c->nblocks){
fprint(2, "%s: cacheCheck: nheap %d refed %d nblocks %ld\n", argv0, c->nheap, refed, c->nblocks);
cacheDump(c);
}
	assert(c->nheap + refed == c->nblocks);
	refed = 0;
	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		if(b->ref){
if(1)fprint(2, "%s: p=%d a=%ud %V ref=%d %L\n", argv0, b->part, b->addr, b->score, b->ref, &b->l);
			refed++;
		}
	}
if(refed > 0)fprint(2, "%s: cacheCheck: in used %d\n", argv0, refed);
}


/*
 * locate the block with the oldest second to last use.
 * remove it from the heap, and fix up the heap.
 */
/* called with c->lk held */
static Block *
cacheBumpBlock(Cache *c)
{
	int printed;
	Block *b;

	/*
	 * locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	printed = 0;
	if(c->nheap == 0){
		while(c->nheap == 0){
			rwakeup(&c->flush);
			rsleep(&c->heapwait);
			if(c->nheap == 0){
				printed = 1;
				fprint(2, "%s: entire cache is busy, %d dirty "
					"-- waking flush thread\n",
					argv0, c->ndirty);
			}
		}
		if(printed)
			fprint(2, "%s: cache is okay again, %d dirty\n",
				argv0, c->ndirty);
	}

	b = c->heap[0];
	heapDel(b);

	assert(b->heap == BadHeap);
	assert(b->ref == 0);
	assert(b->iostate != BioDirty && b->iostate != BioReading && b->iostate != BioWriting);
	assert(b->prior == nil);
	assert(b->uhead == nil);

	/*
	 * unchain the block from hash chain
	 */
	if(b->prev){
		*(b->prev) = b->next;
		if(b->next)
			b->next->prev = b->prev;
		b->prev = nil;
	}


if(0)fprint(2, "%s: dropping %d:%x:%V\n", argv0, b->part, b->addr, b->score);
	/* set block to a reasonable state */
	b->ref = 1;
	b->part = PartError;
	memset(&b->l, 0, sizeof(b->l));
	b->iostate = BioEmpty;

	return b;
}

/*
 * look for a particular version of the block in the memory cache.
 */
static Block *
_cacheLocalLookup(Cache *c, int part, u32int addr, u32int vers,
	int waitlock, int *lockfailure)
{
	Block *b;
	ulong h;

	h = addr % c->hashSize;

	if(lockfailure)
		*lockfailure = 0;

	/*
	 * look for the block in the cache
	 */
	qlock(&c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part == part && b->addr == addr)
			break;
	}
	if(b == nil || b->vers != vers){
		qunlock(&c->lk);
		return nil;
	}
	if(!waitlock && !canqlock(&b->lk)){
		*lockfailure = 1;
		qunlock(&c->lk);
		return nil;
	}
	heapDel(b);
	b->ref++;
	qunlock(&c->lk);

	bwatchLock(b);
	if(waitlock)
		qlock(&b->lk);
	b->nlock = 1;

	for(;;){
		switch(b->iostate){
		default:
			abort();
		case BioEmpty:
		case BioLabel:
		case BioClean:
		case BioDirty:
			if(b->vers != vers){
				blockPut(b);
				return nil;
			}
			return b;
		case BioReading:
		case BioWriting:
			rsleep(&b->ioready);
			break;
		case BioVentiError:
			blockPut(b);
			werrstr("venti i/o error block 0x%.8ux", addr);
			return nil;
		case BioReadError:
			blockPut(b);
			werrstr("error reading block 0x%.8ux", addr);
			return nil;
		}
	}
	/* NOT REACHED */
}


/*
 * fetch a local (on-disk) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 */
Block *
_cacheLocal(Cache *c, int part, u32int addr, int mode, u32int epoch)
{
	Block *b;
	ulong h;

	assert(part != PartVenti);

	h = addr % c->hashSize;

	/*
	 * look for the block in the cache
	 */
	qlock(&c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part != part || b->addr != addr)
			continue;
		if(epoch && b->l.epoch != epoch){
fprint(2, "%s: _cacheLocal want epoch %ud got %ud\n", argv0, epoch, b->l.epoch);
			qunlock(&c->lk);
			werrstr(ELabelMismatch);
			return nil;
		}
		heapDel(b);
		b->ref++;
		break;
	}

	if(b == nil){
		b = cacheBumpBlock(c);

		b->part = part;
		b->addr = addr;
		localToGlobal(addr, b->score);

		/* chain onto correct hash */
		b->next = c->heads[h];
		c->heads[h] = b;
		if(b->next != nil)
			b->next->prev = &b->next;
		b->prev = &c->heads[h];
	}

	qunlock(&c->lk);

	/*
	 * BUG: what if the epoch changes right here?
	 * In the worst case, we could end up in some weird
	 * lock loop, because the block we want no longer exists,
	 * and instead we're trying to lock a block we have no
	 * business grabbing.
	 *
	 * For now, I'm not going to worry about it.
	 */

if(0)fprint(2, "%s: cacheLocal: %d: %d %x\n", argv0, getpid(), b->part, b->addr);
	bwatchLock(b);
	qlock(&b->lk);
	b->nlock = 1;

	if(part == PartData && b->iostate == BioEmpty){
		if(!readLabel(c, &b->l, addr)){
			blockPut(b);
			return nil;
		}
		blockSetIOState(b, BioLabel);
	}
	if(epoch && b->l.epoch != epoch){
		blockPut(b);
fprint(2, "%s: _cacheLocal want epoch %ud got %ud\n", argv0, epoch, b->l.epoch);
		werrstr(ELabelMismatch);
		return nil;
	}

	b->pc = getcallerpc(&c);
	for(;;){
		switch(b->iostate){
		default:
			abort();
		case BioLabel:
			if(mode == OOverWrite)
				/*
				 * leave iostate as BioLabel because data
				 * hasn't been read.
				 */
				return b;
			/* fall through */
		case BioEmpty:
			diskRead(c->disk, b);
			rsleep(&b->ioready);
			break;
		case BioClean:
		case BioDirty:
			return b;
		case BioReading:
		case BioWriting:
			rsleep(&b->ioready);
			break;
		case BioReadError:
			blockSetIOState(b, BioEmpty);
			blockPut(b);
			werrstr("error reading block 0x%.8ux", addr);
			return nil;
		}
	}
	/* NOT REACHED */
}

Block *
cacheLocal(Cache *c, int part, u32int addr, int mode)
{
	return _cacheLocal(c, part, addr, mode, 0);
}

/*
 * fetch a local (on-disk) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 * check tag and type.
 */
Block *
cacheLocalData(Cache *c, u32int addr, int type, u32int tag, int mode, u32int epoch)
{
	Block *b;

	b = _cacheLocal(c, PartData, addr, mode, epoch);
	if(b == nil)
		return nil;
	if(b->l.type != type || b->l.tag != tag){
		fprint(2, "%s: cacheLocalData: addr=%d type got %d exp %d: tag got %ux exp %ux\n",
			argv0, addr, b->l.type, type, b->l.tag, tag);
		werrstr(ELabelMismatch);
		blockPut(b);
		return nil;
	}
	b->pc = getcallerpc(&c);
	return b;
}

/*
 * fetch a global (Venti) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 * check tag and type if it's really a local block in disguise.
 */
Block *
cacheGlobal(Cache *c, uchar score[VtScoreSize], int type, u32int tag, int mode)
{
	int n;
	Block *b;
	ulong h;
	u32int addr;

	addr = globalToLocal(score);
	if(addr != NilBlock){
		b = cacheLocalData(c, addr, type, tag, mode, 0);
		if(b)
			b->pc = getcallerpc(&c);
		return b;
	}

	h = (u32int)(score[0]|(score[1]<<8)|(score[2]<<16)|(score[3]<<24)) % c->hashSize;

	/*
	 * look for the block in the cache
	 */
	qlock(&c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part != PartVenti || memcmp(b->score, score, VtScoreSize) != 0 || b->l.type != type)
			continue;
		heapDel(b);
		b->ref++;
		break;
	}

	if(b == nil){
if(0)fprint(2, "%s: cacheGlobal %V %d\n", argv0, score, type);

		b = cacheBumpBlock(c);

		b->part = PartVenti;
		b->addr = NilBlock;
		b->l.type = type;
		memmove(b->score, score, VtScoreSize);

		/* chain onto correct hash */
		b->next = c->heads[h];
		c->heads[h] = b;
		if(b->next != nil)
			b->next->prev = &b->next;
		b->prev = &c->heads[h];
	}
	qunlock(&c->lk);

	bwatchLock(b);
	qlock(&b->lk);
	b->nlock = 1;
	b->pc = getcallerpc(&c);

	switch(b->iostate){
	default:
		abort();
	case BioEmpty:
		n = vtread(c->z, score, vtType[type], b->data, c->size);
		if(n < 0 || vtsha1check(score, b->data, n) < 0){
			blockSetIOState(b, BioVentiError);
			blockPut(b);
			werrstr(
			"venti error reading block %V or wrong score: %r",
				score);
			return nil;
		}
		vtzeroextend(vtType[type], b->data, n, c->size);
		blockSetIOState(b, BioClean);
		return b;
	case BioClean:
		return b;
	case BioVentiError:
		blockPut(b);
		werrstr("venti i/o error or wrong score, block %V", score);
		return nil;
	case BioReadError:
		blockPut(b);
		werrstr("error reading block %V", b->score);
		return nil;
	}
	/* NOT REACHED */
}

/*
 * allocate a new on-disk block and load it into the memory cache.
 * BUG: if the disk is full, should we flush some of it to Venti?
 */
static u32int lastAlloc;

Block *
cacheAllocBlock(Cache *c, int type, u32int tag, u32int epoch, u32int epochLow)
{
	FreeList *fl;
	u32int addr;
	Block *b;
	int n, nwrap;
	Label lab;

	n = c->size / LabelSize;
	fl = c->fl;

	qlock(&fl->lk);
	addr = fl->last;
	b = cacheLocal(c, PartLabel, addr/n, OReadOnly);
	if(b == nil){
		fprint(2, "%s: cacheAllocBlock: xxx %r\n", argv0);
		qunlock(&fl->lk);
		return nil;
	}
	nwrap = 0;
	for(;;){
		if(++addr >= fl->end){
			addr = 0;
			if(++nwrap >= 2){
				blockPut(b);
				werrstr("disk is full");
				/*
				 * try to avoid a continuous spew of console
				 * messages.
				 */
				if (fl->last != 0)
					fprint(2, "%s: cacheAllocBlock: xxx1 %r\n",
						argv0);
				fl->last = 0;
				qunlock(&fl->lk);
				return nil;
			}
		}
		if(addr%n == 0){
			blockPut(b);
			b = cacheLocal(c, PartLabel, addr/n, OReadOnly);
			if(b == nil){
				fl->last = addr;
				fprint(2, "%s: cacheAllocBlock: xxx2 %r\n", argv0);
				qunlock(&fl->lk);
				return nil;
			}
		}
		if(!labelUnpack(&lab, b->data, addr%n))
			continue;
		if(lab.state == BsFree)
			goto Found;
		if(lab.state&BsClosed)
		if(lab.epochClose <= epochLow || lab.epoch==lab.epochClose)
			goto Found;
	}
Found:
	blockPut(b);
	b = cacheLocal(c, PartData, addr, OOverWrite);
	if(b == nil){
		fprint(2, "%s: cacheAllocBlock: xxx3 %r\n", argv0);
		return nil;
	}
assert(b->iostate == BioLabel || b->iostate == BioClean);
	fl->last = addr;
	lab.type = type;
	lab.tag = tag;
	lab.state = BsAlloc;
	lab.epoch = epoch;
	lab.epochClose = ~(u32int)0;
	if(!blockSetLabel(b, &lab, 1)){
		fprint(2, "%s: cacheAllocBlock: xxx4 %r\n", argv0);
		blockPut(b);
		return nil;
	}
	vtzeroextend(vtType[type], b->data, 0, c->size);
if(0)diskWrite(c->disk, b);

if(0)fprint(2, "%s: fsAlloc %ud type=%d tag = %ux\n", argv0, addr, type, tag);
	lastAlloc = addr;
	fl->nused++;
	qunlock(&fl->lk);
	b->pc = getcallerpc(&c);
	return b;
}

int
cacheDirty(Cache *c)
{
	return c->ndirty;
}

void
cacheCountUsed(Cache *c, u32int epochLow, u32int *used, u32int *total, u32int *bsize)
{
	int n;
	u32int addr, nused;
	Block *b;
	Label lab;
	FreeList *fl;

	fl = c->fl;
	n = c->size / LabelSize;
	*bsize = c->size;
	qlock(&fl->lk);
	if(fl->epochLow == epochLow){
		*used = fl->nused;
		*total = fl->end;
		qunlock(&fl->lk);
		return;
	}
	b = nil;
	nused = 0;
	for(addr=0; addr<fl->end; addr++){
		if(addr%n == 0){
			blockPut(b);
			b = cacheLocal(c, PartLabel, addr/n, OReadOnly);
			if(b == nil){
				fprint(2, "%s: flCountUsed: loading %ux: %r\n",
					argv0, addr/n);
				break;
			}
		}
		if(!labelUnpack(&lab, b->data, addr%n))
			continue;
		if(lab.state == BsFree)
			continue;
		if(lab.state&BsClosed)
		if(lab.epochClose <= epochLow || lab.epoch==lab.epochClose)
			continue;
		nused++;
	}
	blockPut(b);
	if(addr == fl->end){
		fl->nused = nused;
		fl->epochLow = epochLow;
	}
	*used = nused;
	*total = fl->end;
	qunlock(&fl->lk);
	return;
}

static FreeList *
flAlloc(u32int end)
{
	FreeList *fl;

	fl = vtmallocz(sizeof(*fl));
	fl->last = 0;
	fl->end = end;
	return fl;
}

static void
flFree(FreeList *fl)
{
	vtfree(fl);
}

u32int
cacheLocalSize(Cache *c, int part)
{
	return diskSize(c->disk, part);
}

/*
 * The thread that has locked b may refer to it by
 * multiple names.  Nlock counts the number of
 * references the locking thread holds.  It will call
 * blockPut once per reference.
 */
void
blockDupLock(Block *b)
{
	assert(b->nlock > 0);
	b->nlock++;
}

/*
 * we're done with the block.
 * unlock it.  can't use it after calling this.
 */
void
blockPut(Block* b)
{
	Cache *c;

	if(b == nil)
		return;

if(0)fprint(2, "%s: blockPut: %d: %d %x %d %s\n", argv0, getpid(), b->part, b->addr, c->nheap, bioStr(b->iostate));

	if(b->iostate == BioDirty)
		bwatchDependency(b);

	if(--b->nlock > 0)
		return;

	/*
	 * b->nlock should probably stay at zero while
	 * the block is unlocked, but diskThread and rsleep
	 * conspire to assume that they can just qlock(&b->lk); blockPut(b),
	 * so we have to keep b->nlock set to 1 even
	 * when the block is unlocked.
	 */
	assert(b->nlock == 0);
	b->nlock = 1;
//	b->pc = 0;

	bwatchUnlock(b);
	qunlock(&b->lk);
	c = b->c;
	qlock(&c->lk);

	if(--b->ref > 0){
		qunlock(&c->lk);
		return;
	}

	assert(b->ref == 0);
	switch(b->iostate){
	default:
		b->used = c->now++;
		heapIns(b);
		break;
	case BioEmpty:
	case BioLabel:
		if(c->nheap == 0)
			b->used = c->now++;
		else
			b->used = c->heap[0]->used;
		heapIns(b);
		break;
	case BioDirty:
		break;
	}
	qunlock(&c->lk);
}

/*
 * set the label associated with a block.
 */
Block*
_blockSetLabel(Block *b, Label *l)
{
	int lpb;
	Block *bb;
	u32int a;
	Cache *c;

	c = b->c;

	assert(b->part == PartData);
	assert(b->iostate == BioLabel || b->iostate == BioClean || b->iostate == BioDirty);
	lpb = c->size / LabelSize;
	a = b->addr / lpb;
	bb = cacheLocal(c, PartLabel, a, OReadWrite);
	if(bb == nil){
		blockPut(b);
		return nil;
	}
	b->l = *l;
	labelPack(l, bb->data, b->addr%lpb);
	blockDirty(bb);
	return bb;
}

int
blockSetLabel(Block *b, Label *l, int allocating)
{
	Block *lb;

	lb = _blockSetLabel(b, l);
	if(lb == nil)
		return 0;

	/*
	 * If we're allocating the block, make sure the label (bl)
	 * goes to disk before the data block (b) itself.  This is to help
	 * the blocks that in turn depend on b.
	 *
	 * Suppose bx depends on (must be written out after) b.
	 * Once we write b we'll think it's safe to write bx.
	 * Bx can't get at b unless it has a valid label, though.
	 *
	 * Allocation is the only case in which having a current label
	 * is vital because:
	 *
	 *	- l.type is set at allocation and never changes.
	 *	- l.tag is set at allocation and never changes.
	 *	- l.state is not checked when we load blocks.
	 *	- the archiver cares deeply about l.state being
	 *		BaActive vs. BaCopied, but that's handled
	 *		by direct calls to _blockSetLabel.
	 */

	if(allocating)
		blockDependency(b, lb, -1, nil, nil);
	blockPut(lb);
	return 1;
}

/*
 * Record that bb must be written out before b.
 * If index is given, we're about to overwrite the score/e
 * at that index in the block.  Save the old value so we
 * can write a safer ``old'' version of the block if pressed.
 */
void
blockDependency(Block *b, Block *bb, int index, uchar *score, Entry *e)
{
	BList *p;

	if(bb->iostate == BioClean)
		return;

	/*
	 * Dependencies for blocks containing Entry structures
	 * or scores must always be explained.  The problem with
	 * only explaining some of them is this.  Suppose we have two
	 * dependencies for the same field, the first explained
	 * and the second not.  We try to write the block when the first
	 * dependency is not written but the second is.  We will roll back
	 * the first change even though the second trumps it.
	 */
	if(index == -1 && bb->part == PartData)
		assert(b->l.type == BtData);

	if(bb->iostate != BioDirty){
		fprint(2, "%s: %d:%x:%d iostate is %d in blockDependency\n",
			argv0, bb->part, bb->addr, bb->l.type, bb->iostate);
		abort();
	}

	p = blistAlloc(bb);
	if(p == nil)
		return;

	assert(bb->iostate == BioDirty);
if(0)fprint(2, "%s: %d:%x:%d depends on %d:%x:%d\n", argv0, b->part, b->addr, b->l.type, bb->part, bb->addr, bb->l.type);

	p->part = bb->part;
	p->addr = bb->addr;
	p->type = bb->l.type;
	p->vers = bb->vers;
	p->index = index;
	if(p->index >= 0){
		/*
		 * This test would just be b->l.type==BtDir except
		 * we need to exclude the super block.
		 */
		if(b->l.type == BtDir && b->part == PartData)
			entryPack(e, p->old.entry, 0);
		else
			memmove(p->old.score, score, VtScoreSize);
	}
	p->next = b->prior;
	b->prior = p;
}

/*
 * Mark an in-memory block as dirty.  If there are too many
 * dirty blocks, start writing some out to disk.
 *
 * If there were way too many dirty blocks, we used to
 * try to do some flushing ourselves, but it's just too dangerous --
 * it implies that the callers cannot have any of our priors locked,
 * but this is hard to avoid in some cases.
 */
int
blockDirty(Block *b)
{
	Cache *c;

	c = b->c;

	assert(b->part != PartVenti);

	if(b->iostate == BioDirty)
		return 1;
	assert(b->iostate == BioClean || b->iostate == BioLabel);

	qlock(&c->lk);
	b->iostate = BioDirty;
	c->ndirty++;
	if(c->ndirty > (c->maxdirty>>1))
		rwakeup(&c->flush);
	qunlock(&c->lk);

	return 1;
}

/*
 * We've decided to write out b.  Maybe b has some pointers to blocks
 * that haven't yet been written to disk.  If so, construct a slightly out-of-date
 * copy of b that is safe to write out.  (diskThread will make sure the block
 * remains marked as dirty.)
 */
uchar *
blockRollback(Block *b, uchar *buf)
{
	u32int addr;
	BList *p;
	Super super;

	/* easy case */
	if(b->prior == nil)
		return b->data;

	memmove(buf, b->data, b->c->size);
	for(p=b->prior; p; p=p->next){
		/*
		 * we know p->index >= 0 because blockWrite has vetted this block for us.
		 */
		assert(p->index >= 0);
		assert(b->part == PartSuper || (b->part == PartData && b->l.type != BtData));
		if(b->part == PartSuper){
			assert(p->index == 0);
			superUnpack(&super, buf);
			addr = globalToLocal(p->old.score);
			if(addr == NilBlock){
				fprint(2, "%s: rolling back super block: "
					"bad replacement addr %V\n",
					argv0, p->old.score);
				abort();
			}
			super.active = addr;
			superPack(&super, buf);
			continue;
		}
		if(b->l.type == BtDir)
			memmove(buf+p->index*VtEntrySize, p->old.entry, VtEntrySize);
		else
			memmove(buf+p->index*VtScoreSize, p->old.score, VtScoreSize);
	}
	return buf;
}

/*
 * Try to write block b.
 * If b depends on other blocks:
 *
 *	If the block has been written out, remove the dependency.
 *	If the dependency is replaced by a more recent dependency,
 *		throw it out.
 *	If we know how to write out an old version of b that doesn't
 *		depend on it, do that.
 *
 *	Otherwise, bail.
 */
int
blockWrite(Block *b, int waitlock)
{
	uchar *dmap;
	Cache *c;
	BList *p, **pp;
	Block *bb;
	int lockfail;

	c = b->c;

	if(b->iostate != BioDirty)
		return 1;

	dmap = b->dmap;
	memset(dmap, 0, c->ndmap);
	pp = &b->prior;
	for(p=*pp; p; p=*pp){
		if(p->index >= 0){
			/* more recent dependency has succeeded; this one can go */
			if(dmap[p->index/8] & (1<<(p->index%8)))
				goto ignblock;
		}

		lockfail = 0;
		bb = _cacheLocalLookup(c, p->part, p->addr, p->vers, waitlock,
			&lockfail);
		if(bb == nil){
			if(lockfail)
				return 0;
			/* block not in cache => was written already */
			dmap[p->index/8] |= 1<<(p->index%8);
			goto ignblock;
		}

		/*
		 * same version of block is still in cache.
		 *
		 * the assertion is true because the block still has version p->vers,
		 * which means it hasn't been written out since we last saw it.
		 */
		if(bb->iostate != BioDirty){
			fprint(2, "%s: %d:%x:%d iostate is %d in blockWrite\n",
				argv0, bb->part, bb->addr, bb->l.type, bb->iostate);
			/* probably BioWriting if it happens? */
			if(bb->iostate == BioClean)
				goto ignblock;
		}

		blockPut(bb);

		if(p->index < 0){
			/*
			 * We don't know how to temporarily undo
			 * b's dependency on bb, so just don't write b yet.
			 */
			if(0) fprint(2, "%s: blockWrite skipping %d %x %d %d; need to write %d %x %d\n",
				argv0, b->part, b->addr, b->vers, b->l.type, p->part, p->addr, bb->vers);
			return 0;
		}
		/* keep walking down the list */
		pp = &p->next;
		continue;

ignblock:
		*pp = p->next;
		blistFree(c, p);
		continue;
	}

	/*
	 * DiskWrite must never be called with a double-locked block.
	 * This call to diskWrite is okay because blockWrite is only called
	 * from the cache flush thread, which never double-locks a block.
	 */
	diskWrite(c->disk, b);
	return 1;
}

/*
 * Change the I/O state of block b.
 * Just an assignment except for magic in
 * switch statement (read comments there).
 */
void
blockSetIOState(Block *b, int iostate)
{
	int dowakeup;
	Cache *c;
	BList *p, *q;

if(0) fprint(2, "%s: iostate part=%d addr=%x %s->%s\n", argv0, b->part, b->addr, bioStr(b->iostate), bioStr(iostate));

	c = b->c;

	dowakeup = 0;
	switch(iostate){
	default:
		abort();
	case BioEmpty:
		assert(!b->uhead);
		break;
	case BioLabel:
		assert(!b->uhead);
		break;
	case BioClean:
		bwatchDependency(b);
		/*
		 * If b->prior is set, it means a write just finished.
		 * The prior list isn't needed anymore.
		 */
		for(p=b->prior; p; p=q){
			q = p->next;
			blistFree(c, p);
		}
		b->prior = nil;
		/*
		 * Freeing a block or just finished a write.
		 * Move the blocks from the per-block unlink
		 * queue to the cache unlink queue.
		 */
		if(b->iostate == BioDirty || b->iostate == BioWriting){
			qlock(&c->lk);
			c->ndirty--;
			b->iostate = iostate;	/* change here to keep in sync with ndirty */
			b->vers = c->vers++;
			if(b->uhead){
				/* add unlink blocks to unlink queue */
				if(c->uhead == nil){
					c->uhead = b->uhead;
					rwakeup(&c->unlink);
				}else
					c->utail->next = b->uhead;
				c->utail = b->utail;
				b->uhead = nil;
			}
			qunlock(&c->lk);
		}
		assert(!b->uhead);
		dowakeup = 1;
		break;
	case BioDirty:
		/*
		 * Wrote out an old version of the block (see blockRollback).
		 * Bump a version count, leave it dirty.
		 */
		if(b->iostate == BioWriting){
			qlock(&c->lk);
			b->vers = c->vers++;
			qunlock(&c->lk);
			dowakeup = 1;
		}
		break;
	case BioReading:
	case BioWriting:
		/*
		 * Adding block to disk queue.  Bump reference count.
		 * diskThread decs the count later by calling blockPut.
		 * This is here because we need to lock c->lk to
		 * manipulate the ref count.
		 */
		qlock(&c->lk);
		b->ref++;
		qunlock(&c->lk);
		break;
	case BioReadError:
	case BioVentiError:
		/*
		 * Oops.
		 */
		dowakeup = 1;
		break;
	}
	b->iostate = iostate;
	/*
	 * Now that the state has changed, we can wake the waiters.
	 */
	if(dowakeup)
		rwakeupall(&b->ioready);
}

/*
 * The active file system is a tree of blocks.
 * When we add snapshots to the mix, the entire file system
 * becomes a dag and thus requires a bit more care.
 *
 * The life of the file system is divided into epochs.  A snapshot
 * ends one epoch and begins the next.  Each file system block
 * is marked with the epoch in which it was created (b.epoch).
 * When the block is unlinked from the file system (closed), it is marked
 * with the epoch in which it was removed (b.epochClose).
 * Once we have discarded or archived all snapshots up to
 * b.epochClose, we can reclaim the block.
 *
 * If a block was created in a past epoch but is not yet closed,
 * it is treated as copy-on-write.  Of course, in order to insert the
 * new pointer into the tree, the parent must be made writable,
 * and so on up the tree.  The recursion stops because the root
 * block is always writable.
 *
 * If blocks are never closed, they will never be reused, and
 * we will run out of disk space.  But marking a block as closed
 * requires some care about dependencies and write orderings.
 *
 * (1) If a block p points at a copy-on-write block b and we
 * copy b to create bb, then p must be written out after bb and
 * lbb (bb's label block).
 *
 * (2) We have to mark b as closed, but only after we switch
 * the pointer, so lb must be written out after p.  In fact, we
 * can't even update the in-memory copy, or the cache might
 * mistakenly give out b for reuse before p gets written.
 *
 * CacheAllocBlock's call to blockSetLabel records a "bb after lbb" dependency.
 * The caller is expected to record a "p after bb" dependency
 * to finish (1), and also expected to call blockRemoveLink
 * to arrange for (2) to happen once p is written.
 *
 * Until (2) happens, some pieces of the code (e.g., the archiver)
 * still need to know whether a block has been copied, so we
 * set the BsCopied bit in the label and force that to disk *before*
 * the copy gets written out.
 */
Block*
blockCopy(Block *b, u32int tag, u32int ehi, u32int elo)
{
	Block *bb, *lb;
	Label l;

	if((b->l.state&BsClosed) || b->l.epoch >= ehi)
		fprint(2, "%s: blockCopy %#ux %L but fs is [%ud,%ud]\n",
			argv0, b->addr, &b->l, elo, ehi);

	bb = cacheAllocBlock(b->c, b->l.type, tag, ehi, elo);
	if(bb == nil){
		blockPut(b);
		return nil;
	}

	/*
	 * Update label so we know the block has been copied.
	 * (It will be marked closed once it has been unlinked from
	 * the tree.)  This must follow cacheAllocBlock since we
	 * can't be holding onto lb when we call cacheAllocBlock.
	 */
	if((b->l.state&BsCopied)==0)
	if(b->part == PartData){	/* not the superblock */
		l = b->l;
		l.state |= BsCopied;
		lb = _blockSetLabel(b, &l);
		if(lb == nil){
			/* can't set label => can't copy block */
			blockPut(b);
			l.type = BtMax;
			l.state = BsFree;
			l.epoch = 0;
			l.epochClose = 0;
			l.tag = 0;
			blockSetLabel(bb, &l, 0);
			blockPut(bb);
			return nil;
		}
		blockDependency(bb, lb, -1, nil, nil);
		blockPut(lb);
	}

	memmove(bb->data, b->data, b->c->size);
	blockDirty(bb);
	blockPut(b);
	return bb;
}

/*
 * Block b once pointed at the block bb at addr/type/tag, but no longer does.
 * If recurse is set, we are unlinking all of bb's children as well.
 *
 * We can't reclaim bb (or its kids) until the block b gets written to disk.  We add
 * the relevant information to b's list of unlinked blocks.  Once b is written,
 * the list will be queued for processing.
 *
 * If b depends on bb, it doesn't anymore, so we remove bb from the prior list.
 */
void
blockRemoveLink(Block *b, u32int addr, int type, u32int tag, int recurse)
{
	BList *p, **pp, bl;

	/* remove bb from prior list */
	for(pp=&b->prior; (p=*pp)!=nil; ){
		if(p->part == PartData && p->addr == addr){
			*pp = p->next;
			blistFree(b->c, p);
		}else
			pp = &p->next;
	}

	bl.part = PartData;
	bl.addr = addr;
	bl.type = type;
	bl.tag = tag;
	if(b->l.epoch == 0)
		assert(b->part == PartSuper);
	bl.epoch = b->l.epoch;
	bl.next = nil;
	bl.recurse = recurse;

	if(b->part == PartSuper && b->iostate == BioClean)
		p = nil;
	else
		p = blistAlloc(b);
	if(p == nil){
		/*
		 * b has already been written to disk.
		 */
		doRemoveLink(b->c, &bl);
		return;
	}

	/* Uhead is only processed when the block goes from Dirty -> Clean */
	assert(b->iostate == BioDirty);

	*p = bl;
	if(b->uhead == nil)
		b->uhead = p;
	else
		b->utail->next = p;
	b->utail = p;
}

/*
 * Process removal of a single block and perhaps its children.
 */
static void
doRemoveLink(Cache *c, BList *p)
{
	int i, n, recurse;
	u32int a;
	Block *b;
	Label l;
	BList bl;

	recurse = (p->recurse && p->type != BtData && p->type != BtDir);

	/*
	 * We're not really going to overwrite b, but if we're not
	 * going to look at its contents, there is no point in reading
	 * them from the disk.
	 */
	b = cacheLocalData(c, p->addr, p->type, p->tag, recurse ? OReadOnly : OOverWrite, 0);
	if(b == nil)
		return;

	/*
	 * When we're unlinking from the superblock, close with the next epoch.
	 */
	if(p->epoch == 0)
		p->epoch = b->l.epoch+1;

	/* sanity check */
	if(b->l.epoch > p->epoch){
		fprint(2, "%s: doRemoveLink: strange epoch %ud > %ud\n",
			argv0, b->l.epoch, p->epoch);
		blockPut(b);
		return;
	}

	if(recurse){
		n = c->size / VtScoreSize;
		for(i=0; i<n; i++){
			a = globalToLocal(b->data + i*VtScoreSize);
			if(a == NilBlock || !readLabel(c, &l, a))
				continue;
			if(l.state&BsClosed)
				continue;
			/*
			 * If stack space becomes an issue...
			p->addr = a;
			p->type = l.type;
			p->tag = l.tag;
			doRemoveLink(c, p);
			 */

			bl.part = PartData;
			bl.addr = a;
			bl.type = l.type;
			bl.tag = l.tag;
			bl.epoch = p->epoch;
			bl.next = nil;
			bl.recurse = 1;
			/* give up the block lock - share with others */
			blockPut(b);
			doRemoveLink(c, &bl);
			b = cacheLocalData(c, p->addr, p->type, p->tag, OReadOnly, 0);
			if(b == nil){
				fprint(2, "%s: warning: lost block in doRemoveLink\n",
					argv0);
				return;
			}
		}
	}

	l = b->l;
	l.state |= BsClosed;
	l.epochClose = p->epoch;
	if(l.epochClose == l.epoch){
		qlock(&c->fl->lk);
		if(l.epoch == c->fl->epochLow)
			c->fl->nused--;
		blockSetLabel(b, &l, 0);
		qunlock(&c->fl->lk);
	}else
		blockSetLabel(b, &l, 0);
	blockPut(b);
}

/*
 * Allocate a BList so that we can record a dependency
 * or queue a removal related to block b.
 * If we can't find a BList, we write out b and return nil.
 */
static BList *
blistAlloc(Block *b)
{
	Cache *c;
	BList *p;

	if(b->iostate != BioDirty){
		/*
		 * should not happen anymore -
	 	 * blockDirty used to flush but no longer does.
		 */
		assert(b->iostate == BioClean);
		fprint(2, "%s: blistAlloc: called on clean block\n", argv0);
		return nil;
	}

	c = b->c;
	qlock(&c->lk);
	if(c->blfree == nil){
		/*
		 * No free BLists.  What are our options?
		 */

		/* Block has no priors? Just write it. */
		if(b->prior == nil){
			qunlock(&c->lk);
			diskWriteAndWait(c->disk, b);
			return nil;
		}

		/*
		 * Wake the flush thread, which will hopefully free up
		 * some BLists for us.  We used to flush a block from
		 * our own prior list and reclaim that BList, but this is
		 * a no-no: some of the blocks on our prior list may
		 * be locked by our caller.  Or maybe their label blocks
		 * are locked by our caller.  In any event, it's too hard
		 * to make sure we can do I/O for ourselves.  Instead,
		 * we assume the flush thread will find something.
		 * (The flush thread never blocks waiting for a block,
		 * so it can't deadlock like we can.)
		 */
		while(c->blfree == nil){
			rwakeup(&c->flush);
			rsleep(&c->blrend);
			if(c->blfree == nil)
				fprint(2, "%s: flushing for blists\n", argv0);
		}
	}

	p = c->blfree;
	c->blfree = p->next;
	qunlock(&c->lk);
	return p;
}

static void
blistFree(Cache *c, BList *bl)
{
	qlock(&c->lk);
	bl->next = c->blfree;
	c->blfree = bl;
	rwakeup(&c->blrend);
	qunlock(&c->lk);
}

char*
bsStr(int state)
{
	static char s[100];

	if(state == BsFree)
		return "Free";
	if(state == BsBad)
		return "Bad";

	sprint(s, "%x", state);
	if(!(state&BsAlloc))
		strcat(s, ",Free");	/* should not happen */
	if(state&BsCopied)
		strcat(s, ",Copied");
	if(state&BsVenti)
		strcat(s, ",Venti");
	if(state&BsClosed)
		strcat(s, ",Closed");
	return s;
}

char *
bioStr(int iostate)
{
	switch(iostate){
	default:
		return "Unknown!!";
	case BioEmpty:
		return "Empty";
	case BioLabel:
		return "Label";
	case BioClean:
		return "Clean";
	case BioDirty:
		return "Dirty";
	case BioReading:
		return "Reading";
	case BioWriting:
		return "Writing";
	case BioReadError:
		return "ReadError";
	case BioVentiError:
		return "VentiError";
	case BioMax:
		return "Max";
	}
}

static char *bttab[] = {
	"BtData",
	"BtData+1",
	"BtData+2",
	"BtData+3",
	"BtData+4",
	"BtData+5",
	"BtData+6",
	"BtData+7",
	"BtDir",
	"BtDir+1",
	"BtDir+2",
	"BtDir+3",
	"BtDir+4",
	"BtDir+5",
	"BtDir+6",
	"BtDir+7",
};

char*
btStr(int type)
{
	if(type < nelem(bttab))
		return bttab[type];
	return "unknown";
}

int
labelFmt(Fmt *f)
{
	Label *l;

	l = va_arg(f->args, Label*);
	return fmtprint(f, "%s,%s,e=%ud,%d,tag=%#ux",
		btStr(l->type), bsStr(l->state), l->epoch, (int)l->epochClose, l->tag);
}

int
scoreFmt(Fmt *f)
{
	uchar *v;
	int i;
	u32int addr;

	v = va_arg(f->args, uchar*);
	if(v == nil){
		fmtprint(f, "*");
	}else if((addr = globalToLocal(v)) != NilBlock)
		fmtprint(f, "0x%.8ux", addr);
	else{
		for(i = 0; i < VtScoreSize; i++)
			fmtprint(f, "%2.2ux", v[i]);
	}

	return 0;
}

static int
upHeap(int i, Block *b)
{
	Block *bb;
	u32int now;
	int p;
	Cache *c;

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
downHeap(int i, Block *b)
{
	Block *bb;
	u32int now;
	int k;
	Cache *c;

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
heapDel(Block *b)
{
	int i, si;
	Cache *c;

	c = b->c;

	si = b->heap;
	if(si == BadHeap)
		return;
	b->heap = BadHeap;
	c->nheap--;
	if(si == c->nheap)
		return;
	b = c->heap[c->nheap];
	i = upHeap(si, b);
	if(i == si)
		downHeap(i, b);
}

/*
 * Insert a block into the heap.
 * Called with c->lk held.
 */
static void
heapIns(Block *b)
{
	assert(b->heap == BadHeap);
	upHeap(b->c->nheap++, b);
	rwakeup(&b->c->heapwait);
}

/*
 * Get just the label for a block.
 */
int
readLabel(Cache *c, Label *l, u32int addr)
{
	int lpb;
	Block *b;
	u32int a;

	lpb = c->size / LabelSize;
	a = addr / lpb;
	b = cacheLocal(c, PartLabel, a, OReadOnly);
	if(b == nil){
		blockPut(b);
		return 0;
	}

	if(!labelUnpack(l, b->data, addr%lpb)){
		blockPut(b);
		return 0;
	}
	blockPut(b);
	return 1;
}

/*
 * Process unlink queue.
 * Called with c->lk held.
 */
static void
unlinkBody(Cache *c)
{
	BList *p;

	while(c->uhead != nil){
		p = c->uhead;
		c->uhead = p->next;
		qunlock(&c->lk);
		doRemoveLink(c, p);
		qlock(&c->lk);
		p->next = c->blfree;
		c->blfree = p;
	}
}

/*
 * Occasionally unlink the blocks on the cache unlink queue.
 */
static void
unlinkThread(void *a)
{
	Cache *c = a;

	threadsetname("unlink");

	qlock(&c->lk);
	for(;;){
		while(c->uhead == nil && c->die.l == nil)
			rsleep(&c->unlink);
		if(c->die.l != nil)
			break;
		unlinkBody(c);
	}
	c->ref--;
	rwakeup(&c->die);
	qunlock(&c->lk);
}

static int
baddrCmp(const void *a0, const void *a1)
{
	BAddr *b0, *b1;
	b0 = (BAddr*)a0;
	b1 = (BAddr*)a1;

	if(b0->part < b1->part)
		return -1;
	if(b0->part > b1->part)
		return 1;
	if(b0->addr < b1->addr)
		return -1;
	if(b0->addr > b1->addr)
		return 1;
	return 0;
}

/*
 * Scan the block list for dirty blocks; add them to the list c->baddr.
 */
static void
flushFill(Cache *c)
{
	int i, ndirty;
	BAddr *p;
	Block *b;

	qlock(&c->lk);
	if(c->ndirty == 0){
		qunlock(&c->lk);
		return;
	}

	p = c->baddr;
	ndirty = 0;
	for(i=0; i<c->nblocks; i++){
		b = c->blocks + i;
		if(b->part == PartError)
			continue;
		if(b->iostate == BioDirty || b->iostate == BioWriting)
			ndirty++;
		if(b->iostate != BioDirty)
			continue;
		p->part = b->part;
		p->addr = b->addr;
		p->vers = b->vers;
		p++;
	}
	if(ndirty != c->ndirty){
		fprint(2, "%s: ndirty mismatch expected %d found %d\n",
			argv0, c->ndirty, ndirty);
		c->ndirty = ndirty;
	}
	qunlock(&c->lk);

	c->bw = p - c->baddr;
	qsort(c->baddr, c->bw, sizeof(BAddr), baddrCmp);
}

/*
 * This is not thread safe, i.e. it can't be called from multiple threads.
 *
 * It's okay how we use it, because it only gets called in
 * the flushThread.  And cacheFree, but only after
 * cacheFree has killed off the flushThread.
 */
static int
cacheFlushBlock(Cache *c)
{
	Block *b;
	BAddr *p;
	int lockfail, nfail;

	nfail = 0;
	for(;;){
		if(c->br == c->be){
			if(c->bw == 0 || c->bw == c->be)
				flushFill(c);
			c->br = 0;
			c->be = c->bw;
			c->bw = 0;
			c->nflush = 0;
		}

		if(c->br == c->be)
			return 0;
		p = c->baddr + c->br;
		c->br++;
		b = _cacheLocalLookup(c, p->part, p->addr, p->vers, Nowaitlock,
			&lockfail);

		if(b && blockWrite(b, Nowaitlock)){
			c->nflush++;
			blockPut(b);
			return 1;
		}
		if(b)
			blockPut(b);

		/*
		 * Why didn't we write the block?
		 */

		/* Block already written out */
		if(b == nil && !lockfail)
			continue;

		/* Failed to acquire lock; sleep if happens a lot. */
		if(lockfail && ++nfail > 100){
			sleep(500);
			nfail = 0;
		}
		/* Requeue block. */
		if(c->bw < c->be)
			c->baddr[c->bw++] = *p;
	}
}

/*
 * Occasionally flush dirty blocks from memory to the disk.
 */
static void
flushThread(void *a)
{
	Cache *c = a;
	int i;

	threadsetname("flush");
	qlock(&c->lk);
	while(c->die.l == nil){
		rsleep(&c->flush);
		qunlock(&c->lk);
		for(i=0; i<FlushSize; i++)
			if(!cacheFlushBlock(c)){
				/*
				 * If i==0, could be someone is waking us repeatedly
				 * to flush the cache but there's no work to do.
				 * Pause a little.
				 */
				if(i==0){
					// fprint(2, "%s: flushthread found "
					//	"nothing to flush - %d dirty\n",
					//	argv0, c->ndirty);
					sleep(250);
				}
				break;
			}
		if(i==0 && c->ndirty){
			/*
			 * All the blocks are being written right now -- there's nothing to do.
			 * We might be spinning with cacheFlush though -- he'll just keep
			 * kicking us until c->ndirty goes down.  Probably we should sleep
			 * on something that the diskThread can kick, but for now we'll
			 * just pause for a little while waiting for disks to finish.
			 */
			sleep(100);
		}
		qlock(&c->lk);
		rwakeupall(&c->flushwait);
	}
	c->ref--;
	rwakeup(&c->die);
	qunlock(&c->lk);
}

/*
 * Flush the cache.
 */
void
cacheFlush(Cache *c, int wait)
{
	qlock(&c->lk);
	if(wait){
		while(c->ndirty){
		//	consPrint("cacheFlush: %d dirty blocks, uhead %p\n",
		//		c->ndirty, c->uhead);
			rwakeup(&c->flush);
			rsleep(&c->flushwait);
		}
	//	consPrint("cacheFlush: done (uhead %p)\n", c->ndirty, c->uhead);
	}else if(c->ndirty)
		rwakeup(&c->flush);
	qunlock(&c->lk);
}

/*
 * Kick the flushThread every 30 seconds.
 */
static void
cacheSync(void *v)
{
	Cache *c;

	c = v;
	cacheFlush(c, 0);
}
