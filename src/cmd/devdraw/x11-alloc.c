#include <u.h>
#include "x11-inc.h"
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

AUTOLIB(X11);

/*
 * Allocate a Memimage with an optional pixmap backing on the X server.
 */
Memimage*
_xallocmemimage(Rectangle r, u32int chan, int pixmap)
{
	int d, offset;
	Memimage *m;
	Xmem *xm;
	XImage *xi;

	m = _allocmemimage(r, chan);
	if(chan != GREY1 && chan != _x.chan)
		return m;
	if(_x.display == 0 || _x.windows == nil)
		return m;

	/*
	 * For bootstrapping, don't bother storing 1x1 images
	 * on the X server.  Memimageinit needs to allocate these
	 * and we memimageinit before we do the rest of the X stuff.
	 * Of course, 1x1 images on the server are useless anyway.
	 */
	if(Dx(r)==1 && Dy(r)==1)
		return m;

	xm = mallocz(sizeof(Xmem), 1);
	if(xm == nil){
		freememimage(m);
		return nil;
	}

	/*
	 * Allocate backing store.
	 */
	if(chan == GREY1)
		d = 1;
	else
		d = _x.depth;
	if(pixmap != PMundef)
		xm->pixmap = pixmap;
	else
		xm->pixmap = XCreatePixmap(_x.display, _x.windows->drawable, Dx(r), Dy(r), d);

	/*
	 * We want to align pixels on word boundaries.
	 */
	if(m->depth == 24)
		offset = r.min.x&3;
	else
		offset = r.min.x&(31/m->depth);
	r.min.x -= offset;
	assert(wordsperline(r, m->depth) <= m->width);

	/*
	 * Wrap our data in an XImage structure.
	 */
	xi = XCreateImage(_x.display, _x.vis, d,
		ZPixmap, 0, (char*)m->data->bdata, Dx(r), Dy(r),
		32, m->width*sizeof(u32int));
	if(xi == nil){
		freememimage(m);
		if(xm->pixmap != pixmap)
			XFreePixmap(_x.display, xm->pixmap);
		return nil;
	}

	xm->xi = xi;
	xm->r = r;

	/*
	 * Set the XImage parameters so that it looks exactly like
	 * a Memimage -- we're using the same data.
	 */
	if(m->depth < 8 || m->depth == 24)
		xi->bitmap_unit = 8;
	else
		xi->bitmap_unit = m->depth;
	xi->byte_order = LSBFirst;
	xi->bitmap_bit_order = MSBFirst;
	xi->bitmap_pad = 32;
	XInitImage(xi);
	XFlush(_x.display);

	m->X = xm;
	return m;
}

Memimage*
allocmemimage(Rectangle r, u32int chan)
{
	return _xallocmemimage(r, chan, PMundef);
}

void
freememimage(Memimage *m)
{
	Xmem *xm;

	if(m == nil)
		return;

	xm = m->X;
	if(xm && m->data->ref == 1){
		if(xm->xi){
			xm->xi->data = nil;
			XFree(xm->xi);
		}
		XFreePixmap(_x.display, xm->pixmap);
		free(xm);
		m->X = nil;
	}
	_freememimage(m);
}
