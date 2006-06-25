#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

void
_memltorear(Memimage *i, Memimage *rear)
{
	Memlayer *l;
	Memscreen *s;
	Memimage *f, *r, *rr;
	Rectangle x;
	int overlap;

	l = i->layer;
	s = l->screen;
	while(l->rear != rear){
		r = l->rear;
		x = l->screenr;
		overlap = rectclip(&x, r->layer->screenr);
		if(overlap){
			memlhide(i, x);
			l->clear = 0;
		}
		/* swap l and r in screen's list */
		rr = r->layer->rear;
		f = l->front;
		if(rr == nil)
			s->rearmost = i;
		else
			rr->layer->front = i;
		if(f == nil)
			s->frontmost = r;
		else
			f->layer->rear = r;
		l->rear = rr;
		l->front = r;
		r->layer->rear = i;
		r->layer->front = f;
		if(overlap)
			memlexpose(r, x);
	}
}

void
memltorear(Memimage *i)
{
	_memltorear(i, nil);
	_memlsetclear(i->layer->screen);
}

void
memltorearn(Memimage **ip, int n)
{
	Memimage *i, *rear;
	Memscreen *s;

	if(n == 0)
		return;
	rear = nil;
	while(--n >= 0){
		i = *ip++;
		_memltorear(i, rear);
		rear = i;
	}
	s = rear->layer->screen;
	_memlsetclear(s);
}
