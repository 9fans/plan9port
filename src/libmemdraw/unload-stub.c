#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _unloadmemimage(i, r, data, ndata);
}
