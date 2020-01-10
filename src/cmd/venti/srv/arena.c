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
static void loadcig(Arena *arena);

static QLock	sumlock;
static Rendez	sumwait;
static ASum	*sumq;
static ASum	*sumqtail;
static uchar zero[8192];

int	arenasumsleeptime;

int
initarenasum(void)
{
	needzeroscore();  /* OS X */

	qlock(&sumlock);
	sumwait.l = &sumlock;
	qunlock(&sumlock);

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

	if(arena->diskstats.sealed && scorecmp(zeroscore, arena->score)==0)
		sealarena(arena);

	return arena;
}

void
freearena(Arena *arena)
{
	if(arena == nil)
		return;
	free(arena);
}

Arena*
newarena(Part *part, u32int vers, char *name, u64int base, u64int size, u32int blocksize)
{
	int bsize;
	Arena *arena;

	if(nameok(name) < 0){
		seterr(EOk, "illegal arena name", name);
		return nil;
	}
	arena = MKZ(Arena);
	arena->part = part;
	arena->version = vers;
	if(vers == ArenaVersion4)
		arena->clumpmagic = _ClumpMagic;
	else{
		do
			arena->clumpmagic = fastrand();
		while(arena->clumpmagic==_ClumpMagic || arena->clumpmagic==0);
	}
	arena->blocksize = blocksize;
	arena->clumpmax = arena->blocksize / ClumpInfoSize;
	arena->base = base + blocksize;
	arena->size = size - 2 * blocksize;

	namecp(arena->name, name);

	bsize = sizeof zero;
	if(bsize > arena->blocksize)
		bsize = arena->blocksize;

	if(wbarena(arena)<0 || wbarenahead(arena)<0
	|| writepart(arena->part, arena->base, zero, bsize)<0){
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

	/*
	 * because the clump blocks are laid out
	 * in reverse order at the end of the arena,
	 * it can be a few percent faster to read
	 * the clumps backwards, which reads the
	 * disk blocks forwards.
	 */
	for(i = n-1; i >= 0; i--){
		cib = getcib(arena, clump + i, 0, &r);
		if(cib == nil){
			n = i;
			continue;
		}
		unpackclumpinfo(&cis[i], &cib->data->data[cib->offset]);
		putcib(arena, cib);
	}
	return n;
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
	a = arena->size - arenadirsize(arena, arena->memstats.clumps);
	qunlock(&arena->lock);
	if(aa >= a){
		seterr(EOk, "reading beyond arena clump storage: clumps=%d aa=%lld a=%lld -1 clumps=%lld\n", arena->memstats.clumps, aa, a, arena->size - arenadirsize(arena, arena->memstats.clumps - 1));
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
		b = getdblock(arena->part, a, OREAD);
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
	a = arena->size - arenadirsize(arena, arena->memstats.clumps);
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
		b = getdblock(arena->part, a, off != 0 || off + n < blocksize ? ORDWR : OWRITE);
		if(b == nil){
			qunlock(&arena->lock);
			return -1;
		}
		dirtydblock(b, DirtyArena);
		m = blocksize - off;
		if(m > n - nn)
			m = n - nn;
		memmove(&b->data[off], &clbuf[nn], m);
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

	n = c->info.size + ClumpSize + U32Size;
	qlock(&arena->lock);
	aa = arena->memstats.used;
	if(arena->memstats.sealed
	|| aa + n + U32Size + arenadirsize(arena, arena->memstats.clumps + 1) > arena->size){
		if(!arena->memstats.sealed){
			logerr(EOk, "seal memstats %s", arena->name);
			arena->memstats.sealed = 1;
			wbarena(arena);
		}
		qunlock(&arena->lock);
		return TWID64;
	}
	if(packclump(c, &clbuf[0], arena->clumpmagic) < 0){
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
		b = getdblock(arena->part, a, off != 0 ? ORDWR : OWRITE);
		if(b == nil){
			qunlock(&arena->lock);
			return TWID64;
		}
		dirtydblock(b, DirtyArena);
		m = blocksize - off;
		if(m > n - nn)
			m = n - nn;
		memmove(&b->data[off], &clbuf[nn], m);
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

	arena->memstats.used += c->info.size + ClumpSize;
	arena->memstats.uncsize += c->info.uncsize;
	if(c->info.size < c->info.uncsize)
		arena->memstats.cclumps++;

	clump = arena->memstats.clumps;
	if(clump % ArenaCIGSize == 0){
		if(arena->cig == nil){
			loadcig(arena);
			if(arena->cig == nil)
				goto NoCIG;
		}
		/* add aa as start of next cig */
		if(clump/ArenaCIGSize != arena->ncig){
			fprint(2, "bad arena cig computation %s: writing clump %d but %d cigs\n",
				arena->name, clump, arena->ncig);
			arena->ncig = -1;
			vtfree(arena->cig);
			arena->cig = nil;
			goto NoCIG;
		}
		arena->cig = vtrealloc(arena->cig, (arena->ncig+1)*sizeof arena->cig[0]);
		arena->cig[arena->ncig++].offset = aa;
	}
NoCIG:
	arena->memstats.clumps++;

	if(arena->memstats.clumps == 0)
		sysfatal("clumps wrapped");
	arena->wtime = now();
	if(arena->ctime == 0)
		arena->ctime = arena->wtime;

	writeclumpinfo(arena, clump, &c->info);
	wbarena(arena);

	qunlock(&arena->lock);

	return aa;
}

int
atailcmp(ATailStats *a, ATailStats *b)
{
	/* good test */
	if(a->used < b->used)
		return -1;
	if(a->used > b->used)
		return 1;

	/* suspect tests - why order this way? (no one cares) */
	if(a->clumps < b->clumps)
		return -1;
	if(a->clumps > b->clumps)
		return 1;
	if(a->cclumps < b->cclumps)
		return -1;
	if(a->cclumps > b->cclumps)
		return 1;
	if(a->uncsize < b->uncsize)
		return -1;
	if(a->uncsize > b->uncsize)
		return 1;
	if(a->sealed < b->sealed)
		return -1;
	if(a->sealed > b->sealed)
		return 1;

	/* everything matches */
	return 0;
}

void
setatailstate(AState *as)
{
	int i, j, osealed;
	Arena *a;
	Index *ix;

	trace(0, "setatailstate %s 0x%llux clumps %d", as->arena->name, as->aa, as->stats.clumps);

	/*
	 * Look up as->arena to find index.
	 */
	needmainindex();	/* OS X linker */
	ix = mainindex;
	for(i=0; i<ix->narenas; i++)
		if(ix->arenas[i] == as->arena)
			break;
	if(i==ix->narenas || as->aa < ix->amap[i].start || as->aa >= ix->amap[i].stop || as->arena != ix->arenas[i]){
		fprint(2, "funny settailstate 0x%llux\n", as->aa);
		return;
	}

	for(j=0; j<=i; j++){
		a = ix->arenas[j];
		if(atailcmp(&a->diskstats, &a->memstats) == 0)
			continue;
		qlock(&a->lock);
		osealed = a->diskstats.sealed;
		if(j == i)
			a->diskstats = as->stats;
		else
			a->diskstats = a->memstats;
		wbarena(a);
		if(a->diskstats.sealed != osealed && !a->inqueue)
			sealarena(a);
		qunlock(&a->lock);
	}
}

/*
 * once sealed, an arena never has any data added to it.
 * it should only be changed to fix errors.
 * this also syncs the clump directory.
 */
static void
sealarena(Arena *arena)
{
	arena->inqueue = 1;
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
	as->next = nil;
	if(sumq)
		sumqtail->next = as;
	else
		sumq = as;
	sumqtail = as;
	/*
	 * Might get here while initializing arenas,
	 * before initarenasum has been called.
	 */
	if(sumwait.l)
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
	int t;
	u8int score[VtScoreSize];

	bs = MaxIoSize;
	if(bs < arena->blocksize)
		bs = arena->blocksize;

	/*
	 * read & sum all blocks except the last one
	 */
	flushdcache();
	memset(&s, 0, sizeof s);
	b = alloczblock(bs, 0, arena->part->blocksize);
	e = arena->base + arena->size;
	for(a = arena->base - arena->blocksize; a + arena->blocksize <= e; a += bs){
		disksched();
		while((t=arenasumsleeptime) == SleepForever){
			sleep(1000);
			disksched();
		}
		sleep(t);
		if(a + bs > e)
			bs = arena->blocksize;
		if(readpart(arena->part, a, b->data, bs) < 0)
			goto ReadErr;
		addstat(StatSumRead, 1);
		addstat(StatSumReadBytes, bs);
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
	addstat(StatSumRead, 1);
	addstat(StatSumReadBytes, bs);

	sha1(b->data, bs-VtScoreSize, nil, &s);
	sha1(zeroscore, VtScoreSize, nil, &s);
	sha1(nil, 0, score, &s);

	/*
	 * check for no checksum or the same
	 */
	if(scorecmp(score, &b->data[bs - VtScoreSize]) != 0
	&& scorecmp(zeroscore, &b->data[bs - VtScoreSize]) != 0)
		logerr(EOk, "overwriting mismatched checksums for arena=%s, found=%V calculated=%V",
			arena->name, &b->data[bs - VtScoreSize], score);
	freezblock(b);

	qlock(&arena->lock);
	scorecp(arena->score, score);
	wbarena(arena);
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

	if((b = getdblock(arena->part, arena->base + arena->size, OWRITE)) == nil){
		logerr(EAdmin, "can't write arena trailer: %r");
		return -1;
	}
	dirtydblock(b, DirtyArenaTrailer);
	bad = okarena(arena)<0 || packarena(arena, b->data)<0;
	scorecp(b->data + arena->blocksize - VtScoreSize, arena->score);
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
	head.clumpmagic = arena->clumpmagic;
	b = alloczblock(arena->blocksize, 1, arena->part->blocksize);
	if(b == nil){
		logerr(EAdmin, "can't write arena header: %r");
/* ZZZ add error message? */
		return -1;
	}
	/*
	 * this writepart is okay because it only happens
	 * during initialization.
	 */
	bad = packarenahead(&head, b->data)<0 ||
	      writepart(arena->part, arena->base - arena->blocksize, b->data, arena->blocksize)<0 ||
	      flushpart(arena->part)<0;
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

	b = alloczblock(arena->blocksize, 0, arena->part->blocksize);
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
	if(arena->version != ArenaVersion4 && arena->version != ArenaVersion5){
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
	     || arena->clumpmagic != head.clumpmagic
	     || arena->version != head.version
	     || arena->blocksize != head.blocksize
	     || arena->size + 2 * arena->blocksize != head.size){
		if(namecmp(arena->name, head.name)!=0)
			logerr(ECorrupt, "arena tail name %s head %s",
				arena->name, head.name);
		else if(arena->clumpmagic != head.clumpmagic)
			logerr(ECorrupt, "arena tail clumpmagic 0x%lux head 0x%lux",
				(ulong)arena->clumpmagic, (ulong)head.clumpmagic);
		else if(arena->version != head.version)
			logerr(ECorrupt, "arena tail version %d head version %d",
				arena->version, head.version);
		else if(arena->blocksize != head.blocksize)
			logerr(ECorrupt, "arena tail block size %d head %d",
				arena->blocksize, head.blocksize);
		else if(arena->size+2*arena->blocksize != head.size)
			logerr(ECorrupt, "arena tail size %lud head %lud",
				(ulong)arena->size+2*arena->blocksize, head.size);
		else
			logerr(ECorrupt, "arena header inconsistent with arena data");
	}
	freezblock(b);

	return 0;
}

static int
okarena(Arena *arena)
{
	u64int dsize;
	int ok;

	ok = 0;
	dsize = arenadirsize(arena, arena->diskstats.clumps);
	if(arena->diskstats.used + dsize > arena->size){
		seterr(ECorrupt, "arena %s used > size", arena->name);
		ok = -1;
	}

	if(arena->diskstats.cclumps > arena->diskstats.clumps)
		logerr(ECorrupt, "arena %s has more compressed clumps than total clumps", arena->name);

	/*
	 * This need not be true if some of the disk is corrupted.
	 *
	if(arena->diskstats.uncsize + arena->diskstats.clumps * ClumpSize + arena->blocksize < arena->diskstats.used)
		logerr(ECorrupt, "arena %s uncompressed size inconsistent with used space %lld %d %lld", arena->name, arena->diskstats.uncsize, arena->diskstats.clumps, arena->diskstats.used);
	 */

	/*
	 * this happens; it's harmless.
	 *
	if(arena->ctime > arena->wtime)
		logerr(ECorrupt, "arena %s creation time after last write time", arena->name);
	 */
	return ok;
}

static CIBlock*
getcib(Arena *arena, int clump, int writing, CIBlock *rock)
{
	int mode;
	CIBlock *cib;
	u32int block, off;

	if(clump >= arena->memstats.clumps){
		seterr(EOk, "clump directory access out of range");
		return nil;
	}
	block = clump / arena->clumpmax;
	off = (clump - block * arena->clumpmax) * ClumpInfoSize;
	cib = rock;
	cib->block = block;
	cib->offset = off;

	if(writing){
		if(off == 0 && clump == arena->memstats.clumps-1)
			mode = OWRITE;
		else
			mode = ORDWR;
	}else
		mode = OREAD;

	cib->data = getdblock(arena->part,
		arena->base + arena->size - (block + 1) * arena->blocksize, mode);
	if(cib->data == nil)
		return nil;
	return cib;
}

static void
putcib(Arena *arena, CIBlock *cib)
{
	USED(arena);

	putdblock(cib->data);
	cib->data = nil;
}


/*
 * For index entry readahead purposes, the arenas are
 * broken into smaller subpieces, called clump info groups
 * or cigs.  Each cig has ArenaCIGSize clumps (ArenaCIGSize
 * is chosen to make the index entries take up about half
 * a megabyte).  The index entries do not contain enough
 * information to determine what the clump index is for
 * a given address in an arena.  That info is needed both for
 * figuring out which clump group an address belongs to
 * and for prefetching a clump group's index entries from
 * the arena table of contents.  The first time clump groups
 * are accessed, we scan the entire arena table of contents
 * (which might be 10s of megabytes), recording the data
 * offset of each clump group.
 */

/*
 * load clump info group information by scanning entire toc.
 */
static void
loadcig(Arena *arena)
{
	u32int i, j, ncig, nci;
	ArenaCIG *cig;
	ClumpInfo *ci;
	u64int offset;
	int ms;

	if(arena->cig || arena->ncig < 0)
		return;

//	fprint(2, "loadcig %s\n", arena->name);

	ncig = (arena->memstats.clumps+ArenaCIGSize-1) / ArenaCIGSize;
	if(ncig == 0){
		arena->cig = vtmalloc(1);
		arena->ncig = 0;
		return;
	}

	ms = msec();
	cig = vtmalloc(ncig*sizeof cig[0]);
	ci = vtmalloc(ArenaCIGSize*sizeof ci[0]);
	offset = 0;
	for(i=0; i<ncig; i++){
		nci = readclumpinfos(arena, i*ArenaCIGSize, ci, ArenaCIGSize);
		cig[i].offset = offset;
		for(j=0; j<nci; j++)
			offset += ClumpSize + ci[j].size;
		if(nci < ArenaCIGSize){
			if(i != ncig-1){
				vtfree(ci);
				vtfree(cig);
				arena->ncig = -1;
				fprint(2, "loadcig %s: got %ud cigs, expected %ud\n", arena->name, i+1, ncig);
				goto out;
			}
		}
	}
	vtfree(ci);

	arena->ncig = ncig;
	arena->cig = cig;

out:
	ms = msec() - ms;
	addstat2(StatCigLoad, 1, StatCigLoadTime, ms);
}

/*
 * convert arena address into arena group + data boundaries.
 */
int
arenatog(Arena *arena, u64int addr, u64int *gstart, u64int *glimit, int *g)
{
	int r, l, m;

	qlock(&arena->lock);
	if(arena->cig == nil)
		loadcig(arena);
	if(arena->cig == nil || arena->ncig == 0){
		qunlock(&arena->lock);
		return -1;
	}

	l = 1;
	r = arena->ncig - 1;
	while(l <= r){
		m = (r + l) / 2;
		if(arena->cig[m].offset <= addr)
			l = m + 1;
		else
			r = m - 1;
	}
	l--;

	*g = l;
	*gstart = arena->cig[l].offset;
	if(l+1 < arena->ncig)
		*glimit = arena->cig[l+1].offset;
	else
		*glimit = arena->memstats.used;
	qunlock(&arena->lock);
	return 0;
}

/*
 * load the clump info for group g into the index entries.
 */
int
asumload(Arena *arena, int g, IEntry *entries, int nentries)
{
	int i, base, limit;
	u64int addr;
	ClumpInfo ci;
	IEntry *ie;

	if(nentries < ArenaCIGSize){
		fprint(2, "asking for too few entries\n");
		return -1;
	}

	qlock(&arena->lock);
	if(arena->cig == nil)
		loadcig(arena);
	if(arena->cig == nil || arena->ncig == 0 || g >= arena->ncig){
		qunlock(&arena->lock);
		return -1;
	}

	addr = 0;
	base = g*ArenaCIGSize;
	limit = base + ArenaCIGSize;
	if(base > arena->memstats.clumps)
		base = arena->memstats.clumps;
	ie = entries;
	for(i=base; i<limit; i++){
		if(readclumpinfo(arena, i, &ci) < 0)
			break;
		if(ci.type != VtCorruptType){
			scorecp(ie->score, ci.score);
			ie->ia.type = ci.type;
			ie->ia.size = ci.uncsize;
			ie->ia.blocks = (ci.size + ClumpSize + (1<<ABlockLog) - 1) >> ABlockLog;
			ie->ia.addr = addr;
			ie++;
		}
		addr += ClumpSize + ci.size;
	}
	qunlock(&arena->lock);
	return ie - entries;
}
