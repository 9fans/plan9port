#include <u.h>
#include <libc.h>
#include <draw.h>
#include <mouse.h>
#include <frame.h>

static
int
region(int a, int b)
{
	if(a < b)
		return -1;
	if(a == b)
		return 0;
	return 1;
}

void
frselect(Frame *f, Mousectl *mc)	/* when called, button 1 is down */
{
	ulong p0, p1, q;
	Point mp, pt0, pt1, qt;
	int reg, b, scrled;

	mp = mc->m.xy;
	b = mc->m.buttons;

	f->modified = 0;
	frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 0);
	p0 = p1 = frcharofpt(f, mp);
	f->p0 = p0;
	f->p1 = p1;
	pt0 = frptofchar(f, p0);
	pt1 = frptofchar(f, p1);
	frdrawsel(f, pt0, p0, p1, 1);
	reg = 0;
	do{
		scrled = 0;
		if(f->scroll){
			if(mp.y < f->r.min.y){
				(*f->scroll)(f, -(f->r.min.y-mp.y)/(int)f->font->height-1);
				p0 = f->p1;
				p1 = f->p0;
				scrled = 1;
			}else if(mp.y > f->r.max.y){
				(*f->scroll)(f, (mp.y-f->r.max.y)/(int)f->font->height+1);
				p0 = f->p0;
				p1 = f->p1;
				scrled = 1;
			}
			if(scrled){
				if(reg != region(p1, p0))
					q = p0, p0 = p1, p1 = q;	/* undo the swap that will happen below */
				pt0 = frptofchar(f, p0);
				pt1 = frptofchar(f, p1);
				reg = region(p1, p0);
			}
		}
		q = frcharofpt(f, mp);
		if(p1 != q){
			if(reg != region(q, p0)){	/* crossed starting point; reset */
				if(reg > 0)
					frdrawsel(f, pt0, p0, p1, 0);
				else if(reg < 0)
					frdrawsel(f, pt1, p1, p0, 0);
				p1 = p0;
				pt1 = pt0;
				reg = region(q, p0);
				if(reg == 0)
					frdrawsel(f, pt0, p0, p1, 1);
			}
			qt = frptofchar(f, q);
			if(reg > 0){
				if(q > p1)
					frdrawsel(f, pt1, p1, q, 1);
				else if(q < p1)
					frdrawsel(f, qt, q, p1, 0);
			}else if(reg < 0){
				if(q > p1)
					frdrawsel(f, pt1, p1, q, 0);
				else
					frdrawsel(f, qt, q, p1, 1);
			}
			p1 = q;
			pt1 = qt;
		}
		f->modified = 0;
		if(p0 < p1) {
			f->p0 = p0;
			f->p1 = p1;
		}
		else {
			f->p0 = p1;
			f->p1 = p0;
		}
		if(scrled)
			(*f->scroll)(f, 0);
		flushimage(f->display, 1);
		if(!scrled)
			readmouse(mc);
		mp = mc->m.xy;
	}while(mc->m.buttons == b);
}

void
frselectpaint(Frame *f, Point p0, Point p1, Image *col)
{
	int n;
	Point q0, q1;

	q0 = p0;
	q1 = p1;
	q0.y += f->font->height;
	q1.y += f->font->height;
	n = (p1.y-p0.y)/f->font->height;
	if(f->b == nil)
		drawerror(f->display, "frselectpaint b==0");
	if(p0.y == f->r.max.y)
		return;
	if(n == 0)
		draw(f->b, Rpt(p0, q1), col, nil, ZP);
	else{
		if(p0.x >= f->r.max.x)
			p0.x = f->r.max.x-1;
		draw(f->b, Rect(p0.x, p0.y, f->r.max.x, q0.y), col, nil, ZP);
		if(n > 1)
			draw(f->b, Rect(f->r.min.x, q0.y, f->r.max.x, p1.y),
				col, nil, ZP);
		draw(f->b, Rect(f->r.min.x, p1.y, q1.x, q1.y),
			col, nil, ZP);
	}
}
