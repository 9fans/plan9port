#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _cloadmemimage(i, r, data, ndata);
}
