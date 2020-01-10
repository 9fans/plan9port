#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp, Memimage *mask, Point mp, int op)
{
	Memdrawparam *par;

	if((par = _memimagedrawsetup(dst, r, src, sp, mask, mp, op)) == nil)
		return;
	_memimagedraw(par);
}

void
memfillcolor(Memimage *m, u32int val)
{
	_memfillcolor(m, val);
}

u32int
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}
