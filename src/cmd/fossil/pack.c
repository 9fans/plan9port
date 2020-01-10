#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

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
#define	U32PUT(p,v)	(p)[0]=((v)>>24)&0xFF;(p)[1]=((v)>>16)&0xFF;(p)[2]=((v)>>8)&0xFF;(p)[3]=(v)&0xFF
#define	U48PUT(p,v,t32)	t32=(v)>>32;U16PUT(p,t32);t32=(v);U32PUT((p)+2,t32)
#define	U64PUT(p,v,t32)	t32=(v)>>32;U32PUT(p,t32);t32=(v);U32PUT((p)+4,t32)

void
headerPack(Header *h, uchar *p)
{
	memset(p, 0, HeaderSize);
	U32PUT(p, HeaderMagic);
	U16PUT(p+4, HeaderVersion);
	U16PUT(p+6, h->blockSize);
	U32PUT(p+8, h->super);
	U32PUT(p+12, h->label);
	U32PUT(p+16, h->data);
	U32PUT(p+20, h->end);
}

int
headerUnpack(Header *h, uchar *p)
{
	if(U32GET(p) != HeaderMagic){
		werrstr("vac header bad magic");
		return 0;
	}
	h->version = U16GET(p+4);
	if(h->version != HeaderVersion){
		werrstr("vac header bad version");
		return 0;
	}
	h->blockSize = U16GET(p+6);
	h->super = U32GET(p+8);
	h->label = U32GET(p+12);
	h->data = U32GET(p+16);
	h->end = U32GET(p+20);
	return 1;
}

void
labelPack(Label *l, uchar *p, int i)
{
	p += i*LabelSize;
	U8PUT(p, l->state);
	U8PUT(p+1, l->type);
	U32PUT(p+2, l->epoch);
	U32PUT(p+6, l->epochClose);
	U32PUT(p+10, l->tag);
}

int
labelUnpack(Label *l, uchar *p, int i)
{
	p += i*LabelSize;
	l->state = p[0];
	l->type = p[1];
	l->epoch = U32GET(p+2);
	l->epochClose = U32GET(p+6);
	l->tag = U32GET(p+10);

	if(l->type > BtMax){
Bad:
		werrstr(EBadLabel);
		fprint(2, "%s: labelUnpack: bad label: 0x%.2ux 0x%.2ux 0x%.8ux "
			"0x%.8ux 0x%.8ux\n", argv0, l->state, l->type, l->epoch,
			l->epochClose, l->tag);
		return 0;
	}
	if(l->state != BsBad && l->state != BsFree){
		if(!(l->state&BsAlloc) || l->state & ~BsMask)
			goto Bad;
		if(l->state&BsClosed){
			if(l->epochClose == ~(u32int)0)
				goto Bad;
		}else{
			if(l->epochClose != ~(u32int)0)
				goto Bad;
		}
	}
	return 1;
}

u32int
globalToLocal(uchar score[VtScoreSize])
{
	int i;

	for(i=0; i<VtScoreSize-4; i++)
		if(score[i] != 0)
			return NilBlock;

	return U32GET(score+VtScoreSize-4);
}

void
localToGlobal(u32int addr, uchar score[VtScoreSize])
{
	memset(score, 0, VtScoreSize-4);
	U32PUT(score+VtScoreSize-4, addr);
}

void
entryPack(Entry *e, uchar *p, int index)
{
	ulong t32;
	int flags;

	p += index * VtEntrySize;

	U32PUT(p, e->gen);
	U16PUT(p+4, e->psize);
	U16PUT(p+6, e->dsize);
	flags = e->flags | ((e->depth << _VtEntryDepthShift) & _VtEntryDepthMask);
	U8PUT(p+8, flags);
	memset(p+9, 0, 5);
	U48PUT(p+14, e->size, t32);

	if(flags & VtEntryLocal){
		if(globalToLocal(e->score) == NilBlock)
			abort();
		memset(p+20, 0, 7);
		U8PUT(p+27, e->archive);
		U32PUT(p+28, e->snap);
		U32PUT(p+32, e->tag);
		memmove(p+36, e->score+16, 4);
	}else
		memmove(p+20, e->score, VtScoreSize);
}

int
entryUnpack(Entry *e, uchar *p, int index)
{
	p += index * VtEntrySize;

	e->gen = U32GET(p);
	e->psize = U16GET(p+4);
	e->dsize = U16GET(p+6);
	e->flags = U8GET(p+8);
	e->depth = (e->flags & _VtEntryDepthMask) >> _VtEntryDepthShift;
	e->flags &= ~_VtEntryDepthMask;
	e->size = U48GET(p+14);

	if(e->flags & VtEntryLocal){
		e->archive = p[27];
		e->snap = U32GET(p+28);
		e->tag = U32GET(p+32);
		memset(e->score, 0, 16);
		memmove(e->score+16, p+36, 4);
	}else{
		e->archive = 0;
		e->snap = 0;
		e->tag = 0;
		memmove(e->score, p+20, VtScoreSize);
	}

	return 1;
}

int
entryType(Entry *e)
{
	return (((e->flags & _VtEntryDir) != 0) << 3) | e->depth;
}


void
superPack(Super *s, uchar *p)
{
	u32int t32;

	memset(p, 0, SuperSize);
	U32PUT(p, SuperMagic);
	assert(s->version == SuperVersion);
	U16PUT(p+4, s->version);
	U32PUT(p+6, s->epochLow);
	U32PUT(p+10, s->epochHigh);
	U64PUT(p+14, s->qid, t32);
	U32PUT(p+22, s->active);
	U32PUT(p+26, s->next);
	U32PUT(p+30, s->current);
	memmove(p+34, s->last, VtScoreSize);
	memmove(p+54, s->name, sizeof(s->name));
}

int
superUnpack(Super *s, uchar *p)
{
	memset(s, 0, sizeof(*s));
	if(U32GET(p) != SuperMagic)
		goto Err;
	s->version = U16GET(p+4);
	if(s->version != SuperVersion)
		goto Err;
	s->epochLow = U32GET(p+6);
	s->epochHigh = U32GET(p+10);
	s->qid = U64GET(p+14);
	if(s->epochLow == 0 || s->epochLow > s->epochHigh || s->qid == 0)
		goto Err;
	s->active = U32GET(p+22);
	s->next = U32GET(p+26);
	s->current = U32GET(p+30);
	memmove(s->last, p+34, VtScoreSize);
	memmove(s->name, p+54, sizeof(s->name));
	s->name[sizeof(s->name)-1] = 0;
	return 1;
Err:
	memset(s, 0, sizeof(*s));
	werrstr(EBadSuper);
	return 0;
}
