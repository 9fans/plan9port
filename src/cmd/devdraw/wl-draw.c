#include "u.h"
#include "libc.h"
#include "draw.h"
#include "memdraw.h"

Memimage*
allocmemimage(Rectangle r, u32int chan)
{
	return _allocmemimage(r, chan);
}

void
freememimage(Memimage *i)
{
	_freememimage(i);
}

void
memfillcolor(Memimage *i, u32int val)
{
	_memfillcolor(i, val);
}


int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _cloadmemimage(i, r, data, ndata);
}

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp, Memimage *mask, Point mp, int op)
{
	Memdrawparam *par;

	par = _memimagedrawsetup(dst, r, src, sp, mask, mp, op);
	if(par == nil)
		return;
	_memimagedraw(par);
}

u32int
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _loadmemimage(i, r, data, ndata);
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _unloadmemimage(i, r, data, ndata);
}
