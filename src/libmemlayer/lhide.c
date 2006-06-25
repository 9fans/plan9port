#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

/*
 * Hide puts that portion of screenr now on the screen into the window's save area.
 * Expose puts that portion of screenr now in the save area onto the screen.
 *
 * Hide and Expose both require that the layer structures in the screen
 * match the geometry they are being asked to update, that is, they update the
 * save area (hide) or screen (expose) based on what those structures tell them.
 * This means they must be called at the correct time during window shuffles.
 */

static
void
lhideop(Memimage *src, Rectangle screenr, Rectangle clipr, void *etc, int insave)
{
	Rectangle r;
	Memlayer *l;

	USED(clipr.min.x);
	USED(insave);
	l = etc;
	if(src != l->save){	/* do nothing if src is already in save area */
		r = rectsubpt(screenr, l->delta);
		memdraw(l->save, r, src, screenr.min, nil, screenr.min, S);
	}
}

void
memlhide(Memimage *i, Rectangle screenr)
{
	if(i->layer->save == nil)
		return;
	if(rectclip(&screenr, i->layer->screen->image->r) == 0)
		return;
	_memlayerop(lhideop, i, screenr, screenr, i->layer);
}

static
void
lexposeop(Memimage *dst, Rectangle screenr, Rectangle clipr, void *etc, int insave)
{
	Memlayer *l;
	Rectangle r;

	USED(clipr.min.x);
	if(insave)	/* if dst is save area, don't bother */
		return;
	l = etc;
	r = rectsubpt(screenr, l->delta);
	if(l->save)
		memdraw(dst, screenr, l->save, r.min, nil, r.min, S);
	else
		l->refreshfn(dst, r, l->refreshptr);
}

void
memlexpose(Memimage *i, Rectangle screenr)
{
	if(rectclip(&screenr, i->layer->screen->image->r) == 0)
		return;
	_memlayerop(lexposeop, i, screenr, screenr, i->layer);
}
