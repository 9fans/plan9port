#include "stdinc.h"
#include "dat.h"
#include "fns.h"

typedef struct ASum ASum;

struct ASum
{
	Arena	*arena;
	ASum	*next;
};

static void	sealarena(Arena *arena);
static int	okarena(Arena *arena);
static int	loadarena(Arena *arena);
static CIBlock	*getcib(Arena *arena, int clump, int writing, CIBlock *rock);
static void	putcib(Arena *arena, CIBlock *cib);
static void	sumproc(void *);

static QLock	sumlock;
static Rendez	sumwait;
static ASum	*sumq;

int
initarenasum(void)
{
	sumwait.l = &sumlock;

	if(vtproc(sumproc, nil) < 0){
		seterr(EOk, "can't start arena checksum slave: %r");
		return -1;
	}
	return 0;
}

/*
 * make an Arena, and initialize it based upon the disk header and trailer.
 */
Arena*
initarena(Part *part, u64int base, u64int size, u32int blocksize)
{
	Arena *arena;

	arena = MKZ(Arena);
	arena->part = part;
	arena->blocksize = blocksize;
	arena->clumpmax = arena->blocksize / ClumpInfoSize;
	arena->base = base + blocksize;
	arena->size = size - 2 * blocksize;

	if(loadarena(arena) < 0){
		seterr(ECorrupt, "arena header or trailer corrupted");
		freearena(arena);
		return nil;
	}
	if(okarena(arena) < 0){
		freearena(arena);
		return nil;
	}

	if(arena->sealed && scorecmp(zeroscore, arena->score)==0)
		backsumarena(arena);

	return arena;
}

void
freearena(Arena *arena)
{
	if(arena == nil)
		return;
	if(arena->cib.data != nil){
		putdblock(arena->cib.data);
		arena->cib.data = nil;
	}
	free(arena);
}

Arena*
newarena(Part *part, char *name, u64int base, u64int size, u32int blocksize)
{
	Arena *arena;

	if(nameok(name) < 0){
		seterr(EOk, "illegal arena name", name);
		return nil;
	}
	arena = MKZ(Arena);
	arena->part = part;
	arena->version = ArenaVersion;
	arena->blocksize = blocksize;
	arena->clumpmax = arena->blocksize / ClumpInfoSize;
	arena->base = base + blocksize;
	arena->size = size - 2 * blocksize;

	namecp(arena->name, name);

	if(wbarena(arena)<0 || wbarenahead(arena)<0){
		freearena(arena);
		return nil;
	}

	return arena;
}

int
readclumpinfo(Arena *arena, int clump, ClumpInfo *ci)
{
	CIBlock *cib, r;

	cib = getcib(arena, clump, 0, &r);
	if(cib == nil)
		return -1;
	unpackclumpinfo(ci, &cib->data->data[cib->offset]);
	putcib(arena, cib);
	return 0;
}

int
readclumpinfos(Arena *arena, int clump, ClumpInfo *cis, int n)
{
	CIBlock *cib, r;
	int i;

	for(i = 0; i < n; i++){
		cib = getcib(arena, clump + i, 0, &r);
		if(cib == nil)
			break;
		unpackclumpinfo(&cis[i], &cib->data->data[cib->offset]);
		putcib(arena, cib);
	}
	return i;
}

/*
 * write directory information for one clump
 * must be called the arena locked
 */
int
writeclumpinfo(Arena *arena, int clump, ClumpInfo *ci)
{
	CIBlock *cib, r;

	cib = getcib(arena, clump, 1, &r);
	if(cib == nil)
		return -1;
	dirtydblock(cib->data, DirtyArenaCib);
	packclumpinfo(ci, &cib->data->data[cib->offset]);
	putcib(arena, cib);
	return 0;
}

u64int
arenadirsize(Arena *arena, u32int clumps)
{
	return ((clumps / arena->clumpmax) + 1) * arena->blocksize;
}

/*
 * read a clump of data
 * n is a hint of the size of the data, not including the header
 * make sure it won't run off the end, then return the number of bytes actually read
 */
u32int
readarena(Arena *arena, u64int aa, u8int *buf, long n)
{
	DBlock *b;
	u64int a;
	u32int blocksize, off, m;
	long nn;

	if(n == 0)
		return -1;

	qlock(&arena->lock);
	a = arena->size - arenadirsize(arena, arena->clumps);
	qunlock(&arena->lock);
	if(aa >= a){
		seterr(EOk, "reading beyond arena clump storage: clumps=%d aa=%lld a=%lld -1 clumps=%lld\n", arena->clumps, aa, a, arena->size - arenadirsize(arena, arena->clumps - 1));
		return -1;
	}
	if(aa + n > a)
		n = a - aa;

	blocksize = arena->blocksize;
	a = arena->base + aa;
	off = a & (blocksize - 1);
	a -= off;
	nn = 0;
	for(;;){
		b = getdblock(arena->part, a, 1);
		if(b == nil)
			return -1;
		m = blocksize - off;
		if(m > n - nn)
			m = n - nn;
		memmove(&buf[nn], &b->data[off], m);
		putdblock(b);
		nn += m;
		if(nn == n)
			break;
		off = 0;
		a += blocksize;
	}
	return n;
}

/*
 * write some data to the clump section at a given offset
 * used to fix up corrupted arenas.
 */
u32int
writearena(Arena *arena, u64int aa, u8int *clbuf, u32int n)
{
	DBlock *b;
	u64int a;
	u32int blocksize, off, m;
	long nn;
	int ok;

	if(n == 0)
		return -1;

	qlock(&arena->lock);
	a = arena->size - arenadirsize(arena, arena->clumps);
	if(aa >= a || aa + n > a){
		qunlock(&arena->lock);
		seterr(EOk, "writing beyond arena clump storage");
		return -1;
	}

	blocksize = arena->blocksize;
	a = arena->base + aa;
	off = a & (blocksize - 1);
	a -= off;
	nn = 0;
	for(;;){
		b = getdblock(arena->part, a, off != 0 || off + n < blocksize);
		if(b == nil){
			qunlock(&arena->lock);
			return -1;
		}
		dirtydblock(b, DirtyArena);
		m = blocksize - off;
		if(m > n - nn)
			m = n - nn;
		memmove(&b->data[off], &clbuf[nn], m);
		// ok = writepart(arena->part, a, b->data, blocksize);
		ok = 0;
		putdblock(b);
		if(ok < 0){
			qunlock(&arena->lock);
			return -1;
		}
		nn += m;
		if(nn == n)
			break;
		off = 0;
		a += blocksize;
	}
	qunlock(&arena->lock);
	return n;
}

/*
 * allocate space for the clump and write it,
 * updating the arena directory
ZZZ question: should this distinguish between an arena
filling up and real errors writing the clump?
 */
u64int
writeaclump(Arena *arena, Clump *c, u8int *clbuf)
{
	DBlock *b;
	u64int a, aa;
	u32int clump, n, nn, m, off, blocksize;
	int ok;

	n = c->info.size + ClumpSize;
	qlock(&arena->lock);
	aa = arena->used;
	if(arena->sealed
	|| aa + n + U32Size + arenadirsize(arena, arena->clumps + 1) > arena->size){
		if(!arena->sealed)
			sealarena(arena);
		qunlock(&arena->lock);
		return TWID64;
	}
	if(packclump(c, &clbuf[0]) < 0){
		qunlock(&arena->lock);
		return TWID64;
	}

	/*
	 * write the data out one block at a time
	 */
	blocksize = arena->blocksize;
	a = arena->base + aa;
	off = a & (blocksize - 1);
	a -= off;
	nn = 0;
	for(;;){
		b = getdblock(arena->part, a, off != 0);
		if(b == nil){
			qunlock(&arena->lock);
			return TWID64;
		}
		dirtydblock(b, DirtyArena);
		m = blocksize - off;
		if(m > n - nn)
			m = n - nn;
		memmove(&b->data[off], &clbuf[nn], m);
	//	ok = writepart(arena->part, a, b->data, blocksize);
		ok = 0;
		putdblock(b);
		if(ok < 0){
			qunlock(&arena->lock);
			return TWID64;
		}
		nn += m;
		if(nn == n)
			break;
		off = 0;
		a += blocksize;
	}

	arena->used += c->info.size + ClumpSize;
	arena->uncsize += c->info.uncsize;
	if(c->info.size < c->info.uncsize)
		arena->cclumps++;

	clump = arena->clumps++;
	if(arena->clumps == 0)
		sysfatal("clumps wrapped\n");
	arena->wtime = now();
	if(arena->ctime == 0)
		arena->ctime = arena->wtime;

	writeclumpinfo(arena, clump, &c->info);
//ZZZ make this an enum param
	if((clump & 0x1ff) == 0x1ff){
		flushciblocks(arena);
		wbarena(arena);
	}

	qunlock(&arena->lock);
	return aa;
}

/*
 * once sealed, an arena never has any data added to it.
 * it should only be changed to fix errors.
 * this also syncs the clump directory.
 */
static void
sealarena(Arena *arena)
{
	flushciblocks(arena);
	flushdcache();
	arena->sealed = 1;
	wbarena(arena);
	backsumarena(arena);
}

void
backsumarena(Arena *arena)
{
	ASum *as;

	as = MK(ASum);
	if(as == nil)
		return;
	qlock(&sumlock);
	as->arena = arena;
	as->next = sumq;
	sumq = as;
	rwakeup(&sumwait);
	qunlock(&sumlock);
}

static void
sumproc(void *unused)
{
	ASum *as;
	Arena *arena;

	USED(unused);

	for(;;){
		qlock(&sumlock);
		while(sumq == nil)
			rsleep(&sumwait);
		as = sumq;
		sumq = as->next;
		qunlock(&sumlock);
		arena = as->arena;
		free(as);

		sumarena(arena);
	}
}

void
sumarena(Arena *arena)
{
	ZBlock *b;
	DigestState s;
	u64int a, e;
	u32int bs;
	u8int score[VtScoreSize];

	bs = MaxIoSize;
	if(bs < arena->blocksize)
		bs = arena->blocksize;

	/*
	 * read & sum all blocks except the last one
	 */
	memset(&s, 0, sizeof s);
	b = alloczblock(bs, 0);
	e = arena->base + arena->size;
	for(a = arena->base - arena->blocksize; a + arena->blocksize <= e; a += bs){
		if(a + bs > e)
			bs = arena->blocksize;
		if(readpart(arena->part, a, b->data, bs) < 0)
			goto ReadErr;
		sha1(b->data, bs, nil, &s);
	}

	/*
	 * the last one is special, since it may already have the checksum included
	 */
	bs = arena->blocksize;
	if(readpart(arena->part, e, b->data, bs) < 0){
ReadErr:
		logerr(EOk, "sumarena can't sum %s, read at %lld failed: %r", arena->name, a);
		freezblock(b);
		return;
	}

	sha1(b->data, bs-VtScoreSize, nil, &s);
	sha1(zeroscore, VtScoreSize, nil, &s);
	sha1(nil, 0, score, &s);

	/*
	 * check for no checksum or the same
	 *
	 * the writepart is okay because we flushed the dcache in sealarena
	 */
	if(scorecmp(score, &b->data[bs - VtScoreSize]) != 0){
		if(scorecmp(zeroscore, &b->data[bs - VtScoreSize]) != 0)
			logerr(EOk, "overwriting mismatched checksums for arena=%s, found=%V calculated=%V",
				arena->name, &b->data[bs - VtScoreSize], score);
		scorecp(&b->data[bs - VtScoreSize], score);
		if(writepart(arena->part, e, b->data, bs) < 0)
			logerr(EOk, "sumarena can't write sum for %s: %r", arena->name);
	}
	freezblock(b);

	qlock(&arena->lock);
	scorecp(arena->score, score);
	qunlock(&arena->lock);
}

/*
 * write the arena trailer block to the partition
 */
int
wbarena(Arena *arena)
{
	DBlock *b;
	int bad;

	if((b = getdblock(arena->part, arena->base + arena->size, 0)) == nil){
		logerr(EAdmin, "can't write arena trailer: %r");
		return -1;
	}
	dirtydblock(b, DirtyArenaTrailer);
	bad = okarena(arena)<0 || packarena(arena, b->data)<0;
	putdblock(b);
	if(bad)
		return -1;
	return 0;
}

int
wbarenahead(Arena *arena)
{
	ZBlock *b;
	ArenaHead head;
	int bad;

	namecp(head.name, arena->name);
	head.version = arena->version;
	head.size = arena->size + 2 * arena->blocksize;
	head.blocksize = arena->blocksize;
	b = alloczblock(arena->blocksize, 1);
	if(b == nil){
		logerr(EAdmin, "can't write arena header: %r");
///ZZZ add error message?
		return -1;
	}
	/*
	 * this writepart is okay because it only happens
	 * during initialization.
	 */
	bad = packarenahead(&head, b->data)<0 ||
	      writepart(arena->part, arena->base - arena->blocksize, b->data, arena->blocksize)<0;
	freezblock(b);
	if(bad)
		return -1;
	return 0;
}

/*
 * read the arena header and trailer blocks from disk
 */
static int
loadarena(Arena *arena)
{
	ArenaHead head;
	ZBlock *b;

	b = alloczblock(arena->blocksize, 0);
	if(b == nil)
		return -1;
	if(readpart(arena->part, arena->base + arena->size, b->data, arena->blocksize) < 0){
		freezblock(b);
		return -1;
	}
	if(unpackarena(arena, b->data) < 0){
		freezblock(b);
		return -1;
	}
	if(arena->version != ArenaVersion){
		seterr(EAdmin, "unknown arena version %d", arena->version);
		freezblock(b);
		return -1;
	}
	scorecp(arena->score, &b->data[arena->blocksize - VtScoreSize]);

	if(readpart(arena->part, arena->base - arena->blocksize, b->data, arena->blocksize) < 0){
		logerr(EAdmin, "can't read arena header: %r");
		freezblock(b);
		return 0;
	}
	if(unpackarenahead(&head, b->data) < 0)
		logerr(ECorrupt, "corrupted arena header: %r");
	else if(namecmp(arena->name, head.name)!=0
	     || arena->version != head.version
	     || arena->blocksize != head.blocksize
	     || arena->size + 2 * arena->blocksize != head.size)
		logerr(ECorrupt, "arena header inconsistent with arena data");
	freezblock(b);

	return 0;
}

static int
okarena(Arena *arena)
{
	u64int dsize;
	int ok;

	ok = 0;
	dsize = arenadirsize(arena, arena->clumps);
	if(arena->used + dsize > arena->size){
		seterr(ECorrupt, "arena used > size");
		ok = -1;
	}

	if(arena->cclumps > arena->clumps)
		logerr(ECorrupt, "arena has more compressed clumps than total clumps");

	if(arena->uncsize + arena->clumps * ClumpSize + arena->blocksize < arena->used)
		logerr(ECorrupt, "arena uncompressed size inconsistent with used space %lld %d %lld", arena->uncsize, arena->clumps, arena->used);

	if(arena->ctime > arena->wtime)
		logerr(ECorrupt, "arena creation time after last write time");

	return ok;
}

static CIBlock*
getcib(Arena *arena, int clump, int writing, CIBlock *rock)
{
	int read;
	CIBlock *cib;
	u32int block, off;

	if(clump >= arena->clumps){
		seterr(EOk, "clump directory access out of range");
		return nil;
	}
	block = clump / arena->clumpmax;
	off = (clump - block * arena->clumpmax) * ClumpInfoSize;

/*
	if(arena->cib.block == block
	&& arena->cib.data != nil){
		arena->cib.offset = off;
		return &arena->cib;
	}

	if(writing){
		flushciblocks(arena);
		cib = &arena->cib;
	}else
		cib = rock;
*/
	cib = rock;

	qlock(&stats.lock);
	stats.cireads++;
	qunlock(&stats.lock);

	cib->block = block;
	cib->offset = off;

	read = 1;
	if(writing && off == 0 && clump == arena->clumps-1)
		read = 0;

	cib->data = getdblock(arena->part, arena->base + arena->size - (block + 1) * arena->blocksize, read);
	if(cib->data == nil)
		return nil;
	return cib;
}

static void
putcib(Arena *arena, CIBlock *cib)
{
	if(cib != &arena->cib){
		putdblock(cib->data);
		cib->data = nil;
	}
}

/*
 * must be called with arena locked
 * 
 * cache turned off now that dcache does write caching too.
 */
int
flushciblocks(Arena *arena)
{
	int ok;

	if(arena->cib.data == nil)
		return 0;
	qlock(&stats.lock);
	stats.ciwrites++;
	qunlock(&stats.lock);
//	ok = writepart(arena->part, arena->base + arena->size - (arena->cib.block + 1) * arena->blocksize, arena->cib.data->data, arena->blocksize);
	ok = 0;
	if(ok < 0)
		seterr(EAdmin, "failed writing arena directory block");
	putdblock(arena->cib.data);
	arena->cib.data = nil;
	return ok;
}
