#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

typedef struct MetaChunk MetaChunk;

struct MetaChunk {
	ushort offset;
	ushort size;
	ushort index;
};

static int	stringUnpack(char **s, uchar **p, int *n);

/*
 * integer conversion routines
 */
#define	U8GET(p)	((p)[0])
#define	U16GET(p)	(((p)[0]<<8)|(p)[1])
#define	U32GET(p)	(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3])
#define	U48GET(p)	(((uvlong)U16GET(p)<<32)|(uvlong)U32GET((p)+2))
#define	U64GET(p)	(((uvlong)U32GET(p)<<32)|(uvlong)U32GET((p)+4))

#define	U8PUT(p,v)	(p)[0]=(v)
#define	U16PUT(p,v)	(p)[0]=(v)>>8;(p)[1]=(v)
#define	U32PUT(p,v)	(p)[0]=(v)>>24;(p)[1]=(v)>>16;(p)[2]=(v)>>8;(p)[3]=(v)
#define	U48PUT(p,v,t32)	t32=(v)>>32;U16PUT(p,t32);t32=(v);U32PUT((p)+2,t32)
#define	U64PUT(p,v,t32)	t32=(v)>>32;U32PUT(p,t32);t32=(v);U32PUT((p)+4,t32)

static int
stringUnpack(char **s, uchar **p, int *n)
{
	int nn;

	if(*n < 2)
		return 0;
	
	nn = U16GET(*p);
	*p += 2;
	*n -= 2;
	if(nn > *n)
		return 0;
	*s = vtMemAlloc(nn+1);
	memmove(*s, *p, nn);
	(*s)[nn] = 0;
	*p += nn;
	*n -= nn;
	return 1;
}

static int
stringPack(char *s, uchar *p)
{
	int n;

	n = strlen(s);
	U16PUT(p, n);
	memmove(p+2, s, n);
	return n+2;
}


int
mbUnpack(MetaBlock *mb, uchar *p, int n)
{
	u32int magic;

	mb->maxsize = n;
	mb->buf = p;

	if(n == 0) {
		memset(mb, 0, sizeof(MetaBlock));
		return 1;
	}

	magic = U32GET(p);
	if(magic != MetaMagic && magic != MetaMagic+1) {
		vtSetError("bad meta block magic");
		return 0;
	}
	mb->size = U16GET(p+4);
	mb->free = U16GET(p+6);
	mb->maxindex = U16GET(p+8);
	mb->nindex = U16GET(p+10);
	mb->unbotch = (magic == MetaMagic+1);

	if(mb->size > n) {
		vtSetError("bad meta block size");
		return 0;
	}
	p += MetaHeaderSize;
	n -= MetaHeaderSize;

	USED(p);
	if(n < mb->maxindex*MetaIndexSize) {
 		vtSetError("truncated meta block 2");
		return 0;
	}
	return 1;
}

void
mbPack(MetaBlock *mb)
{
	uchar *p;

	p = mb->buf;

	U32PUT(p, MetaMagic);
	U16PUT(p+4, mb->size);
	U16PUT(p+6, mb->free);
	U16PUT(p+8, mb->maxindex);
	U16PUT(p+10, mb->nindex);
}


void
mbDelete(MetaBlock *mb, int i, MetaEntry *me)
{
	uchar *p;
	int n;

	assert(i < mb->nindex);

	if(me->p - mb->buf + me->size == mb->size)
		mb->size -= me->size;
	else
		mb->free += me->size;

	p = mb->buf + MetaHeaderSize + i*MetaIndexSize;
	n = (mb->nindex-i-1)*MetaIndexSize;
	memmove(p, p+MetaIndexSize, n);
	memset(p+n, 0, MetaIndexSize);
	mb->nindex--;
}

void
mbInsert(MetaBlock *mb, int i, MetaEntry *me)
{
	uchar *p;
	int o, n;

	assert(mb->nindex < mb->maxindex);

	o = me->p - mb->buf;
	n = me->size;
	if(o+n > mb->size) {
		mb->free -= mb->size - o;
		mb->size = o + n;
	} else
		mb->free -= n;

	p = mb->buf + MetaHeaderSize + i*MetaIndexSize;
	n = (mb->nindex-i)*MetaIndexSize;
	memmove(p+MetaIndexSize, p, n);
	U16PUT(p, me->p - mb->buf);
	U16PUT(p+2, me->size);
	mb->nindex++;
}

int
meUnpack(MetaEntry *me, MetaBlock *mb, int i)
{
	uchar *p;
	int eo, en;

	if(i < 0 || i >= mb->nindex) {
		vtSetError("bad meta entry index");
		return 0;
	}

	p = mb->buf + MetaHeaderSize + i*MetaIndexSize;
	eo = U16GET(p);
	en = U16GET(p+2);

if(0)print("eo = %d en = %d\n", eo, en);
	if(eo < MetaHeaderSize + mb->maxindex*MetaIndexSize) {
		vtSetError("corrupted entry in meta block");
		return 0;
	}

	if(eo+en > mb->size) {
 		vtSetError("truncated meta block");
		return 0;
	}

	p = mb->buf + eo;
	
	/* make sure entry looks ok and includes an elem name */
	if(en < 8 || U32GET(p) != DirMagic || en < 8 + U16GET(p+6)) {
		vtSetError("corrupted meta block entry");
		return 0;
	}

	me->p = p;
	me->size = en;

	return 1;
}

/* assumes a small amount of checking has been done in mbEntry */
int
meCmp(MetaEntry *me, char *s)
{
	int n;
	uchar *p;

	p = me->p;

	p += 6;
	n = U16GET(p);
	p += 2;

	assert(n + 8 < me->size);

	while(n > 0) {
		if(*s == 0)
			return -1;
		if(*p < (uchar)*s)
			return -1;
		if(*p > (uchar)*s)
			return 1;
		p++;
		s++;
		n--;
	}
	return *s != 0;
}

int
meCmpNew(MetaEntry *me, char *s)
{
	int n;
	uchar *p;

	p = me->p;

	p += 6;
	n = U16GET(p);
	p += 2;

	assert(n + 8 < me->size);

	while(n > 0) {
		if(*s == 0)
			return 1;
		if(*p < (uchar)*s)
			return -1;
		if(*p > (uchar)*s)
			return 1;
		p++;
		s++;
		n--;
	}
	return -(*s != 0);
}

static int
offsetCmp(void *s0, void *s1)
{
	MetaChunk *mc0, *mc1;

	mc0 = s0;
	mc1 = s1;
	if(mc0->offset < mc1->offset)
		return -1;
	if(mc0->offset > mc1->offset)
		return 1;
	return 0;
}

static MetaChunk *
metaChunks(MetaBlock *mb)
{
	MetaChunk *mc;
	int oo, o, n, i;
	uchar *p;

	mc = vtMemAlloc(mb->nindex*sizeof(MetaChunk));
	p = mb->buf + MetaHeaderSize;
	for(i = 0; i<mb->nindex; i++) {
		mc[i].offset = U16GET(p);
		mc[i].size = U16GET(p+2);
		mc[i].index = i;
		p += MetaIndexSize;
	}

	qsort(mc, mb->nindex, sizeof(MetaChunk), offsetCmp);

	/* check block looks ok */
	oo = MetaHeaderSize + mb->maxindex*MetaIndexSize;
	o = oo;
	n = 0;
	for(i=0; i<mb->nindex; i++) {
		o = mc[i].offset;
		n = mc[i].size;
		if(o < oo)
			goto Err;
		oo += n;
	}
	if(o+n <= mb->size)
		goto Err;
	if(mb->size - oo != mb->free)
		goto Err;

	return mc;
Err:
	vtMemFree(mc);
	return nil;
}

static void
mbCompact(MetaBlock *mb, MetaChunk *mc)
{
	int oo, o, n, i;

	oo = MetaHeaderSize + mb->maxindex*MetaIndexSize;
	
	for(i=0; i<mb->nindex; i++) {
		o = mc[i].offset;
		n = mc[i].size;
		if(o != oo) {
			memmove(mb->buf + oo, mb->buf + o, n);
			U16PUT(mb->buf + MetaHeaderSize + mc[i].index*MetaIndexSize, oo);
		}
		oo += n;
	}

	mb->size = oo;
	mb->free = 0;
}

uchar *
mbAlloc(MetaBlock *mb, int n)
{
	int i, o;
	MetaChunk *mc;

	/* off the end */
	if(mb->maxsize - mb->size >= n)
		return mb->buf + mb->size;

	/* check if possible */
	if(mb->maxsize - mb->size + mb->free < n)
		return nil;

	mc = metaChunks(mb);

	/* look for hole */
	o = MetaHeaderSize + mb->maxindex*MetaIndexSize;
	for(i=0; i<mb->nindex; i++) {
		if(mc[i].offset - o >= n) {
			vtMemFree(mc);
			return mb->buf + o;
		}
		o = mc[i].offset + mc[i].size;
	}

	if(mb->maxsize - o >= n) {
		vtMemFree(mc);
		return mb->buf + o;
	}

	/* compact and return off the end */
	mbCompact(mb, mc);
	vtMemFree(mc);

	assert(mb->maxsize - mb->size >= n);
	return mb->buf + mb->size;
}

int
vdSize(VacDir *dir)
{
	int n;
	
	/* constant part */

	n = 	4 +	/* magic */
		2 + 	/* version */
		4 +	/* entry */
		4 + 	/* guid */
		4 + 	/* mentry */
		4 + 	/* mgen */
		8 +	/* qid */
		4 + 	/* mtime */
		4 + 	/* mcount */
		4 + 	/* ctime */
		4 + 	/* atime */
		4 +	/* mode */
		0;

	/* strings */
	n += 2 + strlen(dir->elem);
	n += 2 + strlen(dir->uid);
	n += 2 + strlen(dir->gid);
	n += 2 + strlen(dir->mid);

	/* optional sections */
	if(dir->qidSpace) {
		n += 	3 + 	/* option header */
			8 + 	/* qidOffset */
			8;	/* qid Max */
	}

	return n;
}

void
vdPack(VacDir *dir, MetaEntry *me)
{
	uchar *p;
	ulong t32;

	p = me->p;
	
	U32PUT(p, DirMagic);
	U16PUT(p+4, 9);		/* version */
	p += 6;

	p += stringPack(dir->elem, p);

	U32PUT(p, dir->entry);
	U32PUT(p+4, dir->gen);
	U32PUT(p+8, dir->mentry);
	U32PUT(p+12, dir->mgen);
	U64PUT(p+16, dir->qid, t32);
	p += 24;

	p += stringPack(dir->uid, p);
	p += stringPack(dir->gid, p);
	p += stringPack(dir->mid, p);
	
	U32PUT(p, dir->mtime);
	U32PUT(p+4, dir->mcount);
	U32PUT(p+8, dir->ctime);
	U32PUT(p+12, dir->atime);
	U32PUT(p+16, dir->mode);
	p += 5*4;

	if(dir->qidSpace) {
		U8PUT(p, DirQidSpaceEntry);
		U16PUT(p+1, 2*8);
		p += 3;
		U64PUT(p, dir->qidOffset, t32);
		U64PUT(p+8, dir->qidMax, t32);
	}

	assert(p == me->p + me->size);
}


int
vdUnpack(VacDir *dir, MetaEntry *me)
{
	int t, nn, n, version;
	uchar *p;
	
	p = me->p;
	n = me->size;

	memset(dir, 0, sizeof(VacDir));

if(0)print("vdUnpack\n");
	/* magic */
	if(n < 4 || U32GET(p) != DirMagic)
		goto Err;
	p += 4;
	n -= 4;

if(0)print("vdUnpack: got magic\n");
	/* version */
	if(n < 2)
		goto Err;
	version = U16GET(p);
	if(version < 7 || version > 9)
		goto Err;
	p += 2;
	n -= 2;	

if(0)print("vdUnpack: got version\n");

	/* elem */
	if(!stringUnpack(&dir->elem, &p, &n))
		goto Err;

if(0)print("vdUnpack: got elem\n");

	/* entry  */
	if(n < 4)
		goto Err;
	dir->entry = U32GET(p);
	p += 4;
	n -= 4;

if(0)print("vdUnpack: got entry\n");

	if(version < 9) {
		dir->gen = 0;
		dir->mentry = dir->entry+1;
		dir->mgen = 0;
	} else {
		if(n < 3*4)
			goto Err;
		dir->gen = U32GET(p);
		dir->mentry = U32GET(p+4);
		dir->mgen = U32GET(p+8);
		p += 3*4;
		n -= 3*4;
	}

if(0)print("vdUnpack: got gen etc\n");

	/* size is gotten from DirEntry */

	/* qid */
	if(n < 8)
		goto Err;
	dir->qid = U64GET(p);
	p += 8;
	n -= 8;

if(0)print("vdUnpack: got qid\n");
	/* skip replacement */
	if(version == 7) {
		if(n < VtScoreSize)
			goto Err;
		p += VtScoreSize;
		n -= VtScoreSize;
	}
	
	/* uid */
	if(!stringUnpack(&dir->uid, &p, &n))
		goto Err;

	/* gid */
	if(!stringUnpack(&dir->gid, &p, &n))
		goto Err;

	/* mid */
	if(!stringUnpack(&dir->mid, &p, &n))
		goto Err;

if(0)print("vdUnpack: got ids\n");
	if(n < 5*4)
		goto Err;
	dir->mtime = U32GET(p);
	dir->mcount = U32GET(p+4);
	dir->ctime = U32GET(p+8);
	dir->atime = U32GET(p+12);
	dir->mode = U32GET(p+16);
	p += 5*4;
	n -= 5*4;

if(0)print("vdUnpack: got times\n");
	/* optional meta data */
	while(n > 0) {
		if(n < 3)
			goto Err;
		t = p[0];
		nn = U16GET(p+1);
		p += 3;
		n -= 3;
		if(n < nn)
			goto Err;
		switch(t) {
		case DirPlan9Entry:
			/* not valid in version >= 9 */
			if(version >= 9)
				break;
			if(dir->plan9 || nn != 12)
				goto Err;
			dir->plan9 = 1;
			dir->p9path = U64GET(p);
			dir->p9version = U32GET(p+8);
			if(dir->mcount == 0)
				dir->mcount = dir->p9version;
			break;
		case DirGenEntry:
			/* not valid in version >= 9 */
			if(version >= 9)
				break;
			break;
		case DirQidSpaceEntry:
			if(dir->qidSpace || nn != 16)
				goto Err;
			dir->qidSpace = 1;
			dir->qidOffset = U64GET(p);
			dir->qidMax = U64GET(p+8);
			break;
		}
		p += nn;
		n -= nn;
	}
if(0)print("vdUnpack: got options\n");

	if(p != me->p + me->size)
		goto Err;

if(0)print("vdUnpack: correct size\n");
	return 1;
Err:
if(0)print("vdUnpack: XXXXXXXXXXXX EbadMeta\n");
	vtSetError(EBadMeta);
	vdCleanup(dir);
	return 0;
}
