#include <u.h>
#include <libc.h>
#include <bin.h>

enum
{
	StructAlign = sizeof(union {vlong vl; double d; ulong p; void *v;
				struct{vlong v;}vs; struct{double d;}ds; struct{ulong p;}ss; struct{void *v;}xs;})
};

enum
{
	BinSize	= 8*1024
};

struct Bin
{
	Bin	*next;
	ulong	total;			/* total bytes allocated in can->next */
	ulong	pos;
	ulong	end;
	ulong	v;			/* last value allocated */
	uchar	body[BinSize];
};

/*
 * allocator which allows an entire set to be freed at one time
 */
static Bin*
mkbin(Bin *bin, ulong size)
{
	Bin *b;

	size = ((size << 1) + (BinSize - 1)) & ~(BinSize - 1);
	b = malloc(sizeof(Bin) + size - BinSize);
	if(b == nil)
		return nil;
	b->next = bin;
	b->total = 0;
	if(bin != nil)
		b->total = bin->total + bin->pos - (ulong)bin->body;
	b->pos = (ulong)b->body;
	b->end = b->pos + size;
	return b;
}

void*
binalloc(Bin **bin, ulong size, int zero)
{
	Bin *b;
	ulong p;

	if(size == 0)
		size = 1;
	b = *bin;
	if(b == nil){
		b = mkbin(nil, size);
		if(b == nil)
			return nil;
		*bin = b;
	}
	p = b->pos;
	p = (p + (StructAlign - 1)) & ~(StructAlign - 1);
	if(p + size > b->end){
		b = mkbin(b, size);
		if(b == nil)
			return nil;
		*bin = b;
		p = b->pos;
	}
	b->pos = p + size;
	b->v = p;
	if(zero)
		memset((void*)p, 0, size);
	return (void*)p;
}

void*
bingrow(Bin **bin, void *op, ulong osize, ulong size, int zero)
{
	Bin *b;
	void *np;
	ulong p;

	p = (ulong)op;
	b = *bin;
	if(b != nil && p == b->v && p + size <= b->end){
		b->pos = p + size;
		if(zero)
			memset((char*)p + osize, 0, size - osize);
		return op;
	}
	np = binalloc(bin, size, zero);
	if(np == nil)
		return nil;
	memmove(np, op, osize);
	return np;
}

void
binfree(Bin **bin)
{
	Bin *last;

	while(*bin != nil){
		last = *bin;
		*bin = (*bin)->next;
		last->pos = (ulong)last->body;
		free(last);
	}
}
