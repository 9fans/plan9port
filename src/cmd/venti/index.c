/*
 * Index, mapping scores to log positions. 
 *
 * The index is made up of some number of index sections, each of
 * which is typically stored on a different disk.  The blocks in all the 
 * index sections are logically numbered, with each index section 
 * responsible for a range of blocks.  Blocks are typically 8kB.
 *
 * Index Version 1:
 * 
 * The N index blocks are treated as a giant hash table.  The top 32 bits
 * of score are used as the key for a lookup.  Each index block holds
 * one hash bucket, which is responsible for ceil(2^32 / N) of the key space.
 * 
 * The index is sized so that a particular bucket is extraordinarily 
 * unlikely to overflow: assuming compressed data blocks are 4kB 
 * on disk, and assuming each block has a 40 byte index entry,
 * the index data will be 1% of the total data.  Since scores are essentially
 * random, all buckets should be about the same fullness.
 * A factor of 5 gives us a wide comfort boundary to account for 
 * random variation.  So the index disk space should be 5% of the arena disk space.
 *
 * Problems with Index Version 1:
 * 
 * Because the index size is chosen to handle the worst case load,
 * the index is very sparse, especially when the Venti server is mostly empty.
 * This has a few bad properties.
 * 
 * Loading an index block (which typically requires a random disk seek)
 * is a very expensive operation, yet it yields only a couple index entries.
 * We're not making efficient use of the disk arm.
 *
 * Writing a block requires first checking to see if the block already
 * exists on the server, which in turn requires an index lookup.  When
 * writing fresh data, these lookups will fail.  The index entry cache 
 * cannot serve these, so they must go to disk, which is expensive.
 * 
 * Thus both the reading and the writing of blocks are held back by
 * the expense of accessing the index.
 * 
 * Index Version 2:
 * 
 * The index is sized to be exactly 2^M blocks.  The index blocks are 
 * logically arranged in a (not exactly balanced) binary tree of depth at
 * most M.  The nodes are named by bit strings describing the path from
 * the root to the node.  The root is . (dot).  The left child of the root is .0,
 * the right child .1.  The node you get to by starting at the root and going
 * left right right left is .0110.  At the beginning, there is only the root block.
 * When a block with name .xxx fills, it splits into .xxx0 and .xxx1.
 * All the index data is kept in the leaves of the tree.
 *
 * Index leaf blocks are laid out on disk by interpreting the bitstring as a 
 * binary fraction and multiplying by 2^M -- .0 is the first block, .1 is
 * the block halfway into the index, .0110 is at position 6/16, and
 * .xxx and .xxx0 map to the same block (but only one can be a leaf
 * node at any given time, so this is okay!).  A cheap implementation of
 * this is to append zeros to the bit string to make it M bits long.  That's
 * the integer index block number.
 *
 * To find the leaf block that should hold a score, use the bits of the 
 * score one at a time to walk down the tree to a leaf node.  If the tree
 * has leaf nodes .0, .10, and .11, then score 0110101... ends up in bucket
 * .0 while score 101110101... ends up in bucket .10.  There is no leaf node
 * .1 because it has split into .10 and .11.
 *
 * If we know which disk blocks are in use, we can reconstruct the interior
 * of the tree: if .xxx1 is in use, then .xxx has been split.  We keep an in-use
 * bitmap of all index disk blocks to aid in reconstructing the interior of the
 * tree.  At one bit per index block, the bitmap is small enough to be kept
 * in memory even on the largest of Venti servers.
 *
 * After the root block splits, the index blocks being used will always be
 * at least half full (averaged across the entire index).  So unlike Version 1,
 * Index Version 2 is quite dense, alleviating the two problems above.
 * Index block reads now return many index entries.  More importantly,
 * small servers can keep most of the index in the disk cache, making them
 * more likely to handle negative lookups without going to disk.
 *
 * As the index becomes more full, Index Version 2's performance
 * degrades gracefully into Index Version 1.  V2 is really an optimization
 * for little servers.
 *
 * Write Ordering for Index Version 2:
 * 
 * Unlike Index Version 1, Version 2 must worry about write ordering
 * within the index.  What happens if the in-use bitmap is out of sync
 * with the actual leaf nodes?  What happens if .xxx splits into .xxx0 and
 * .xxx1 but only one of the new blocks gets written to disk?
 *
 * We arrange that when .xxx splits, the .xxx1 block is written first,
 * then the .xxx0 block, and finally the in-use bitmap entry for .xxx1.
 * The split is committed by the writing of .xxx0.  This ordering leaves
 * two possible partial disk writes:
 *
 * (a) If .xxx1 is written but .xxx0 and the bitmap are not, then it's as if
 * the split never happened -- we won't think .xxx1 is in use, and we
 * won't go looking for it.
 *
 * (b) If .xxx1 and .xxx0 are written but the bitmap is not, then the first
 * time we try to load .xxx, we'll get .xxx0 instead, realize the bitmap is
 * out of date, and update the bitmap.
 *
 * Backwards Compatibility
 *
 * Because there are large Venti servers in use with Index V1, this code
 * will read either index version, but when asked to create a new index,
 * will only create V2.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	bucklook(u8int *score, int type, u8int *data, int n);
static int	writebucket(ISect *is, u32int buck, IBucket *ib, DBlock *b);
static int	okibucket(IBucket *ib, ISect *is);
static int	initindex1(Index*);
static ISect	*initisect1(ISect *is);
static int	splitiblock(Index *ix, DBlock *b, ISect *is, u32int buck, IBucket *ib);

#define KEY(k,d)	((d) ? (k)>>(32-(d)) : 0)

//static QLock	indexlock;	//ZZZ

static char IndexMagic[] = "venti index configuration";

Index*
initindex(char *name, ISect **sects, int n)
{
	IFile f;
	Index *ix;
	ISect *is;
	u32int last, blocksize, tabsize;
	int i;

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

	if(initindex1(ix) < 0){
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

static int
initindex1(Index *ix)
{
	u32int buckets;

	switch(ix->version){
	case IndexVersion1:
		ix->div = (((u64int)1 << 32) + ix->buckets - 1) / ix->buckets;
		buckets = (((u64int)1 << 32) - 1) / ix->div + 1;
		if(buckets != ix->buckets){
			seterr(ECorrupt, "inconsistent math for divisor and buckets in %s", ix->name);
			return -1;
		}
		break;

	case IndexVersion2:
		buckets = ix->buckets - ix->bitblocks;
		if(ix->buckets < ix->bitblocks || (buckets&(buckets-1)))
			seterr(ECorrupt, "bucket count not a power of two in %s", ix->name);
		ix->maxdepth = u64log2(buckets);
		ix->bitkeylog = u64log2(ix->blocksize*8);
		ix->bitkeymask = (1<<ix->bitkeylog)-1;
		break;
	}
	return 0;
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
 * index: IndexMagic '\n' version '\n' name '\n' blocksize '\n' [V2: bitblocks '\n'] sections arenas
 * version, blocksize: u32int
 * name: max. ANameSize string
 * sections, arenas: AMap
 */
int
outputindex(Fmt *f, Index *ix)
{
	if(fmtprint(f, "%s\n%ud\n%s\n%ud\n", IndexMagic, ix->version, ix->name, ix->blocksize) < 0
	|| (ix->version==IndexVersion2 && fmtprint(f, "%ud\n", ix->bitblocks) < 0)
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
	if(ix->version != IndexVersion1 && ix->version != IndexVersion2){
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
		seterr(ECorrupt, "syntax error: bad block size number in %s", f->name);
		return -1;
	}
	ix->blocksize = v;

	if(ix->version == IndexVersion2){
		/*
		 * free bitmap size
		 */
		if(ifileu32int(f, &v) < 0){
			seterr(ECorrupt, "syntax error: bad bitmap size in %s", f->name);
			return -1;
		}
		ix->bitblocks = v;
	}

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
	u32int div, ub, xb, fb, start, stop, blocksize, tabsize;
	int i, j, version;

	version = IndexVersion2;

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

	div = 0;
	fb = 0;
	if(version == IndexVersion1){
		div = (((u64int)1 << 32) + nb - 1) / nb;
		ub = (((u64int)1 << 32) - 1) / div + 1;
		if(div < 100){
			seterr(EBug, "index divisor too coarse");
			return nil;
		}
	}else{
		fb = (nb+blocksize*8-1)/(blocksize*8);
		for(ub=1; ub<=((nb-fb)>>1); ub<<=1)
			;
		ub += fb;
	}
	if(ub > nb){
		seterr(EBug, "index initialization math wrong");
		return nil;
	}
	xb = nb - ub;

	/*
	 * initialize each of the index sections
	 * and the section map table
	 */
	smap = MKNZ(AMap, n);
	if(smap == nil){
		seterr(EOk, "can't create new index: out of memory");
		return nil;
	}
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
	ix->version = version;
	namecp(ix->name, name);
	ix->sects = sects;
	ix->smap = smap;
	ix->nsects = n;
	ix->blocksize = blocksize;
	ix->buckets = ub;
	ix->tabsize = tabsize;
	ix->div = div;
	ix->bitblocks = fb;

	if(initindex1(ix) < 0){
		free(smap);
		return nil;
	}

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
 * initialize the computed parameters for an index
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
 * convert an arena index to an relative arena address
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

	ok = -1;

	qlock(&stats.lock);
	stats.indexreads++;
	qunlock(&stats.lock);
	b = loadibucket(ix, score, &is, &buck, &ib);
	if(b == nil)
		return -1;

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

	ok = 0;

	qlock(&stats.lock);
	stats.indexwreads++;
	qunlock(&stats.lock);

top:
	b = loadibucket(ix, ie->score, &is, &buck, &ib);
	if(b == nil)
		return -1;

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

	/* block is full -- not supposed to happen in V1 */
	if(ix->version == IndexVersion1){
		seterr(EAdmin, "index bucket %ud on %s is full\n", buck, is->part->name);
		ok = -1;
		goto out;
	}

	/* in V2, split the block and try again; splitiblock puts b */
	if(splitiblock(ix, b, is, buck, &ib) < 0)
		return -1;
	goto top;

out:
	putdblock(b);
	return ok;
}

static int
writebucket(ISect *is, u32int buck, IBucket *ib, DBlock *b)
{
	assert(b->dirty == DirtyIndex || b->dirty == DirtyIndexSplit);

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

static int
okibucket(IBucket *ib, ISect *is)
{
	if(ib->n <= is->buckmax)
		return 0;

	seterr(EICorrupt, "corrupted disk index bucket: n=%ud max=%ud, depth=%lud range=[%lud,%lud)",
		ib->n, is->buckmax, ib->depth, is->start, is->stop);
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

/*
 * find the number of the index section holding bucket #buck
 */
static int
indexsect0(Index *ix, u32int buck)
{
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
	return l - 1;
}

/*
 * load the index block at bucket #buck
 */
static DBlock*
loadibucket0(Index *ix, u32int buck, ISect **pis, u32int *pbuck, IBucket *ib, int read)
{
	ISect *is;
	DBlock *b;

	is = ix->sects[indexsect0(ix, buck)];
	if(buck < is->start || is->stop <= buck){
		seterr(EAdmin, "index lookup out of range: %ud not found in index\n", buck);
		return nil;
	}

	buck -= is->start;
	if((b = getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), read)) == nil)
		return nil;

	if(pis)
		*pis = is;
	if(pbuck)
		*pbuck = buck;
	if(ib)
		unpackibucket(ib, b->data);
	return b;
}

/*
 * find the number of the index section holding score
 */
static int
indexsect1(Index *ix, u8int *score)
{
	return indexsect0(ix, hashbits(score, 32) / ix->div);
}

/*
 * load the index block responsible for score.
 */
static DBlock*
loadibucket1(Index *ix, u8int *score, ISect **pis, u32int *pbuck, IBucket *ib)
{
	return loadibucket0(ix, hashbits(score, 32)/ix->div, pis, pbuck, ib, 1);
}

static u32int
keytobuck(Index *ix, u32int key, int d)
{
	/* clear all but top d bits; can't depend on boundary case shifts */
	if(d == 0)
		key = 0;
	else if(d != 32)
		key &= ~((1<<(32-d))-1);

	/* truncate to maxdepth bits */
	if(ix->maxdepth != 32)
		key >>= 32 - ix->maxdepth;

	return ix->bitblocks + key;
}

/*
 * to check whether .xxx has split, check whether .xxx1 is in use.
 * it might be worth caching the block for future lookups, but for now
 * let's assume the dcache is good enough.
 */
static int
bitmapop(Index *ix, u32int key, int d, int set)
{
	DBlock *b;
	int inuse;
	u32int key1, buck1;

	if(d >= ix->maxdepth)
		return 0;


	/* construct .xxx1 in bucket number format */
	key1 = key | (1<<(32-d-1));
	buck1 = keytobuck(ix, key1, d+1);

if(0) fprint(2, "key %d/%0*ub key1 %d/%0*ub buck1 %08ux\n",
	d, d, KEY(key, d), d+1, d+1, KEY(key1, d+1), buck1);

	/* check whether buck1 is in use */

	if((b = loadibucket0(ix, buck1 >> ix->bitkeylog, nil, nil, nil, 1)) == nil){
		seterr(ECorrupt, "cannot load in-use bitmap block");
fprint(2, "loadibucket: %r\n");
		return -1;
	}
if(0) fprint(2, "buck1 %08ux bitkeymask %08ux bitkeylog %d\n", buck1, ix->bitkeymask, ix->bitkeylog);
	buck1 &= ix->bitkeymask;
	inuse = ((u32int*)b->data)[buck1>>5] & (1<<(buck1&31));
	if(set && !inuse){
		dirtydblock(b, DirtyIndexBitmap);
		((u32int*)b->data)[buck1>>5] |= (1<<(buck1&31));
	}
	putdblock(b);
	return inuse;
}

static int
issplit(Index *ix, u32int key, int d)
{
	return bitmapop(ix, key, d, 0);
}

static int
marksplit(Index *ix, u32int key, int d)
{
	return bitmapop(ix, key, d, 1);
}	

/* 
 * find the number of the index section holding score.
 * it's not terrible to be wrong once in a while, so we just
 * do what the bitmap tells us and don't worry about the 
 * bitmap being out of date.
 */
static int
indexsect2(Index *ix, u8int *score)
{
	u32int key;
	int d, is;

	key = hashbits(score, 32);
	for(d=0; d<=ix->maxdepth; d++){
		is = issplit(ix, key, d);
		if(is == -1)
			return 0;	/* must return something valid! */
		if(!is)
			break;
	}

	if(d > ix->maxdepth){
		seterr(EBug, "index bitmap inconsistent with maxdepth");
		return 0;	/* must return something valid! */
	}

	return indexsect0(ix, keytobuck(ix, key, d));
}

/*
 * load the index block responsible for score. 
 * (unlike indexsect2, must be correct always.)
 */
static DBlock*
loadibucket2(Index *ix, u8int *score, ISect **pis, u32int *pbuck, IBucket *ib)
{
	u32int key;
	int d, try, is;
	DBlock *b;

	for(try=0; try<32; try++){
		key = hashbits(score, 32);
		for(d=0; d<=ix->maxdepth; d++){
			is = issplit(ix, key, d);
			if(is == -1)
				return nil;
			if(!is)
				break;
		}
		if(d > ix->maxdepth){
			seterr(EBug, "index bitmap inconsistent with maxdepth");
			return nil;
		}

		if((b = loadibucket0(ix, keytobuck(ix, key, d), pis, pbuck, ib, 1)) == nil)
			return nil;

		if(ib->depth == d)
			return b;

		if(ib->depth < d){
			fprint(2, "loaded block %ud for %d/%0*ub got %d/%0*ub\n",
				*pbuck, d, d, KEY(key,d), ib->depth, ib->depth, KEY(key, ib->depth));
			seterr(EBug, "index block has smaller depth than expected -- cannot happen");
			putdblock(b);
			return nil;
		}

		/*
		 * ib->depth > d, meaning the bitmap was out of date.
		 * fix the bitmap and try again.  if we actually updated
		 * the bitmap in splitiblock, this would only happen if
		 * venti crashed at an inopportune moment.  but this way
		 * the code gets tested more.
		 */
if(0) fprint(2, "update bitmap: %d/%0*ub is split\n", d, d, KEY(key,d));
		putdblock(b);
		if(marksplit(ix, key, d) < 0)
			return nil;
	}
	seterr(EBug, "loadibucket2 failed to sync bitmap with disk!");
	return nil;
}

static int
splitiblock(Index *ix, DBlock *b, ISect *is, u32int buck, IBucket *ib)
{
	int i, d;
	u8int *score;
	u32int buck0, buck1, key0, key1, key, dmask;
	DBlock *b0, *b1;
	IBucket ib0, ib1;

	if(ib->depth == ix->maxdepth){
if(0) fprint(2, "depth=%d == maxdepth\n", ib->depth);
		seterr(EAdmin, "index bucket %ud on %s is full\n", buck, is->part->name);
		putdblock(b);
		return -1;
	}

	buck = is->start+buck - ix->bitblocks;
	d = ib->depth+1;
	buck0 = buck;
	buck1 = buck0 | (1<<(ix->maxdepth-d));
	if(ix->maxdepth == 32){
		key0 = buck0;
		key1 = buck1;
	}else{
		key0 = buck0 << (32-ix->maxdepth);
		key1 = buck1 << (32-ix->maxdepth);
	}
	buck0 += ix->bitblocks;
	buck1 += ix->bitblocks;
	USED(buck0);
	USED(key1);

	if(d == 32)
		dmask = TWID32;
	else
		dmask = TWID32 ^ ((1<<(32-d))-1);

	/*
	 * Since we hold the lock for b, the bitmap
	 * thinks buck1 doesn't exist, and the bit
	 * for buck1 can't be updated without first
	 * locking and splitting b, it's safe to try to
	 * acquire the lock on buck1 without dropping b.
	 * No one else will go after it too.
	 *
	 * Also, none of the rest of the code ever locks
	 * more than one block at a time, so it's okay if
	 * we do.
	 */
	if((b1 = loadibucket0(ix, buck1, nil, nil, &ib1, 0)) == nil){
		putdblock(b);
		return -1;
	}
	b0 = b;
	ib0 = *ib;

	/*
	 * Funny locking going on here -- see dirtydblock.
	 * We must putdblock(b1) before putdblock(b0).
	 */
	dirtydblock(b0, DirtyIndex);
	dirtydblock(b1, DirtyIndexSplit);

	/*
	 * Split the block contents.
	 * The block is already sorted, so it's pretty easy:
	 * the first part stays, the second part goes to b1.
	 */
	ib0.n = 0;
	ib0.depth = d;
	ib1.n = 0;
	ib1.depth = d;
	for(i=0; i<ib->n; i++){
		score = ib->data+i*IEntrySize;
		key = hashbits(score, 32);
		if((key&dmask) != key0)
			break;
	}
	ib0.n = i;
	ib1.n = ib->n - ib0.n;
	memmove(ib1.data, ib0.data+ib0.n*IEntrySize, ib1.n*IEntrySize);
if(0) fprint(2, "splitiblock %d in %d/%0*ub => %d in %d/%0*ub + %d in %d/%0*ub\n",
	ib->n, d-1, d-1, key0>>(32-d+1), 
	ib0.n, d, d, key0>>(32-d), 
	ib1.n, d, d, key1>>(32-d));

	packibucket(&ib0, b0->data);
	packibucket(&ib1, b1->data);

	/* order matters!  see comment above. */
	putdblock(b1);
	putdblock(b0);

	/*
	 * let the recovery code take care of updating the bitmap.
	 */

	qlock(&stats.lock);
	stats.indexsplits++;
	qunlock(&stats.lock);

	return 0;
}

int
indexsect(Index *ix, u8int *score)
{
	if(ix->version == IndexVersion1)
		return indexsect1(ix, score);
	return indexsect2(ix, score);
}

DBlock*
loadibucket(Index *ix, u8int *score, ISect **pis, u32int *pbuck, IBucket *ib)
{
	if(ix->version == IndexVersion1)
		return loadibucket1(ix, score, pis, pbuck, ib);
	return loadibucket2(ix, score, pis, pbuck, ib);
}


