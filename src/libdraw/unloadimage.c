#include <u.h>
#include <libc.h>
#include <draw.h>

int
unloadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	int bpl, n, ntot, dy;
	uchar *a;
	Display *d;

	if(!rectinrect(r, i->r)){
		werrstr("unloadimage: bad rectangle");
		return -1;
	}
	bpl = bytesperline(r, i->depth);
	if(ndata < bpl*Dy(r)){
		werrstr("unloadimage: buffer too small");
		return -1;
	}

	d = i->display;
	flushimage(d, 0);	/* make sure subsequent flush is for us only */
	ntot = 0;
	while(r.min.y < r.max.y){
		a = bufimage(d, 1+4+4*4);
		if(a == 0){
			werrstr("unloadimage: %r");
			return -1;
		}
		dy = 8000/bpl;
		if(dy <= 0){
			werrstr("unloadimage: image too wide");
			return -1;
		}
		if(dy > Dy(r))
			dy = Dy(r);
		a[0] = 'r';
		BPLONG(a+1, i->id);
		BPLONG(a+5, r.min.x);
		BPLONG(a+9, r.min.y);
		BPLONG(a+13, r.max.x);
		BPLONG(a+17, r.min.y+dy);
		if(flushimage(d, 0) < 0)
			return -1;
		n = _displayrddraw(d, data+ntot, ndata-ntot);
		if(n < 0)
			return n;
		ntot += n;
		r.min.y += dy;
	}
	return ntot;
}
