#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

/*
 * Pull i towards top of screen, just behind front
*/
static
void
_memltofront(Memimage *i, Memimage *front, int fill)
{
	Memlayer *l;
	Memscreen *s;
	Memimage *f, *ff, *rr;
	Rectangle x;
	int overlap;

	l = i->layer;
	s = l->screen;
	while(l->front != front){
		f = l->front;
		x = l->screenr;
		overlap = rectclip(&x, f->layer->screenr);
		if(overlap){
			memlhide(f, x);
			f->layer->clear = 0;
		}
		/* swap l and f in screen's list */
		ff = f->layer->front;
		rr = l->rear;
		if(ff == nil)
			s->frontmost = i;
		else
			ff->layer->rear = i;
		if(rr == nil)
			s->rearmost = f;
		else
			rr->layer->front = f;
		l->front = ff;
		l->rear = f;
		f->layer->front = i;
		f->layer->rear = rr;
		if(overlap && fill)
			memlexpose(i, x);
	}
}

void
_memltofrontfill(Memimage *i, int fill)
{
	_memltofront(i, nil, fill);
	_memlsetclear(i->layer->screen);
}

void
memltofront(Memimage *i)
{
	_memltofront(i, nil, 1);
	_memlsetclear(i->layer->screen);
}

void
memltofrontn(Memimage **ip, int n)
{
	Memimage *i, *front;
	Memscreen *s;

	if(n == 0)
		return;
	front = nil;
	while(--n >= 0){
		i = *ip++;
		_memltofront(i, front, 1);
		front = i;
	}
	s = front->layer->screen;
	_memlsetclear(s);
}
