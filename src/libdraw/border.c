#include <u.h>
#include <libc.h>
#include <draw.h>

void
borderop(Image *im, Rectangle r, int i, Image *color, Point sp, Drawop op)
{
	if(i < 0){
		r = insetrect(r, i);
		sp = addpt(sp, Pt(i,i));
		i = -i;
	}
	drawop(im, Rect(r.min.x, r.min.y, r.max.x, r.min.y+i),
		color, nil, sp, op);
	drawop(im, Rect(r.min.x, r.max.y-i, r.max.x, r.max.y),
		color, nil, Pt(sp.x, sp.y+Dy(r)-i), op);
	drawop(im, Rect(r.min.x, r.min.y+i, r.min.x+i, r.max.y-i),
		color, nil, Pt(sp.x, sp.y+i), op);
	drawop(im, Rect(r.max.x-i, r.min.y+i, r.max.x, r.max.y-i),
		color, nil, Pt(sp.x+Dx(r)-i, sp.y+i), op);
}

void
border(Image *im, Rectangle r, int i, Image *color, Point sp)
{
	borderop(im, r, i, color, sp, SoverD);
}
