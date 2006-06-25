#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

struct Draw
{
	Point	deltas;
	Point	deltam;
	Memlayer		*dstlayer;
	Memimage	*src;
	Memimage	*mask;
	int	op;
};

static
void
ldrawop(Memimage *dst, Rectangle screenr, Rectangle clipr, void *etc, int insave)
{
	struct Draw *d;
	Point p0, p1;
	Rectangle oclipr, srcr, r, mr;
	int ok;

	d = etc;
	if(insave && d->dstlayer->save==nil)
		return;

	p0 = addpt(screenr.min, d->deltas);
	p1 = addpt(screenr.min, d->deltam);

	if(insave){
		r = rectsubpt(screenr, d->dstlayer->delta);
		clipr = rectsubpt(clipr, d->dstlayer->delta);
	}else
		r = screenr;

	/* now in logical coordinates */

	/* clipr may have narrowed what we should draw on, so clip if necessary */
	if(!rectinrect(r, clipr)){
		oclipr = dst->clipr;
		dst->clipr = clipr;
		ok = drawclip(dst, &r, d->src, &p0, d->mask, &p1, &srcr, &mr);
		dst->clipr = oclipr;
		if(!ok)
			return;
	}
	memdraw(dst, r, d->src, p0, d->mask, p1, d->op);
}

void
memdraw(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1, int op)
{
	struct Draw d;
	Rectangle srcr, tr, mr;
	Memlayer *dl, *sl;

	if(drawdebug)
		iprint("memdraw %p %R %p %P %p %P\n", dst, r, src, p0, mask, p1);

	if(mask == nil)
		mask = memopaque;

	if(mask->layer){
if(drawdebug)	iprint("mask->layer != nil\n");
		return;	/* too hard, at least for now */
	}

    Top:
	if(dst->layer==nil && src->layer==nil){
		memimagedraw(dst, r, src, p0, mask, p1, op);
		return;
	}

	if(drawclip(dst, &r, src, &p0, mask, &p1, &srcr, &mr) == 0){
if(drawdebug)	iprint("drawclip dstcr %R srccr %R maskcr %R\n", dst->clipr, src->clipr, mask->clipr);
		return;
	}

	/*
 	 * Convert to screen coordinates.
	 */
	dl = dst->layer;
	if(dl != nil){
		r.min.x += dl->delta.x;
		r.min.y += dl->delta.y;
		r.max.x += dl->delta.x;
		r.max.y += dl->delta.y;
	}
    Clearlayer:
	if(dl!=nil && dl->clear){
		if(src == dst){
			p0.x += dl->delta.x;
			p0.y += dl->delta.y;
			src = dl->screen->image;
		}
		dst = dl->screen->image;
		goto Top;
	}

	sl = src->layer;
	if(sl != nil){
		p0.x += sl->delta.x;
		p0.y += sl->delta.y;
		srcr.min.x += sl->delta.x;
		srcr.min.y += sl->delta.y;
		srcr.max.x += sl->delta.x;
		srcr.max.y += sl->delta.y;
	}

	/*
	 * Now everything is in screen coordinates.
	 * mask is an image.  dst and src are images or obscured layers.
	 */

	/*
	 * if dst and src are the same layer, just draw in save area and expose.
	 */
	if(dl!=nil && dst==src){
		if(dl->save == nil)
			return;	/* refresh function makes this case unworkable */
		if(rectXrect(r, srcr)){
			tr = r;
			if(srcr.min.x < tr.min.x){
				p1.x += tr.min.x - srcr.min.x;
				tr.min.x = srcr.min.x;
			}
			if(srcr.min.y < tr.min.y){
				p1.y += tr.min.x - srcr.min.x;
				tr.min.y = srcr.min.y;
			}
			if(srcr.max.x > tr.max.x)
				tr.max.x = srcr.max.x;
			if(srcr.max.y > tr.max.y)
				tr.max.y = srcr.max.y;
			memlhide(dst, tr);
		}else{
			memlhide(dst, r);
			memlhide(dst, srcr);
		}
		memdraw(dl->save, rectsubpt(r, dl->delta), dl->save,
			subpt(srcr.min, src->layer->delta), mask, p1, op);
		memlexpose(dst, r);
		return;
	}

	if(sl){
		if(sl->clear){
			src = sl->screen->image;
			if(dl != nil){
				r.min.x -= dl->delta.x;
				r.min.y -= dl->delta.y;
				r.max.x -= dl->delta.x;
				r.max.y -= dl->delta.y;
			}
			goto Top;
		}
		/* relatively rare case; use save area */
		if(sl->save == nil)
			return;	/* refresh function makes this case unworkable */
		memlhide(src, srcr);
		/* convert back to logical coordinates */
		p0.x -= sl->delta.x;
		p0.y -= sl->delta.y;
		srcr.min.x -= sl->delta.x;
		srcr.min.y -= sl->delta.y;
		srcr.max.x -= sl->delta.x;
		srcr.max.y -= sl->delta.y;
		src = src->layer->save;
	}

	/*
	 * src is now an image.  dst may be an image or a clear layer
	 */
	if(dst->layer==nil)
		goto Top;
	if(dst->layer->clear)
		goto Clearlayer;

	/*
	 * dst is an obscured layer
	 */
	d.deltas = subpt(p0, r.min);
	d.deltam = subpt(p1, r.min);
	d.dstlayer = dl;
	d.src = src;
	d.op = op;
	d.mask = mask;
	_memlayerop(ldrawop, dst, r, r, &d);
}
