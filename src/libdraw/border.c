#include <u.h>
#include <libc.h>
#include <draw.h>

void
border(Image *im, Rectangle r, int i, Image *color, Point sp)
{
	if(i < 0){
		r = insetrect(r, i);
		sp = addpt(sp, Pt(i,i));
		i = -i;
	}
	draw(im, Rect(r.min.x, r.min.y, r.max.x, r.min.y+i),
		color, nil, sp);
	draw(im, Rect(r.min.x, r.max.y-i, r.max.x, r.max.y),
		color, nil, Pt(sp.x, sp.y+Dy(r)-i));
	draw(im, Rect(r.min.x, r.min.y+i, r.min.x+i, r.max.y-i),
		color, nil, Pt(sp.x, sp.y+i));
	draw(im, Rect(r.max.x-i, r.min.y+i, r.max.x, r.max.y-i),
		color, nil, Pt(sp.x+Dx(r)-i, sp.y+i));
}
