/*
 * Index, mapping scores to log positions.  The log data mentioned in
 * the index _always_ goes out to disk before the index blocks themselves.
 * A counter in the arena tail records which logged blocks have been
 * successfully indexed.  The ordering of dirtydcache calls along with
 * the flags passed to dirtydcache ensure the proper write ordering.
 * 
 * For historical reasons, there are two indexing schemes. In both,
 * the index is broken up into some number of fixed-size (say, 8kB)
 * buckets holding index entries.  An index entry is about 40 bytes.
 * The index can be spread across many disks, although in small
 * configurations it is not uncommon for the index and arenas to be
 * on the same disk.
 *  *
 * In the first scheme, the many buckets are treated as a giant on-disk
 * hash table.  If there are N buckets, then the top 32 bits of the
 * score are used as an index into the hash table, with each bucket
 * holding 2^32 / N of the index space.  The index must be sized so
 * that a bucket can't ever overflow.  Assuming that a typical compressed
 * data block is about 4000 bytes, the index size is expected to be
 * about 1% of the total data size.  Since scores are essentially
 * random, they will be distributed evenly among the buckets, so all
 * buckets should be about the same fullness.  A factor of 5 gives us
 * a wide comfort boundary, so the index storage is suggested to be
 * 5% of the total data storage.
 * 
 * Unfortunately, this very sparse index does not make good use of the
 * disk -- most of it is empty, and disk reads, which are costly because
 * of the random seek to get to an arbitrary bucket, tend to bring in
 * only a few entries, making them hardly cost effective.  The second
 * scheme is a variation on the first scheme that tries to lay out the
 * index in a denser format on the disk.  In this scheme, the index
 * buckets are organized in a binary tree, with all data at the leaf
 * nodes.  Bucket numbers are easiest to treat in binary.  In the
 * beginning, there is a single bucket with 0-bit number "".  When a
 * bucket with number x fills, it splits into buckets 0x and 1x.  Since
 * x and 0x are the same number, this means that half the bucket space
 * is assigned to a new bucket, 1x.  So "" splits into 0 and 1, 1
 * splits into 01 and 11, and so on.  The bucket number determines the
 * placement on disk, and the bucket header includes the number of
 * bits represented by the bucket.  To find the right bucket for a
 * given score with top 32-bits x, read bucket "" off disk and check
 * its depth.  If it is zero, we're done.  If x doesn't match the
 * number of bits in 0's header, we know that the block has split, so
 * we use the last 1 bit of x to load a new block (perhaps the same
 * one) and repeat, using successively more bits of x until we find
 * the block responsible for x.  Note that we're using bits from the
 * _right_ not the left.  This gives the "split into 0x and 1x" property
 * needed by the tree and is easier than using the reversal of the
 * bits on the left.
 *  *
 * At the moment, this second scheme sounds worse than the first --
 * there are log n disk reads to find a block instead of just 1.  But
 * we can keep the tree structure in memory, using 1 bit per block to
 * keep track of whether that block has been allocated.  Want to know
 * whether block x has been split?  Check whether 1x is allocated.  1
 * bit per 8kB gives us an in-use bitmap 1/65536 the size of the index.
 * The index data is 1/100 the size of the arena data, explained above.
 * In this scheme, after the first block split, the index is always
 * at least half full (proof by induction), so it is at most 2x the
 * size of the index data.  This gives a bitmap size of 2/6,553,600
 * of the data size.  Let's call that one millionth.  So each terabyte
 * of storage requires one megabyte of free bitmap.  The bitmap is
 * going to be accessed so much that it will be effectively pinned in
 * the cache.  So it still only takes one disk read to find the block
 * -- the tree walking can be done by consulting the in-core bitmap
 * describing the tree structure.
 *  *
 * Now we have to worry about write ordering, though.  What if the
 * bitmap ends up out of sync with the index blocks?  When block x
 * splits into 0x and 1x, causing an update to bitmap block b, the
 * dcache flushing code makes sure that the writes happen in this
 * order: first 1x, then 0x, then the bitmap.  Writing 1x before 0x
 * makes sure we write the split-off entries to disk before we discard
 * them from 0x.  Writing the bitmap after both simplifies the following
 * case analysis.
 * 
 * If Venti is interrupted while flushing blocks to disk, the state
 * of the disk upon next startup can be one of the following:
 *  *

 * (a) none of 0x, 1x, and b are written
 *	Looks like nothing happened - use as is.
 *
 * (b) 1x is written
 *	Since 0x hasn't been rewritten and the bitmap doesn't record 1x
 * 	as being in use, it's like this never happened.  See (a).
 *	This does mean that the bitmap trumps actual disk contents:
 *	no need to zero the index disks anymore.
 *
 * (c) 0x and 1x are written, but not the bitmap
 *	Writing 0x commits the change.  When we next encounter
 *	0x or 1x on a lookup (we can't get to 1x except through x==0x),
 *	the bitmap will direct us to x, we'll load the block and find out
 * 	that its now 0x, so we update the bitmap.
 *
 * (d) 0x, 1x, and b are written.
 *	Great - just use as is.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	bucklook(u8int *score, int type, u8int *data, int n);
static int	writebucket(ISect *is, u32int buck, IBucket *ib, DBlock *b);
static int	okibucket(IBucket *ib, ISect *is);
static ISect	*initisect1(ISect *is);

//static QLock	indexlock;	//ZZZ

static char IndexMagic[] = "venti index configuration";

Index*
initindex(char *name, ISect **sects, int n)
{
	IFile f;
	Index *ix;
	ISect *is;
	u32int last, blocksize, tabsize;
	int i, nbits;

	if(n <= 0){
		seterr(EOk, "no index sections to initialize index");
		return nil;
	}
	ix = MKZ(Index);
	if(ix == nil){
		seterr(EOk, "can't initialize index: out of memory");
		freeindex(ix);
		return nil;
	}

	tabsize = sects[0]->tabsize;
	if(partifile(&f, sects[0]->part, sects[0]->tabbase, tabsize) < 0)
		return nil;
	if(parseindex(&f, ix) < 0){
		freeifile(&f);
		freeindex(ix);
		return nil;
	}
	freeifile(&f);
	if(namecmp(ix->name, name) != 0){
		seterr(ECorrupt, "mismatched index name: found %s expected %s", ix->name, name);
		return nil;
	}
	if(ix->nsects != n){
		seterr(ECorrupt, "mismatched number index sections: found %d expected %d", n, ix->nsects);
		freeindex(ix);
		return nil;
	}
	ix->sects = sects;
	last = 0;
	blocksize = ix->blocksize;
	for(i = 0; i < ix->nsects; i++){
		is = sects[i];
		if(namecmp(ix->name, is->index) != 0
		|| is->blocksize != blocksize
		|| is->tabsize != tabsize
		|| namecmp(is->name, ix->smap[i].name) != 0
		|| is->start != ix->smap[i].start
		|| is->stop != ix->smap[i].stop
		|| last != is->start
		|| is->start > is->stop){
			seterr(ECorrupt, "inconsistent index sections in %s", ix->name);
			freeindex(ix);
			return nil;
		}
		last = is->stop;
	}
	ix->tabsize = tabsize;
	ix->buckets = last;

	/* compute number of buckets used for in-use map */
	nbits = blocksize*8;
	ix->bitbuckets = (ix->buckets+nbits-1)/nbits;

	last -= ix->bitbuckets;
	/* 
	 * compute log of max. power of two not greater than 
	 * number of remaining buckets.
	 */
	for(nbits=0; last>>=1; nbits++)
		;
	ix->maxdepth = nbits;

	if((1UL<<ix->maxdepth) > ix->buckets-ix->bitbuckets){
		seterr(ECorrupt, "inconsistent math for buckets in %s", ix->name);
		freeindex(ix);
		return nil;
	}

	ix->arenas = MKNZ(Arena*, ix->narenas);
	if(maparenas(ix->amap, ix->arenas, ix->narenas, ix->name) < 0){
		freeindex(ix);
		return nil;
	}
	return ix;
}

int
wbindex(Index *ix)
{
	Fmt f;
	ZBlock *b;
	int i;

	if(ix->nsects == 0){
		seterr(EOk, "no sections in index %s", ix->name);
		return -1;
	}
	b = alloczblock(ix->tabsize, 1);
	if(b == nil){
		seterr(EOk, "can't write index configuration: out of memory");
		return -1;
	}
	fmtzbinit(&f, b);
	if(outputindex(&f, ix) < 0){
		seterr(EOk, "can't make index configuration: table storage too small %d", ix->tabsize);
		freezblock(b);
		return -1;
	}
	for(i = 0; i < ix->nsects; i++){
		if(writepart(ix->sects[i]->part, ix->sects[i]->tabbase, b->data, ix->tabsize) < 0){
			seterr(EOk, "can't write index: %r");
			freezblock(b);
			return -1;
		}
	}
	freezblock(b);

	for(i = 0; i < ix->nsects; i++)
		if(wbisect(ix->sects[i]) < 0)
			return -1;

	return 0;
}

/*
 * index: IndexMagic '\n' version '\n' name '\n' blocksize '\n' sections arenas
 * version, blocksize: u32int
 * name: max. ANameSize string
 * sections, arenas: AMap
 */
int
outputindex(Fmt *f, Index *ix)
{
	if(fmtprint(f, "%s\n%ud\n%s\n%ud\n", IndexMagic, ix->version, ix->name, ix->blocksize) < 0
	|| outputamap(f, ix->smap, ix->nsects) < 0
	|| outputamap(f, ix->amap, ix->narenas) < 0)
		return -1;
	return 0;
}

int
parseindex(IFile *f, Index *ix)
{
	AMapN amn;
	u32int v;
	char *s;

	/*
	 * magic
	 */
	s = ifileline(f);
	if(s == nil || strcmp(s, IndexMagic) != 0){
		seterr(ECorrupt, "bad index magic for %s", f->name);
		return -1;
	}

	/*
	 * version
	 */
	if(ifileu32int(f, &v) < 0){
		seterr(ECorrupt, "syntax error: bad version number in %s", f->name);
		return -1;
	}
	ix->version = v;
	if(ix->version != IndexVersion){
		seterr(ECorrupt, "bad version number in %s", f->name);
		return -1;
	}

	/*
	 * name
	 */
	if(ifilename(f, ix->name) < 0){
		seterr(ECorrupt, "syntax error: bad index name in %s", f->name);
		return -1;
	}

	/*
	 * block size
	 */
	if(ifileu32int(f, &v) < 0){
		seterr(ECorrupt, "syntax error: bad version number in %s", f->name);
		return -1;
	}
	ix->blocksize = v;

	if(parseamap(f, &amn) < 0)
		return -1;
	ix->nsects = amn.n;
	ix->smap = amn.map;

	if(parseamap(f, &amn) < 0)
		return -1;
	ix->narenas = amn.n;
	ix->amap = amn.map;

	return 0;
}

/*
 * initialize an entirely new index
 */
Index *
newindex(char *name, ISect **sects, int n)
{
	Index *ix;
	AMap *smap;
	u64int nb;
	u32int div, ub, xb, start, stop, blocksize, tabsize;
	int i, j;

	if(n < 1){
		seterr(EOk, "creating index with no index sections");
		return nil;
	}

	/*
	 * compute the total buckets available in the index,
	 * and the total buckets which are used.
	 */
	nb = 0;
	blocksize = sects[0]->blocksize;
	tabsize = sects[0]->tabsize;
	for(i = 0; i < n; i++){
		if(sects[i]->start != 0 || sects[i]->stop != 0
		|| sects[i]->index[0] != '\0'){
			seterr(EOk, "creating new index using non-empty section %s", sects[i]->name);
			return nil;
		}
		if(blocksize != sects[i]->blocksize){
			seterr(EOk, "mismatched block sizes in index sections");
			return nil;
		}
		if(tabsize != sects[i]->tabsize){
			seterr(EOk, "mismatched config table sizes in index sections");
			return nil;
		}
		nb += sects[i]->blocks;
	}

	/*
	 * check for duplicate names
	 */
	for(i = 0; i < n; i++){
		for(j = i + 1; j < n; j++){
			if(namecmp(sects[i]->name, sects[j]->name) == 0){
				seterr(EOk, "duplicate section name %s for index %s", sects[i]->name, name);
				return nil;
			}
		}
	}

	if(nb >= ((u64int)1 << 32)){
		seterr(EBug, "index too large");
		return nil;
	}
	div = (((u64int)1 << 32) + nb - 1) / nb;
	ub = (((u64int)1 << 32) - 1) / div + 1;
	if(div < 100){
		seterr(EBug, "index divisor too coarse");
		return nil;
	}
	if(ub > nb){
		seterr(EBug, "index initialization math wrong");
		return nil;
	}

	/*
	 * initialize each of the index sections
	 * and the section map table
	 */
	smap = MKNZ(AMap, n);
	if(smap == nil){
		seterr(EOk, "can't create new index: out of memory");
		return nil;
	}
	xb = nb - ub;
	start = 0;
	for(i = 0; i < n; i++){
		stop = start + sects[i]->blocks - xb / n;
		if(i == n - 1)
			stop = ub;
		sects[i]->start = start;
		sects[i]->stop = stop;
		namecp(sects[i]->index, name);

		smap[i].start = start;
		smap[i].stop = stop;
		namecp(smap[i].name, sects[i]->name);
		start = stop;
	}

	/*
	 * initialize the index itself
	 */
	ix = MKZ(Index);
	if(ix == nil){
		seterr(EOk, "can't create new index: out of memory");
		free(smap);
		return nil;
	}
	ix->version = IndexVersion;
	namecp(ix->name, name);
	ix->sects = sects;
	ix->smap = smap;
	ix->nsects = n;
	ix->blocksize = blocksize;
	ix->div = div;
	ix->buckets = ub;
	ix->tabsize = tabsize;
	return ix;
}

ISect*
initisect(Part *part)
{
	ISect *is;
	ZBlock *b;
	int ok;

	b = alloczblock(HeadSize, 0);
	if(b == nil || readpart(part, PartBlank, b->data, HeadSize) < 0){
		seterr(EAdmin, "can't read index section header: %r");
		return nil;
	}

	is = MKZ(ISect);
	if(is == nil){
		freezblock(b);
		return nil;
	}
	is->part = part;
	ok = unpackisect(is, b->data);
	freezblock(b);
	if(ok < 0){
		seterr(ECorrupt, "corrupted index section header: %r");
		freeisect(is);
		return nil;
	}

	if(is->version != ISectVersion){
		seterr(EAdmin, "unknown index section version %d", is->version);
		freeisect(is);
		return nil;
	}

	return initisect1(is);
}

ISect*
newisect(Part *part, char *name, u32int blocksize, u32int tabsize)
{
	ISect *is;
	u32int tabbase;

	is = MKZ(ISect);
	if(is == nil)
		return nil;

	namecp(is->name, name);
	is->version = ISectVersion;
	is->part = part;
	is->blocksize = blocksize;
	is->start = 0;
	is->stop = 0;
	tabbase = (PartBlank + HeadSize + blocksize - 1) & ~(blocksize - 1);
	is->blockbase = (tabbase + tabsize + blocksize - 1) & ~(blocksize - 1);
	is->blocks = is->part->size / blocksize - is->blockbase / blocksize;

	is = initisect1(is);
	if(is == nil)
		return nil;

	return is;
}

/*
 * initialize the computed paramaters for an index
 */
static ISect*
initisect1(ISect *is)
{
	u64int v;

	is->buckmax = (is->blocksize - IBucketSize) / IEntrySize;
	is->blocklog = u64log2(is->blocksize);
	if(is->blocksize != (1 << is->blocklog)){
		seterr(ECorrupt, "illegal non-power-of-2 bucket size %d\n", is->blocksize);
		freeisect(is);
		return nil;
	}
	partblocksize(is->part, is->blocksize);
	is->tabbase = (PartBlank + HeadSize + is->blocksize - 1) & ~(is->blocksize - 1);
	if(is->tabbase >= is->blockbase){
		seterr(ECorrupt, "index section config table overlaps bucket storage");
		freeisect(is);
		return nil;
	}
	is->tabsize = is->blockbase - is->tabbase;
	v = is->part->size & ~(u64int)(is->blocksize - 1);
	if(is->blockbase + (u64int)is->blocks * is->blocksize != v){
		seterr(ECorrupt, "invalid blocks in index section %s", is->name);
//ZZZZZZZZZ
//		freeisect(is);
//		return nil;
	}

	if(is->stop - is->start > is->blocks){
		seterr(ECorrupt, "index section overflows available space");
		freeisect(is);
		return nil;
	}
	if(is->start > is->stop){
		seterr(ECorrupt, "invalid index section range");
		freeisect(is);
		return nil;
	}

	return is;
}

int
wbisect(ISect *is)
{
	ZBlock *b;

	b = alloczblock(HeadSize, 1);
	if(b == nil)
//ZZZ set error?
		return -1;

	if(packisect(is, b->data) < 0){
		seterr(ECorrupt, "can't make index section header: %r");
		freezblock(b);
		return -1;
	}
	if(writepart(is->part, PartBlank, b->data, HeadSize) < 0){
		seterr(EAdmin, "can't write index section header: %r");
		freezblock(b);
		return -1;
	}
	freezblock(b);

	return 0;
}

void
freeisect(ISect *is)
{
	if(is == nil)
		return;
	free(is);
}

void
freeindex(Index *ix)
{
	int i;

	if(ix == nil)
		return;
	free(ix->amap);
	free(ix->arenas);
	if(ix->sects)
		for(i = 0; i < ix->nsects; i++)
			freeisect(ix->sects[i]);
	free(ix->sects);
	free(ix->smap);
	free(ix);
}

/*
 * write a clump to an available arena in the index
 * and return the address of the clump within the index.
ZZZ question: should this distinguish between an arena
filling up and real errors writing the clump?
 */
u64int
writeiclump(Index *ix, Clump *c, u8int *clbuf)
{
	u64int a;
	int i;

	for(i = ix->mapalloc; i < ix->narenas; i++){
		a = writeaclump(ix->arenas[i], c, clbuf);
		if(a != TWID64)
			return a + ix->amap[i].start;
	}

	seterr(EAdmin, "no space left in arenas");
	return TWID64;
}

/*
 * convert an arena index to an relative address address
 */
Arena*
amapitoa(Index *ix, u64int a, u64int *aa)
{
	int i, r, l, m;

	l = 1;
	r = ix->narenas - 1;
	while(l <= r){
		m = (r + l) / 2;
		if(ix->amap[m].start <= a)
			l = m + 1;
		else
			r = m - 1;
	}
	l--;

	if(a > ix->amap[l].stop){
for(i=0; i<ix->narenas; i++)
	print("arena %d: %llux - %llux\n", i, ix->amap[i].start, ix->amap[i].stop);
print("want arena %d for %llux\n", l, a);
		seterr(ECrash, "unmapped address passed to amapitoa");
		return nil;
	}

	if(ix->arenas[l] == nil){
		seterr(ECrash, "unmapped arena selected in amapitoa");
		return nil;
	}
	*aa = a - ix->amap[l].start;
	return ix->arenas[l];
}

int
iaddrcmp(IAddr *ia1, IAddr *ia2)
{
	return ia1->type != ia2->type
		|| ia1->size != ia2->size
		|| ia1->blocks != ia2->blocks
		|| ia1->addr != ia2->addr;
}

/*
 * lookup the score in the partition
 *
 * nothing needs to be explicitly locked:
 * only static parts of ix are used, and
 * the bucket is locked by the DBlock lock.
 */
int
loadientry(Index *ix, u8int *score, int type, IEntry *ie)
{
	ISect *is;
	DBlock *b;
	IBucket ib;
	u32int buck;
	int h, ok;

	buck = hashbits(score, 32) / ix->div;
	ok = -1;

	qlock(&stats.lock);
	stats.indexreads++;
	qunlock(&stats.lock);
	is = findibucket(ix, buck, &buck);
	if(is == nil)
		return -1;
	b = getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), 1);
	if(b == nil)
		return -1;

	unpackibucket(&ib, b->data);
	if(okibucket(&ib, is) < 0)
		goto out;

	h = bucklook(score, type, ib.data, ib.n);
	if(h & 1){
		h ^= 1;
		unpackientry(ie, &ib.data[h]);
		ok = 0;
		goto out;
	}

out:
	putdblock(b);
	return ok;
}

/*
 * insert or update an index entry into the appropriate bucket
 */
int
storeientry(Index *ix, IEntry *ie)
{
	ISect *is;
	DBlock *b;
	IBucket ib;
	u32int buck;
	int h, ok;

	buck = hashbits(ie->score, 32) / ix->div;
	ok = 0;

	qlock(&stats.lock);
	stats.indexwreads++;
	qunlock(&stats.lock);

	is = findibucket(ix, buck, &buck);
	b = getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), 1);
	if(b == nil)
		return -1;

	unpackibucket(&ib, b->data);
	if(okibucket(&ib, is) < 0)
		goto out;

	h = bucklook(ie->score, ie->ia.type, ib.data, ib.n);
	if(h & 1){
		h ^= 1;
		dirtydblock(b, DirtyIndex);
		packientry(ie, &ib.data[h]);
		ok = writebucket(is, buck, &ib, b);
		goto out;
	}

	if(ib.n < is->buckmax){
		dirtydblock(b, DirtyIndex);
		memmove(&ib.data[h + IEntrySize], &ib.data[h], ib.n * IEntrySize - h);
		ib.n++;

		packientry(ie, &ib.data[h]);
		ok = writebucket(is, buck, &ib, b);
		goto out;
	}

out:
	putdblock(b);
	return ok;
}

static int
writebucket(ISect *is, u32int buck, IBucket *ib, DBlock *b)
{
	assert(b->dirty == DirtyIndex);

	if(buck >= is->blocks){
		seterr(EAdmin, "index write out of bounds: %d >= %d\n",
				buck, is->blocks);
		return -1;
	}
	qlock(&stats.lock);
	stats.indexwrites++;
	qunlock(&stats.lock);
	packibucket(ib, b->data);
	// return writepart(is->part, is->blockbase + ((u64int)buck << is->blocklog), b->data, is->blocksize);
	return 0;
}

/*
 * find the number of the index section holding score
 */
int
indexsect(Index *ix, u8int *score)
{
	u32int buck;
	int r, l, m;

	buck = hashbits(score, 32) / ix->div;
	l = 1;
	r = ix->nsects - 1;
	while(l <= r){
		m = (r + l) >> 1;
		if(ix->sects[m]->start <= buck)
			l = m + 1;
		else
			r = m - 1;
	}
	return l - 1;
}

/*
 * find the index section which holds bucket #buck.
 */
static ISect*
findisect(Index *ix, u32int buck, u32int *ibuck)
{
	ISect *is;
	int r, l, m;

	l = 1;
	r = ix->nsects - 1;
	while(l <= r){
		m = (r + l) >> 1;
		if(ix->sects[m]->start <= buck)
			l = m + 1;
		else
			r = m - 1;
	}
	is = ix->sects[l - 1];
	if(is->start <= buck && is->stop > buck){
		*ibuck = buck - is->start;
		return is;
	}
	seterr(EAdmin, "index lookup out of range: %ud not found in index\n", buck);
	return nil;
}

static DBlock*
loadisectblock(Index *ix, u32int buck, int read)
{
	ISect *is;

	if((is = findisect(ix, buck, &buck)) == nil)
		return nil;
	return getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), read);
}

/*
 * find the index section which holds the logical bucket #buck
 */
static DBlock*
loadibucket(Index *ix, u32int buck, IBucket *ib)
{
	int d, i, times;
	u32int ino;
	DBlock *b;
	u32int bbuck;
	IBucket eib;

	times = 0;

top:
	if(times++ > 2*ix->maxdepth){
		seterr(EAdmin, "bucket bitmap tree never converges with buckets");
		return nil;
	}

	bbuck = -1;
	b = nil;

	/*
	 * consider the bits of buck, one at a time, to make the bucket number.
	 */

	/*
	 * walk down the bucket tree using the bitmap, which is used so
	 * often it's almost certain to be in cache.
	 */
	ino = 0;
	for(d=0; d<ix->maxdepth; d++){
		/* fetch the bitmap that says whether ino has been split */
		if(bbuck != (ino>>ix->bitlog)){
			if(b)
				putdblock(b);
			bbuck = (ino>>ix->bitlog);
			if((b = loadisectblock(ix, bbuck, 1)) == nil)
				return nil;
		}
		/* has it been split yet? */
		if((((u32int*)b->data)[(ino&(ix->bitmask))>>5] & (1<<(ino&31))) == 0){
			/* no.  we're done */
			break;
		}
	}
	putdblock(b);

	/*
	 * continue walking down (or up!) the bucket tree, which may not
	 * be completely in sync with the bitmap.  we could continue the loop
	 * here, but it's easiest just to start over once we correct the bitmap.
	 * corrections should only happen when things get out of sync because
	 * a crash keeps some updates from making it to disk, so it's not too
	 * frequent.  we should converge after 2x the max depth, at the very worst
	 * (up and back down the tree).
	 */
	if((b = loadisectblock(ix, ix->bitbuckets+bucketno(buck, d), 1)) == nil)
		return nil;
	unpackibucket(&eib, b->data);
	if(eib.depth > d){
		/* the bitmap thought this block hadn't split */
		putdblock(b);
		if(markblocksplit(buck, d) < 0)
			return nil;
		goto top;
	}
	if(eib.depth < d){
		/* the bitmap thought this block had split */
		putdblock(b);
		if(markblockunsplit(ix, buck, d) < 0)
			return nil;
		goto top;
	}
	*ib = eib;
	return b;
}

static int
markblocksplit(Index *ix, u32int buck, int d)
{
	u32int ino;

	ino = bucketno(buck, d);
	if((b = loadisectblock(ix, ino>>ix->bitlog, 1)) == nil)
		return -1;
	dirtydblock(b, DirtyIndex);
	(((u32int*)b->data)[(ino&(ix->bitmask))>>5] |= (1<<(ino&31));
	putdblock(b);
	return 0;
}

static int
markblockunsplit(Index *ix, u32int buck, int d)
{
	/*
	 * Let's 
	u32int ino;

	ino = bucketno(buck, d);
	
}

static int
okibucket(IBucket *ib, ISect *is)
{
	if(ib->n <= is->buckmax && (ib->next == 0 || ib->next >= is->start && ib->next < is->stop))
		return 0;

	seterr(EICorrupt, "corrupted disk index bucket: n=%ud max=%ud, next=%lud range=[%lud,%lud)",
		ib->n, is->buckmax, ib->next, is->start, is->stop);
	return -1;
}

/*
 * look for score within data;
 * return 1 | byte index of matching index,
 * or 0 | index of least element > score
 */
static int
bucklook(u8int *score, int otype, u8int *data, int n)
{
	int i, r, l, m, h, c, cc, type;

	type = vttodisktype(otype);
	l = 0;
	r = n - 1;
	while(l <= r){
		m = (r + l) >> 1;
		h = m * IEntrySize;
		for(i = 0; i < VtScoreSize; i++){
			c = score[i];
			cc = data[h + i];
			if(c != cc){
				if(c > cc)
					l = m + 1;
				else
					r = m - 1;
				goto cont;
			}
		}
		cc = data[h + IEntryTypeOff];
		if(type != cc){
			if(type > cc)
				l = m + 1;
			else
				r = m - 1;
			goto cont;
		}
		return h | 1;
	cont:;
	}

	return l * IEntrySize;
}

/*
 * compare two IEntries; consistent with bucklook
 */
int
ientrycmp(const void *vie1, const void *vie2)
{
	u8int *ie1, *ie2;
	int i, v1, v2;

	ie1 = (u8int*)vie1;
	ie2 = (u8int*)vie2;
	for(i = 0; i < VtScoreSize; i++){
		v1 = ie1[i];
		v2 = ie2[i];
		if(v1 != v2){
			if(v1 < v2)
				return -1;
			return 0;
		}
	}
	v1 = ie1[IEntryTypeOff];
	v2 = ie2[IEntryTypeOff];
	if(v1 != v2){
		if(v1 < v2)
			return -1;
		return 0;
	}
	return -1;
}
