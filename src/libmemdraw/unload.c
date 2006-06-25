#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int
_unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int y, l;
	uchar *q;

	if(!rectinrect(r, i->r))
		return -1;
	l = bytesperline(r, i->depth);
	if(ndata < l*Dy(r))
		return -1;
	ndata = l*Dy(r);
	q = byteaddr(i, r.min);
	for(y=r.min.y; y<r.max.y; y++){
		memmove(data, q, l);
		q += i->width*sizeof(u32int);
		data += l;
	}
	return ndata;
}
