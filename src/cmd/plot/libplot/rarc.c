#include "mplot.h"
/*		arc plotting routine		*/
/*		from x1,y1 to x2,y2		*/
/*	with center xc,yc and radius rr	*/
/*	integrates difference equation		*/
/*	negative rr draws counterclockwise	*/
#define PI4 0.7854
void rarc(double x1, double y1, double x2, double y2, double xc, double yc, double rr){
	register double dx, dy, a, b;
	double	ph, dph, rd, xnext;
	register int	n;
	dx = x1 - xc;
	dy = y1 - yc;
	rd = sqrt(dx * dx + dy * dy);
	if (rd / e1->quantum < 1.0) {
		move(xc, yc);
		vec(xc, yc);
		return;
	}
	dph = acos(1.0 - (e1->quantum / rd));
	if (dph > PI4)
		dph = PI4;
	ph=atan2((y2-yc),(x2 - xc)) - atan2(dy, dx);
	if (ph < 0)
		ph += 6.2832;
	if (rr < 0)
		ph = 6.2832 - ph;
	if (ph < dph)
		plotline(x1, y1, x2, y2);
	else {
		n = ph / dph;
		a = cos(dph);
		b = sin(dph);
		if (rr < 0)
			b = -b;
		move(x1, y1);
		while ((n--) >= 0) {
			xnext = dx * a - dy * b;
			dy = dx * b + dy * a;
			dx = xnext;
			vec(dx + xc, dy + yc);
		}
	}
}
