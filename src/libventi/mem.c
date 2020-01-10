#include <u.h>
#include <libc.h>
#include <venti.h>

enum {
	IdealAlignment = 32,
	ChunkSize 	= 128*1024
};


void
vtfree(void *p)
{
	if(p == 0)
		return;
	free(p);
}

void *
vtmalloc(int size)
{
	void *p;

	p = malloc(size);
	if(p == 0)
		sysfatal("vtmalloc: out of memory");
	setmalloctag(p, getcallerpc(&size));
	return p;
}

void *
vtmallocz(int size)
{
	void *p = vtmalloc(size);
	memset(p, 0, size);
	setmalloctag(p, getcallerpc(&size));
	return p;
}

void *
vtrealloc(void *p, int size)
{
	if(p == nil)
		return vtmalloc(size);
	p = realloc(p, size);
	if(p == 0)
		sysfatal("vtMemRealloc: out of memory");
	setrealloctag(p, getcallerpc(&size));
	return p;
}

void *
vtbrk(int n)
{
	static Lock lk;
	static uchar *buf;
	static int nbuf, nchunk;
	int align, pad;
	void *p;

	if(n >= IdealAlignment)
		align = IdealAlignment;
	else if(n > 8)
		align = 8;
	else
		align = 4;

	lock(&lk);
	pad = (align - (uintptr)buf) & (align-1);
	if(n + pad > nbuf) {
		buf = vtmallocz(ChunkSize);
		nbuf = ChunkSize;
		pad = (align - (uintptr)buf) & (align-1);
		nchunk++;
	}

	assert(n + pad <= nbuf);

	p = buf + pad;
	buf += pad + n;
	nbuf -= pad + n;
	unlock(&lk);

	return p;
}
