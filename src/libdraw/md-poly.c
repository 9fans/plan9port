#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

void
mempoly(Memimage *dst, Point *vert, int nvert, int end0, int end1, int radius, Memimage *src, Point sp, int op)
{
	int i, e0, e1;
	Point d;

	if(nvert < 2)
		return;
	d = subpt(sp, vert[0]);
	for(i=1; i<nvert; i++){
		e0 = e1 = Enddisc;
		if(i == 1)
			e0 = end0;
		if(i == nvert-1)
			e1 = end1;
		memline(dst, vert[i-1], vert[i], e0, e1, radius, src, addpt(d, vert[i-1]), op);
	}
}
