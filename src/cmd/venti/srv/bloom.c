/*
 * Bloom filter tracking which scores are present in our arenas
 * and (more importantly) which are not.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int ignorebloom;

int
bloominit(Bloom *b, vlong vsize, u8int *data)
{
	ulong size;

	size = vsize;
	if(size != vsize){	/* truncation */
		werrstr("bloom data too big");
		return -1;
	}

	b->size = size;
	b->nhash = 32;	/* will be fixed by caller on initialization */
	if(data != nil)
		if(unpackbloomhead(b, data) < 0)
			return -1;

	b->bitmask = (b->size<<3) - 1;
	b->data = data;
	return 0;
}

void
wbbloomhead(Bloom *b)
{
	packbloomhead(b, b->data);
}

Bloom*
readbloom(Part *p)
{
	uchar buf[512];
	Bloom *b;

	b = vtmallocz(sizeof *b);
	if(readpart(p, 0, buf, sizeof buf) < 0)
		return nil;
	/*
	 * pass buf as b->data so that bloominit
	 * can parse header.  won't be used for
	 * accessing bits (cleared below).
	 */
	if(bloominit(b, 0, buf) < 0){
		vtfree(b);
		return nil;
	}else{
		/*
		 * default block size is system page size.
		 * the bloom filter is usually very big.
		 * bump the block size up to speed i/o.
		 */
		if(p->blocksize < (1<<20)){
			p->blocksize = 1<<20;
			if(p->blocksize > p->size)
				p->blocksize = p->size;
		}
	}
	b->part = p;
	b->data = nil;
	return b;
}

int
resetbloom(Bloom *b)
{
	uchar *data;

	data = vtmallocz(b->size);
	b->data = data;
	if(b->size == MaxBloomSize)	/* 2^32 overflows ulong */
		addstat(StatBloomBits, b->size*8-1);
	else
		addstat(StatBloomBits, b->size*8);
	return 0;
}

int
loadbloom(Bloom *b)
{
	int i, n;
	uint ones;
	uchar *data;
	u32int *a;

	data = vtmallocz(b->size);
	if(readpart(b->part, 0, data, b->size) < 0){
		vtfree(b);
		vtfree(data);
		return -1;
	}
	b->data = data;

	a = (u32int*)b->data;
	n = b->size/4;
	ones = 0;
	for(i=0; i<n; i++)
		ones += countbits(a[i]);
	addstat(StatBloomOnes, ones);

	if(b->size == MaxBloomSize)	/* 2^32 overflows ulong */
		addstat(StatBloomBits, b->size*8-1);
	else
		addstat(StatBloomBits, b->size*8);

	return 0;
}

int
writebloom(Bloom *b)
{
	wbbloomhead(b);
	if(writepart(b->part, 0, b->data, b->size) < 0)
		return -1;
	if(flushpart(b->part) < 0)
		return -1;
	return 0;
}

/*
 * Derive two random 32-bit quantities a, b from the score
 * and then use a+b*i as a sequence of bloom filter indices.
 * Michael Mitzenmacher has a recent (2005) paper saying this is okay.
 * We reserve the bottom bytes (BloomHeadSize*8 bits) for the header.
 */
static void
gethashes(u8int *score, ulong *h)
{
	int i;
	u32int a, b;

	a = 0;
	b = 0;
	for(i=4; i+8<=VtScoreSize; i+=8){
		a ^= *(u32int*)(score+i);
		b ^= *(u32int*)(score+i+4);
	}
	if(i+4 <= VtScoreSize)	/* 20 is not 4-aligned */
		a ^= *(u32int*)(score+i);
	for(i=0; i<BloomMaxHash; i++, a+=b)
		h[i] = a < BloomHeadSize*8 ? BloomHeadSize*8 : a;
}

static void
_markbloomfilter(Bloom *b, u8int *score)
{
	int i, nnew;
	ulong h[BloomMaxHash];
	u32int x, *y, z, *tab;

	trace("markbloomfilter", "markbloomfilter %V", score);
	gethashes(score, h);
	nnew = 0;
	tab = (u32int*)b->data;
	for(i=0; i<b->nhash; i++){
		x = h[i];
		y = &tab[(x&b->bitmask)>>5];
		z = 1<<(x&31);
		if(!(*y&z)){
			nnew++;
			*y |= z;
		}
	}
	if(nnew)
		addstat(StatBloomOnes, nnew);

	trace("markbloomfilter", "markbloomfilter exit");
}

static int
_inbloomfilter(Bloom *b, u8int *score)
{
	int i;
	ulong h[BloomMaxHash], x;
	u32int *tab;

	gethashes(score, h);
	tab = (u32int*)b->data;
	for(i=0; i<b->nhash; i++){
		x = h[i];
		if(!(tab[(x&b->bitmask)>>5] & (1<<(x&31))))
			return 0;
	}
	return 1;
}

int
inbloomfilter(Bloom *b, u8int *score)
{
	int r;

	if(b == nil || b->data == nil)
		return 1;

	if(ignorebloom)
		return 1;

	rlock(&b->lk);
	r = _inbloomfilter(b, score);
	runlock(&b->lk);
	addstat(StatBloomLookup, 1);
	if(r)
		addstat(StatBloomMiss, 1);
	else
		addstat(StatBloomHit, 1);
	return r;
}

void
markbloomfilter(Bloom *b, u8int *score)
{
	if(b == nil || b->data == nil)
		return;

	rlock(&b->lk);
	qlock(&b->mod);
	_markbloomfilter(b, score);
	qunlock(&b->mod);
	runlock(&b->lk);
}

void
markbloomfiltern(Bloom *b, u8int score[][20], int n)
{
	int i;

	if(b == nil || b->data == nil)
		return;

	rlock(&b->lk);
	qlock(&b->mod);
	for(i=0; i<n; i++)
		_markbloomfilter(b, score[i]);
	qunlock(&b->mod);
	runlock(&b->lk);
}

static void
bloomwriteproc(void *v)
{
	int ret;
	Bloom *b;

	threadsetname("bloomwriteproc");
	b = v;
	for(;;){
		recv(b->writechan, 0);
		if((ret=writebloom(b)) < 0)
			fprint(2, "oops! writing bloom: %r\n");
		else
			ret = 0;
		sendul(b->writedonechan, ret);
	}
}

void
startbloomproc(Bloom *b)
{
	b->writechan = chancreate(sizeof(void*), 0);
	b->writedonechan = chancreate(sizeof(ulong), 0);
	vtproc(bloomwriteproc, b);
}
