#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

void
memfillcolor(Memimage *m, u32int val)
{
	_memfillcolor(m, val);
	if(m->X == nil)
		return;
	if((val & 0xFF) == 0xFF)	/* full alpha */
		_xfillcolor(m, m->r, _rgbatoimg(m, val));
	else
		_xputxdata(m, m->r);
}

void
_xfillcolor(Memimage *m, Rectangle r, u32int v)
{
	Point p;
	Xmem *xm;
	XGC gc;

	xm = m->X;
	assert(xm != nil);

	/*
	 * Set up fill context appropriately.
	 */
	if(m->chan == GREY1){
		gc = _x.gcfill0;
		if(_x.gcfill0color != v){
			XSetForeground(_x.display, gc, v);
			_x.gcfill0color = v;
		}
	}else{
		if(m->chan == CMAP8 && _x.usetable)
			v = _x.tox11[v];
		gc = _x.gcfill;
		if(_x.gcfillcolor != v){
			XSetForeground(_x.display, gc, v);
			_x.gcfillcolor = v;
		}
	}

	/*
	 * XFillRectangle takes coordinates relative to image rectangle.
	 */
	p = subpt(r.min, m->r.min);
	XFillRectangle(_x.display, xm->pixmap, gc, p.x, p.y, Dx(r), Dy(r));
}
