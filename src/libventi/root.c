#include <u.h>
#include <libc.h>
#include <venti.h>
#include "cvt.h"

static int
checksize(int n)
{
	if(n < 256) {
		werrstr("bad block size");
		return -1;
	}
	return 0;
}

extern int vttobig(ulong);

void
vtrootpack(VtRoot *r, uchar *p)
{
	uchar *op = p;
	int vers, bsize;

	vers = VtRootVersion;
	bsize = r->blocksize;
	if(bsize >= (1<<16)) {
		vers |= _VtRootVersionBig;
		bsize = vttobig(bsize);
		if(bsize < 0)
			sysfatal("invalid root blocksize: %#lx", r->blocksize);
	}
	U16PUT(p, vers);
	p += 2;
	memmove(p, r->name, sizeof(r->name));
	p += sizeof(r->name);
	memmove(p, r->type, sizeof(r->type));
	p += sizeof(r->type);
	memmove(p, r->score, VtScoreSize);
	p +=  VtScoreSize;
	U16PUT(p, bsize);
	p += 2;
	memmove(p, r->prev, VtScoreSize);
	p += VtScoreSize;

	assert(p-op == VtRootSize);
}

int
vtrootunpack(VtRoot *r, uchar *p)
{
	uchar *op = p;
	uint vers;
	memset(r, 0, sizeof(*r));

	vers = U16GET(p);
	if((vers&~_VtRootVersionBig) != VtRootVersion) {
		werrstr("unknown root version");
		return -1;
	}
	p += 2;
	memmove(r->name, p, sizeof(r->name));
	r->name[sizeof(r->name)-1] = 0;
	p += sizeof(r->name);
	memmove(r->type, p, sizeof(r->type));
	r->type[sizeof(r->type)-1] = 0;
	p += sizeof(r->type);
	memmove(r->score, p, VtScoreSize);
	p +=  VtScoreSize;
	r->blocksize = U16GET(p);
	if(vers & _VtRootVersionBig)
		r->blocksize = (r->blocksize >> 5) << (r->blocksize & 31);
	if(checksize(r->blocksize) < 0)
		return -1;
	p += 2;
	memmove(r->prev, p, VtScoreSize);
	p += VtScoreSize;

	assert(p-op == VtRootSize);
	return 0;
}
