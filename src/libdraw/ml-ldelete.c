#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>

void
memldelete(Memimage *i)
{
	Memscreen *s;
	Memlayer *l;

	l = i->layer;
	/* free backing store and disconnect refresh, to make pushback fast */
	freememimage(l->save);
	l->save = nil;
	l->refreshptr = nil;
	memltorear(i);

	/* window is now the rearmost;  clean up screen structures and deallocate */
	s = i->layer->screen;
	if(s->fill){
		i->clipr = i->r;
		memdraw(i, i->r, s->fill, i->r.min, nil, i->r.min, S);
	}
	if(l->front){
		l->front->layer->rear = nil;
		s->rearmost = l->front;
	}else{
		s->frontmost = nil;
		s->rearmost = nil;
	}
	free(l);
	freememimage(i);
}

/*
 * Just free the data structures, don't do graphics
 */
void
memlfree(Memimage *i)
{
	Memlayer *l;

	l = i->layer;
	freememimage(l->save);
	free(l);
	freememimage(i);
}

void
_memlsetclear(Memscreen *s)
{
	Memimage *i, *j;
	Memlayer *l;

	for(i=s->rearmost; i; i=i->layer->front){
		l = i->layer;
		l->clear = rectinrect(l->screenr, l->screen->image->clipr);
		if(l->clear)
			for(j=l->front; j; j=j->layer->front)
				if(rectXrect(l->screenr, j->layer->screenr)){
					l->clear = 0;
					break;
				}
	}
}
