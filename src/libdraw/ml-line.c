#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

struct Lline
{
	Point			p0;
	Point			p1;
	Point			delta;
	int			end0;
	int			end1;
	int			radius;
	Point			sp;
	Memlayer		*dstlayer;
	Memimage	*src;
	int			op;
};

static void llineop(Memimage*, Rectangle, Rectangle, void*, int);

static
void
_memline(Memimage *dst, Point p0, Point p1, int end0, int end1, int radius, Memimage *src, Point sp, Rectangle clipr, int op)
{
	Rectangle r;
	struct Lline ll;
	Point d;
	int srcclipped;
	Memlayer *dl;

	if(radius < 0)
		return;
	if(src->layer)	/* can't draw line with layered source */
		return;
	srcclipped = 0;

   Top:
	dl = dst->layer;
	if(dl == nil){
		_memimageline(dst, p0, p1, end0, end1, radius, src, sp, clipr, op);
		return;
	}
	if(!srcclipped){
		d = subpt(sp, p0);
		if(rectclip(&clipr, rectsubpt(src->clipr, d)) == 0)
			return;
		if((src->flags&Frepl)==0 && rectclip(&clipr, rectsubpt(src->r, d))==0)
			return;
		srcclipped = 1;
	}

	/* dst is known to be a layer */
	p0.x += dl->delta.x;
	p0.y += dl->delta.y;
	p1.x += dl->delta.x;
	p1.y += dl->delta.y;
	clipr.min.x += dl->delta.x;
	clipr.min.y += dl->delta.y;
	clipr.max.x += dl->delta.x;
	clipr.max.y += dl->delta.y;
	if(dl->clear){
		dst = dst->layer->screen->image;
		goto Top;
	}

	/* XXX */
	/* this is not the correct set of tests */
//	if(log2[dst->depth] != log2[src->depth] || log2[dst->depth]!=3)
//		return;

	/* can't use sutherland-cohen clipping because lines are wide */
	r = memlinebbox(p0, p1, end0, end1, radius);
	/*
	 * r is now a bounding box for the line;
	 * use it as a clipping rectangle for subdivision
	 */
	if(rectclip(&r, clipr) == 0)
		return;
	ll.p0 = p0;
	ll.p1 = p1;
	ll.end0 = end0;
	ll.end1 = end1;
	ll.sp = sp;
	ll.dstlayer = dst->layer;
	ll.src = src;
	ll.radius = radius;
	ll.delta = dl->delta;
	ll.op = op;
	_memlayerop(llineop, dst, r, r, &ll);
}

static
void
llineop(Memimage *dst, Rectangle screenr, Rectangle clipr, void *etc, int insave)
{
	struct Lline *ll;
	Point p0, p1;

	USED(screenr.min.x);
	ll = etc;
	if(insave && ll->dstlayer->save==nil)
		return;
	if(!rectclip(&clipr, screenr))
		return;
	if(insave){
		p0 = subpt(ll->p0, ll->delta);
		p1 = subpt(ll->p1, ll->delta);
		clipr = rectsubpt(clipr, ll->delta);
	}else{
		p0 = ll->p0;
		p1 = ll->p1;
	}
	_memline(dst, p0, p1, ll->end0, ll->end1, ll->radius, ll->src, ll->sp, clipr, ll->op);
}

void
memline(Memimage *dst, Point p0, Point p1, int end0, int end1, int radius, Memimage *src, Point sp, int op)
{
	_memline(dst, p0, p1, end0, end1, radius, src, sp, dst->clipr, op);
}
