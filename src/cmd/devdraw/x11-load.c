#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int n;

	n = _loadmemimage(i, r, data, ndata);
	if(n > 0 && i->X)
		_xputxdata(i, r);
	return n;
}
