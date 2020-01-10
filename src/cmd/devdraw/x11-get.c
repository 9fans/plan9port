#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

static void
addrect(Rectangle *rp, Rectangle r)
{
	if(rp->min.x >= rp->max.x)
		*rp = r;
	else
		combinerect(rp, r);
}

XImage*
_xgetxdata(Memimage *m, Rectangle r)
{
	int x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;

	xm = m->X;
	if(xm == nil)
		return nil;

	if(xm->dirty == 0)
		return xm->xi;

	abort();	/* should never call this now */

	r = xm->dirtyr;
	if(Dx(r)==0 || Dy(r)==0)
		return xm->xi;

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temp for Digital UNIX */
	xdelta = subpt(r.min, tp);

	XGetSubImage(_x.display, xm->pixmap, delta.x, delta.y, Dx(r), Dy(r),
		AllPlanes, ZPixmap, xm->xi, xdelta.x, delta.y);

	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.toplan9[*p];
	}
	xm->dirty = 0;
	xm->dirtyr = Rect(0,0,0,0);
	return xm->xi;
}

void
_xputxdata(Memimage *m, Rectangle r)
{
	int x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;
	XGC gc;
	XImage *xi;

	xm = m->X;
	if(xm == nil)
		return;

	xi = xm->xi;
	gc = m->chan==GREY1 ? _x.gccopy0 : _x.gccopy;

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temporary on Digital UNIX */
	xdelta = subpt(r.min, tp);

	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.tox11[*p];
	}

	XPutImage(_x.display, xm->pixmap, gc, xi, xdelta.x, xdelta.y, delta.x, delta.y,
		Dx(r), Dy(r));

	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.toplan9[*p];
	}
}

void
_xdirtyxdata(Memimage *m, Rectangle r)
{
	Xmem *xm;

	xm = m->X;
	if(xm == nil)
		return;

	xm->dirty = 1;
	addrect(&xm->dirtyr, r);
}
