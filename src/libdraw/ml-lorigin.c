#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

/*
 * Place i so i->r.min = log, i->layer->screenr.min == scr.
*/
int
memlorigin(Memimage *i, Point log, Point scr)
{
	Memlayer *l;
	Memscreen *s;
	Memimage *t, *shad, *nsave;
	Rectangle x, newr, oldr;
	Point delta;
	int overlap, eqlog, eqscr, wasclear;

	l = i->layer;
	s = l->screen;
	oldr = l->screenr;
	newr = Rect(scr.x, scr.y, scr.x+Dx(oldr), scr.y+Dy(oldr));
	eqscr = eqpt(scr, oldr.min);
	eqlog = eqpt(log, i->r.min);
	if(eqscr && eqlog)
		return 0;
	nsave = nil;
	if(eqlog==0 && l->save!=nil){
		nsave = allocmemimage(Rect(log.x, log.y, log.x+Dx(oldr), log.y+Dy(oldr)), i->chan);
		if(nsave == nil)
			return -1;
	}

	/*
	 * Bring it to front and move logical coordinate system.
	 */
	memltofront(i);
	wasclear = l->clear;
	if(nsave){
		if(!wasclear)
			memimagedraw(nsave, nsave->r, l->save, l->save->r.min, nil, Pt(0,0), S);
		freememimage(l->save);
		l->save = nsave;
	}
	delta = subpt(log, i->r.min);
	i->r = rectaddpt(i->r, delta);
	i->clipr = rectaddpt(i->clipr, delta);
	l->delta = subpt(l->screenr.min, i->r.min);
	if(eqscr)
		return 0;

	/*
	 * To clean up old position, make a shadow window there, don't paint it,
	 * push it behind this one, and (later) delete it.  Because the refresh function
	 * for this fake window is a no-op, this will cause no graphics action except
	 * to restore the background and expose the windows previously hidden.
	 */
	shad = memlalloc(s, oldr, memlnorefresh, nil, DNofill);
	if(shad == nil)
		return -1;
	s->frontmost = i;
	if(s->rearmost == i)
		s->rearmost = shad;
	else
		l->rear->layer->front = shad;
	shad->layer->front = i;
	shad->layer->rear = l->rear;
	l->rear = shad;
	l->front = nil;
	shad->layer->clear = 0;

	/*
	 * Shadow is now holding down the fort at the old position.
	 * Move the window and hide things obscured by new position.
	 */
	for(t=l->rear->layer->rear; t!=nil; t=t->layer->rear){
		x = newr;
		overlap = rectclip(&x, t->layer->screenr);
		if(overlap){
			memlhide(t, x);
			t->layer->clear = 0;
		}
	}
	l->screenr = newr;
	l->delta = subpt(scr, i->r.min);
	l->clear = rectinrect(newr, l->screen->image->clipr);

	/*
	 * Everything's covered.  Copy to new position and delete shadow window.
	 */
	if(wasclear)
		memdraw(s->image, newr, s->image, oldr.min, nil, Pt(0,0), S);
	else
		memlexpose(i, newr);
	memldelete(shad);

	return 1;
}

void
memlnorefresh(Memimage *l, Rectangle r, void *v)
{
	USED(l);
	USED(r.min.x);
	USED(v);
}
