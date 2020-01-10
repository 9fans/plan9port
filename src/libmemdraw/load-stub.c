#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _loadmemimage(i, r, data, ndata);
}
