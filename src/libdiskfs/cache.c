#include <u.h>
#include <libc.h>
#include <diskfs.h>

/*
 * Disk cache.  Caches by offset, so higher levels have
 * to deal with alignment issues (if we get asked for the
 * blocks at offsets 0 and 1, we'll do two reads).
 */

typedef struct DiskCache DiskCache;
typedef struct DiskCacheBlock DiskCacheBlock;

struct DiskCache
{
	Disk disk;
	Disk *subdisk;
	DiskCacheBlock **h;
	DiskCacheBlock *lruhead;
	DiskCacheBlock *lrutail;
	int nhash;
	int blocksize;
	Lock lk;
};

struct DiskCacheBlock
{
	Block block;
	Block *subblock;
	Lock lk;
	int ref;
	DiskCache *dc;
	DiskCacheBlock *next;
	DiskCacheBlock *lrunext;
	DiskCacheBlock *lruprev;
	u64int offset;
};

static void
addtohash(DiskCache *d, DiskCacheBlock *b, u64int offset)
{
	int h;

	if(b->offset != ~(u64int)0){
		fprint(2, "bad offset in addtohash\n");
		return;
	}
	b->offset = offset;
	h = offset % d->nhash;
	b->next = d->h[h];
	d->h[h] = b;
}

static void
delfromhash(DiskCache *d, DiskCacheBlock *b)
{
	int h;
	DiskCacheBlock **l;

	if(b->offset == ~(u64int)0)
		return;

	h = b->offset % d->nhash;
	for(l=&d->h[h]; *l; l=&(*l)->next)
		if(*l == b){
			*l = b->next;
			b->offset = ~(u64int)0;
			return;
		}
	fprint(2, "delfromhash: didn't find in hash table\n");
	return;
}

static void
putmru(DiskCache *d, DiskCacheBlock *b)
{
	b->lruprev = nil;
	b->lrunext = d->lruhead;
	d->lruhead = b;
	if(b->lrunext == nil)
		d->lrutail = b;
	else
		b->lrunext->lruprev = b;
}

static void
putlru(DiskCache *d, DiskCacheBlock *b)
{
	b->lruprev = d->lrutail;
	b->lrunext = nil;
	d->lrutail = b;
	if(b->lruprev == nil)
		d->lruhead = b;
	else
		b->lruprev->lrunext = b;
}

static void
delfromlru(DiskCache *d, DiskCacheBlock *b)
{
	if(b->lruprev)
		b->lruprev->lrunext = b->lrunext;
	else
		d->lruhead = b->lrunext;
	if(b->lrunext)
		b->lrunext->lruprev = b->lruprev;
	else
		d->lrutail = b->lruprev;
}

static DiskCacheBlock*
getlru(DiskCache *d)
{
	DiskCacheBlock *b;

	b = d->lrutail;
	if(b){
		delfromlru(d, b);
		delfromhash(d, b);
		blockput(b->subblock);
		b->subblock = nil;
	}
	return b;
}

static DiskCacheBlock*
findblock(DiskCache *d, u64int offset)
{
	int h;
	DiskCacheBlock *b;

	h = offset % d->nhash;
	for(b=d->h[h]; b; b=b->next)
		if(b->offset == offset)
			return b;
	return nil;
}

static DiskCacheBlock*
diskcachereadbig(DiskCache *d, u64int offset)
{
	Block *b;
	DiskCacheBlock *dcb;

	lock(&d->lk);
	dcb = findblock(d, offset);
	if(dcb){
/*fprint(2, "found %llud in cache %p\n", (uvlong)offset, dcb);*/
		if(dcb->ref++ == 0)
			delfromlru(d, dcb);
		unlock(&d->lk);
		return dcb;
	}

	dcb = getlru(d);
	unlock(&d->lk);
	if(dcb == nil){
		fprint(2, "diskcacheread: all blocks in use\n");
		return nil;
	}

	b = diskread(d->subdisk, d->blocksize, offset);
	lock(&d->lk);
	if(b == nil){
		putlru(d, dcb);
		dcb = nil;
	}else{
/*fprint(2, "read %llud from disk %p\n", (uvlong)offset, dcb); */
		dcb->subblock = b;
		dcb->ref++;
		addtohash(d, dcb, offset);
	}
	unlock(&d->lk);
	return dcb;
}

static void
diskcacheblockclose(Block *bb)
{
	DiskCacheBlock *b = bb->priv;

	lock(&b->dc->lk);
	if(--b->ref == 0)
		putmru(b->dc, b);
	unlock(&b->dc->lk);
	free(bb);
}

static Block*
diskcacheread(Disk *dd, u32int len, u64int offset)
{
	int frag, dlen;
	DiskCache *d = (DiskCache*)dd;
	DiskCacheBlock *dcb;
	Block *b;

	if(offset/d->blocksize != (offset+len-1)/d->blocksize){
		fprint(2, "diskBigRead: request for block crossing big block boundary\n");
		return nil;
	}

	b = mallocz(sizeof(Block), 1);
	if(b == nil)
		return nil;

	frag = offset%d->blocksize;

	dcb = diskcachereadbig(d, offset-frag);
	if(dcb == nil){
		free(b);
		return nil;
	}
	b->priv = dcb;
	b->_close = diskcacheblockclose;
	b->data = dcb->subblock->data+frag;

	dlen = dcb->subblock->len;
	if(frag+len >= dlen){
		if(frag >= dlen){
			blockput(b);
			return nil;
		}
		len = dlen-frag;
	}
	b->len = len;
/*fprint(2, "offset %llud at pointer %p %lux\n", (uvlong)offset, b->data, *(ulong*)(b->data+4)); */
	return b;
}

/*
 * It's okay to remove these from the hash table.
 * Either the block is in use by someone or it is on
 * the lru list.  If it's in use it will get put on the lru
 * list once the refs go away.
 */
static int
diskcachesync(Disk *dd)
{
	DiskCache *d = (DiskCache*)dd;
	DiskCacheBlock *b, *nextb;
	int i;

	lock(&d->lk);
	for(i=0; i<d->nhash; i++){
		for(b=d->h[i]; b; b=nextb){
			nextb = b->next;
			b->next = nil;
			b->offset = ~(u64int)0;
		}
		d->h[i] = nil;
	}
	unlock(&d->lk);
	return disksync(d->subdisk);
}

static void
diskcacheclose(Disk *dd)
{
	DiskCacheBlock *b;
	DiskCache *d = (DiskCache*)dd;

	diskclose(d->subdisk);
	for(b=d->lruhead; b; b=b->lrunext)
		blockput(b->subblock);
	free(d);
}

/* needn't be fast */
static int
isprime(int n)
{
	int i;

	for(i=2; i*i<=n; i++)
		if(n%i == 0)
			return 0;
	return 1;
}

Disk*
diskcache(Disk *subdisk, uint blocksize, uint ncache)
{
	int nhash, i;
	DiskCache *d;
	DiskCacheBlock *b;

	nhash = ncache;
	while(nhash > 1 && !isprime(nhash))
		nhash--;
	d = mallocz(sizeof(DiskCache)+ncache*sizeof(DiskCacheBlock)+nhash*sizeof(DiskCacheBlock*), 1);
	if(d == nil)
		return nil;

	b = (DiskCacheBlock*)&d[1];
	d->h = (DiskCacheBlock**)&b[ncache];
	d->nhash = nhash;
	d->blocksize = blocksize;
	d->subdisk = subdisk;
	d->disk._read = diskcacheread;
	d->disk._sync = diskcachesync;
	d->disk._close = diskcacheclose;

	for(i=0; i<ncache; i++){
		b[i].block._close = diskcacheblockclose;
		b[i].offset = ~(u64int)0;
		b[i].dc = d;
		putlru(d, &b[i]);
	}

	return &d->disk;
}
