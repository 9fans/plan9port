#include <u.h>
#include <libc.h>
#include <venti.h>
#include "cvt.h"

static int
checksize(int n)
{
	if(n < 256 || n > VtMaxLumpSize) {
		werrstr("bad block size %#ux", n);
		return -1;
	}
	return 0;
}

void
vtentrypack(VtEntry *e, uchar *p, int index)
{
	ulong t32;
	int flags;
	uchar *op;
	int depth;

	p += index * VtEntrySize;
	op = p;

	U32PUT(p, e->gen);
	p += 4;
	U16PUT(p, e->psize);
	p += 2;
	U16PUT(p, e->dsize);
	p += 2;
	depth = e->type&VtTypeDepthMask;
	flags = (e->flags&~(_VtEntryDir|_VtEntryDepthMask));
	flags |= depth << _VtEntryDepthShift;
	if(e->type - depth == VtDirType)
		flags |= _VtEntryDir;
	U8PUT(p, flags);
	p++;
	memset(p, 0, 5);
	p += 5;
	U48PUT(p, e->size, t32);
	p += 6;
	memmove(p, e->score, VtScoreSize);
	p += VtScoreSize;

	assert(p-op == VtEntrySize);
}

int
vtentryunpack(VtEntry *e, uchar *p, int index)
{
	uchar *op;

	p += index * VtEntrySize;
	op = p;

	e->gen = U32GET(p);
	p += 4;
	e->psize = U16GET(p);
	p += 2;
	e->dsize = U16GET(p);
	p += 2;
	e->flags = U8GET(p);
	e->type = (e->flags&_VtEntryDir) ? VtDirType : VtDataType;
	e->type += (e->flags & _VtEntryDepthMask) >> _VtEntryDepthShift;
	e->flags &= ~(_VtEntryDir|_VtEntryDepthMask);
	p++;
	p += 5;
	e->size = U48GET(p);
	p += 6;
	memmove(e->score, p, VtScoreSize);
	p += VtScoreSize;

	assert(p-op == VtEntrySize);
	
	if(!(e->flags & VtEntryActive))
		return 0;

	/* 
	 * Some old vac files use psize==0 and dsize==0 when the
	 * file itself has size 0 or is zeros.  Just to make programs not
	 * have to figure out what block sizes of 0 means, rewrite them.
	 */
	if(e->psize == 0 && e->dsize == 0
	&& memcmp(e->score, vtzeroscore, VtScoreSize) == 0){
		e->psize = 4096;
		e->dsize = 4096;
	}
	if(checksize(e->psize) < 0 || checksize(e->dsize) < 0)
		return -1;

	return 0;
}

