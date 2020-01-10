#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

Memimage*
allocmemimage(Rectangle r, u32int chan)
{
	return _allocmemimage(r, chan);
}

void
freememimage(Memimage *m)
{
	_freememimage(m);
}
