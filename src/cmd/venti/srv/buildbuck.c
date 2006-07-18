#include "stdinc.h"
#include "dat.h"
#include "fns.h"

/*
 * An IEStream is a sorted list of index entries.
 */
struct IEStream
{
	Part	*part;
	u64int	off;		/* read position within part */
	u64int	n;		/* number of valid ientries left to read */
	u32int	size;		/* allocated space in buffer */
	u8int	*buf;
	u8int	*pos;		/* current place in buffer */
	u8int	*epos;		/* end of valid buffer contents */
};

IEStream*
initiestream(Part *part, u64int off, u64int clumps, u32int size)
{
	IEStream *ies;

/* out of memory? */
	ies = MKZ(IEStream);
	ies->buf = MKN(u8int, size);
	ies->epos = ies->buf;
	ies->pos = ies->epos;
	ies->off = off;
	ies->n = clumps;
	ies->size = size;
	ies->part = part;
	return ies;
}

void
freeiestream(IEStream *ies)
{
	if(ies == nil)
		return;
	free(ies->buf);
	free(ies);
}

/*
 * Return the next IEntry (still packed) in the stream.
 */
static u8int*
peekientry(IEStream *ies)
{
	u32int n, nn;

	n = ies->epos - ies->pos;
	if(n < IEntrySize){
		memmove(ies->buf, ies->pos, n);
		ies->epos = &ies->buf[n];
		ies->pos = ies->buf;
		nn = ies->size;
		if(nn > ies->n * IEntrySize)
			nn = ies->n * IEntrySize;
		nn -= n;
		if(nn == 0)
			return nil;
//fprint(2, "peek %d from %llud into %p\n", nn, ies->off, ies->epos);
		if(readpart(ies->part, ies->off, ies->epos, nn) < 0){
			seterr(EOk, "can't read sorted index entries: %r");
			return nil;
		}
		ies->epos += nn;
		ies->off += nn;
	}
	return ies->pos;
}

/*
 * Compute the bucket number for the given IEntry.
 * Knows that the score is the first thing in the packed
 * representation.
 */
static u32int
iebuck(Index *ix, u8int *b, IBucket *ib, IEStream *ies)
{
	USED(ies);
	USED(ib);
	return hashbits(b, 32) / ix->div;
}

/*
 * Fill ib with the next bucket in the stream.
 */
u32int
buildbucket(Index *ix, IEStream *ies, IBucket *ib, uint maxdata)
{
	IEntry ie1, ie2;
	u8int *b;
	u32int buck;

	buck = TWID32;
	ib->n = 0;
	while(ies->n){
		b = peekientry(ies);
		if(b == nil)
			return TWID32;
/* fprint(2, "b=%p ies->n=%lld ib.n=%d buck=%d score=%V\n", b, ies->n, ib->n, iebuck(ix, b, ib, ies), b); */
		if(ib->n == 0)
			buck = iebuck(ix, b, ib, ies);
		else{
			if(buck != iebuck(ix, b, ib, ies))
				break;
			if(ientrycmp(&ib->data[(ib->n - 1)* IEntrySize], b) == 0){
				/*
				 * guess that the larger address is the correct one to use
				 */
				unpackientry(&ie1, &ib->data[(ib->n - 1)* IEntrySize]);
				unpackientry(&ie2, b);
				seterr(EOk, "duplicate index entry for score=%V type=%d", ie1.score, ie1.ia.type);
				ib->n--;
				if(ie1.ia.addr > ie2.ia.addr)
					memmove(b, &ib->data[ib->n * IEntrySize], IEntrySize);
			}
		}
		if((ib->n+1)*IEntrySize > maxdata){
			seterr(EOk, "bucket overflow");
			return TWID32;
		}
		memmove(&ib->data[ib->n * IEntrySize], b, IEntrySize);
		ib->n++;
		ies->n--;
		ies->pos += IEntrySize;
	}
	return buck;
}
