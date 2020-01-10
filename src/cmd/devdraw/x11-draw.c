#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

static int xdraw(Memdrawparam*);

/*
 * The X acceleration doesn't fit into the standard hwaccel
 * model because we have the extra steps of pulling the image
 * data off the server and putting it back when we're done.
 */
void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
	Memimage *mask, Point mp, int op)
{
	Memdrawparam *par;

	if((par = _memimagedrawsetup(dst, r, src, sp, mask, mp, op)) == nil)
		return;

	/* only fetch dst data if we need it */
	if((par->state&(Simplemask|Fullmask)) != (Simplemask|Fullmask))
		_xgetxdata(par->dst, par->r);

	/* always fetch source and mask */
	_xgetxdata(par->src, par->sr);
	_xgetxdata(par->mask, par->mr);

	/* now can run memimagedraw on the in-memory bits */
	_memimagedraw(par);

	if(xdraw(par))
		return;

	/* put bits back on x server */
	_xputxdata(par->dst, par->r);
}

static int
xdraw(Memdrawparam *par)
{
	u32int sdval;
	uint m, state;
	Memimage *dst, *mask;
	Point dp, mp;
	Rectangle r;
	Xmem *xdst, *xmask;
	XGC gc;

	if(par->dst->X == nil)
		return 0;

	dst   = par->dst;
	mask  = par->mask;
	r     = par->r;
	state = par->state;

	/*
	 * If we have an opaque mask and source is one opaque pixel,
	 * we can convert to the destination format and just XFillRectangle.
	 */
	m = Simplesrc|Fullsrc|Simplemask|Fullmask;
	if((state&m) == m){
		_xfillcolor(dst, r, par->sdval);
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * If no source alpha and an opaque mask, we can just copy
	 * the source onto the destination.  If the channels are the
	 * same and the source is not replicated, XCopyArea works.
	 *
	 * This is disabled because Ubuntu Precise seems to ship with
	 * a buggy X server that sometimes drops the XCopyArea
	 * requests on the floor.
	m = Simplemask|Fullmask;
	if((state&(m|Replsrc))==m && src->chan==dst->chan && src->X){
		Xmem *xsrc;
		Point sp;

		xdst = dst->X;
		xsrc = src->X;
		dp = subpt(r.min,       dst->r.min);
		sp = subpt(par->sr.min, src->r.min);
		gc = dst->chan==GREY1 ?  _x.gccopy0 : _x.gccopy;

		XCopyArea(_x.display, xsrc->pixmap, xdst->pixmap, gc,
			sp.x, sp.y, Dx(r), Dy(r), dp.x, dp.y);
	/*	xdirtyxdata(dst, r); * /
		return 1;
	}
	 */

	/*
	 * If no source alpha, a 1-bit mask, and a simple source,
	 * we can copy through the mask onto the destination.
	 */
	if(dst->X && mask->X && !(mask->flags&Frepl)
	&& mask->chan==GREY1 && (state&Simplesrc)){
		xdst = dst->X;
		xmask = mask->X;
		sdval = par->sdval;

		dp = subpt(r.min, dst->r.min);
		mp = subpt(r.min, subpt(par->mr.min, mask->r.min));

		if(dst->chan == GREY1){
			gc = _x.gcsimplesrc0;
			if(_x.gcsimplesrc0color != sdval){
				XSetForeground(_x.display, gc, sdval);
				_x.gcsimplesrc0color = sdval;
			}
			if(_x.gcsimplesrc0pixmap != xmask->pixmap){
				XSetStipple(_x.display, gc, xmask->pixmap);
				_x.gcsimplesrc0pixmap = xmask->pixmap;
			}
		}else{
			/* this doesn't work on rob's mac?  */
			return 0;
			/* gc = _x.gcsimplesrc;
			if(dst->chan == CMAP8 && _x.usetable)
				sdval = _x.tox11[sdval];

			if(_x.gcsimplesrccolor != sdval){
				XSetForeground(_x.display, gc, sdval);
				_x.gcsimplesrccolor = sdval;
			}
			if(_x.gcsimplesrcpixmap != xmask->pixmap){
				XSetStipple(_x.display, gc, xmask->pixmap);
				_x.gcsimplesrcpixmap = xmask->pixmap;
			}
			*/
		}
		XSetTSOrigin(_x.display, gc, mp.x, mp.y);
		XFillRectangle(_x.display, xdst->pixmap, gc, dp.x, dp.y,
			Dx(r), Dy(r));
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * Can't accelerate.
	 */
	return 0;
}
