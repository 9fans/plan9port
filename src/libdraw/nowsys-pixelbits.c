#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

u32int
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}


