#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

static Image *scrtmp;
static Image *scrback;

void
scrtemps(void)
{
	int h;

	if(scrtmp)
		return;
	if(screensize(0, &h) == 0)
		h = 2048;
	scrtmp = allocimage(display, Rect(0, 0, 32, h), screen->chan, 0, 0);
	scrback = allocimage(display, Rect(0, 0, 32, h), screen->chan, 0, 0);
	if(scrtmp==0 || scrback==0)
		panic("scrtemps");
}

Rectangle
scrpos(Rectangle r, long p0, long p1, long tot)
{
	Rectangle q;
	int h;

	q = r;
	h = q.max.y-q.min.y;
	if(tot == 0)
		return q;
	if(tot > 1024L*1024L)
		tot>>=10, p0>>=10, p1>>=10;
	if(p0 > 0)
		q.min.y += h*p0/tot;
	if(p1 < tot)
		q.max.y -= h*(tot-p1)/tot;
	if(q.max.y < q.min.y+2){
		if(q.min.y+2 <= r.max.y)
			q.max.y = q.min.y+2;
		else
			q.min.y = q.max.y-2;
	}
	return q;
}

void
scrmark(Flayer *l, Rectangle r)
{
	r.max.x--;
	if(rectclip(&r, l->scroll))
		draw(l->f.b, r, l->f.cols[HIGH], nil, ZP);
}

void
scrunmark(Flayer *l, Rectangle r)
{
	if(rectclip(&r, l->scroll))
		draw(l->f.b, r, scrback, nil, Pt(0, r.min.y-l->scroll.min.y));
}

void
scrdraw(Flayer *l, long tot)
{
	Rectangle r, r1, r2;
	Image *b;

	scrtemps();
	if(l->f.b == 0)
		panic("scrdraw");
	r = l->scroll;
	r1 = r;
	if(l->visible == All){
		b = scrtmp;
		r1.min.x = 0;
		r1.max.x = Dx(r);
	}else
		b = l->f.b;
	r2 = scrpos(r1, l->origin, l->origin+l->f.nchars, tot);
	if(!eqrect(r2, l->lastsr)){
		l->lastsr = r2;
		draw(b, r1, l->f.cols[BORD], nil, ZP);
		draw(b, r2, l->f.cols[BACK], nil, r2.min);
		r2 = r1;
		r2.min.x = r2.max.x-1;
		draw(b, r2, l->f.cols[BORD], nil, ZP);
		if(b!=l->f.b)
			draw(l->f.b, r, b, nil, r1.min);
	}
}

void
scroll(Flayer *l, int but)
{
	int in = 0, oin;
	long tot = scrtotal(l);
	Rectangle scr, r, s, rt;
	int x, y, my, oy, h;
	long p0;

	s = l->scroll;
	x = s.min.x+FLSCROLLWID(l)/2;
	scr = scrpos(l->scroll, l->origin, l->origin+l->f.nchars, tot);
	r = scr;
	y = scr.min.y;
	my = mousep->xy.y;
	draw(scrback, Rect(0,0,Dx(l->scroll), Dy(l->scroll)), l->f.b, nil, l->scroll.min);
	do{
		oin = in;
		in = abs(x-mousep->xy.x)<=FLSCROLLWID(l)/2;
		if(oin && !in)
			scrunmark(l, r);
		if(in){
			scrmark(l, r);
			oy = y;
			my = mousep->xy.y;
			if(my < s.min.y)
				my = s.min.y;
			if(my >= s.max.y)
				my = s.max.y;
			if(!eqpt(mousep->xy, Pt(x, my)))
				moveto(mousectl, Pt(x, my));
			if(but == 1){
				p0 = l->origin-frcharofpt(&l->f, Pt(s.max.x, my));
				rt = scrpos(l->scroll, p0, p0+l->f.nchars, tot);
				y = rt.min.y;
			}else if(but == 2){
				y = my;
				if(y > s.max.y-2)
					y = s.max.y-2;
			}else if(but == 3){
				p0 = l->origin+frcharofpt(&l->f, Pt(s.max.x, my));
				rt = scrpos(l->scroll, p0, p0+l->f.nchars, tot);
				y = rt.min.y;
			}
			if(y != oy){
				scrunmark(l, r);
				r = rectaddpt(scr, Pt(0, y-scr.min.y));
				scrmark(l, r);
			}
		}
	}while(button(but));
	if(in){
		h = s.max.y-s.min.y;
		scrunmark(l, r);
		p0 = 0;
		if(but == 1)
			p0 = (long)(my-s.min.y)/l->f.font->height+1;
		else if(but == 2){
			if(tot > 1024L*1024L)
				p0 = ((tot>>10)*(y-s.min.y)/h)<<10;
			else
				p0 = tot*(y-s.min.y)/h;
		}else if(but == 3){
			p0 = l->origin+frcharofpt(&l->f, Pt(s.max.x, my));
			if(p0 > tot)
				p0 = tot;
		}
		scrorigin(l, but, p0);
	}
}
