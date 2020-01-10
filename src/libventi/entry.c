#include <u.h>
#include <libc.h>
#include <venti.h>
#include "cvt.h"

static int
checksize(int n)
{
	if(n < 256) {
		werrstr("bad block size %#ux", n);
		return -1;
	}
	return 0;
}

// _VtEntryBig integer format is floating-point:
// (n>>5) << (n&31).
// Convert this number; must be exact or return -1.
int
vttobig(ulong n)
{
	int shift;
	ulong n0;

	n0 = n;
	shift = 0;
	while(n >= (1<<(16 - 5))) {
		if(n & 1)
			return -1;
		shift++;
		n >>= 1;
	}

	n = (n<<5) | shift;
	if(((n>>5)<<(n&31)) != n0)
		sysfatal("vttobig %#lux => %#lux failed", n0, n);
	return n;
}

void
vtentrypack(VtEntry *e, uchar *p, int index)
{
	ulong t32;
	int flags;
	uchar *op;
	int depth;
	int psize, dsize;

	p += index * VtEntrySize;
	op = p;

	depth = e->type&VtTypeDepthMask;
	flags = (e->flags&~(_VtEntryDir|_VtEntryDepthMask));
	flags |= depth << _VtEntryDepthShift;
	if(e->type - depth == VtDirType)
		flags |= _VtEntryDir;
	U32PUT(p, e->gen);
	p += 4;
	psize = e->psize;
	dsize = e->dsize;
	if(psize >= (1<<16) || dsize >= (1<<16)) {
		flags |= _VtEntryBig;
		psize = vttobig(psize);
		dsize = vttobig(dsize);
		if(psize < 0 || dsize < 0)
			sysfatal("invalid entry psize/dsize: %ld/%ld", e->psize, e->dsize);
	}
	U16PUT(p, psize);
	p += 2;
	U16PUT(p, dsize);
	p += 2;
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
	p++;
	if(e->flags & _VtEntryBig) {
		e->psize = (e->psize>>5)<<(e->psize & 31);
		e->dsize = (e->dsize>>5)<<(e->dsize & 31);
	}
	e->type = (e->flags&_VtEntryDir) ? VtDirType : VtDataType;
	e->type += (e->flags & _VtEntryDepthMask) >> _VtEntryDepthShift;
	e->flags &= ~(_VtEntryDir|_VtEntryDepthMask|_VtEntryBig);
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
