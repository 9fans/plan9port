#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

u32int
pixelbits(Memimage *m, Point p)
{
	if(m->X)
		_xgetxdata(m, Rect(p.x, p.y, p.x+1, p.y+1));
	return _pixelbits(m, p);
}
