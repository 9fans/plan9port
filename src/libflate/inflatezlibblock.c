#include <u.h>
#include <libc.h>
#include <flate.h>
#include "zlib.h"

typedef struct Block	Block;

struct Block
{
	uchar	*pos;
	uchar	*limit;
};

static int
blgetc(void *vb)
{
	Block *b;

	b = vb;
	if(b->pos >= b->limit)
		return -1;
	return *b->pos++;
}

static int
blwrite(void *vb, void *buf, int n)
{
	Block *b;

	b = vb;

	if(n > b->limit - b->pos)
		n = b->limit - b->pos;
	memmove(b->pos, buf, n);
	b->pos += n;
	return n;
}

int
inflatezlibblock(uchar *dst, int dsize, uchar *src, int ssize)
{
	Block bd, bs;
	int ok;

	if(ssize < 6)
		return FlateInputFail;

	if(((src[0] << 8) | src[1]) % 31)
		return FlateCorrupted;
	if((src[0] & ZlibMeth) != ZlibDeflate
	|| (src[0] & ZlibCInfo) > ZlibWin32k)
		return FlateCorrupted;

	bs.pos = src + 2;
	bs.limit = src + ssize - 6;

	bd.pos = dst;
	bd.limit = dst + dsize;

	ok = inflate(&bd, blwrite, &bs, blgetc);
	if(ok != FlateOk)
		return ok;

	if(adler32(1, dst, bs.pos - dst) != ((bs.pos[0] << 24) | (bs.pos[1] << 16) | (bs.pos[2] << 8) | bs.pos[3]))
		return FlateCorrupted;

	return bd.pos - dst;
}
