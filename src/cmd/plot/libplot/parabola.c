#include "mplot.h"
void parabola(double x0, double y0, double x1, double y1, double xb, double yb){
	register double x, y, t;
	double	c0x, c0y, c1x, c1y;
	double	dt, d2, d1;
	d1 = sqrt((xb - x0) * (xb - x0) + (yb - y0) * (yb - y0));
	d2 = sqrt((xb - x1) * (xb - x1) + (yb - y1) * (yb - y1));
	if (d1 <= e1->quantum || d2 <= e1->quantum) {
		plotline(x0, y0, x1, y1);
		return;
	}
	c0x = x0 + x1 - 2. * xb;
	c1x = 2. * (xb - x0);
	c0y = y0 + y1 - 2. * yb;
	c1y = 2. * (yb - y0);
	move(x0, y0);
	dt = e1->quantum / d1;
	dt /= e1->grade;
	for (t = dt; t < 0.5; t += dt) {
		x = (c0x * t + c1x) * t + x0;
		y = (c0y * t + c1y) * t + y0;
		vec(x, y);
	}
	dt = e1->quantum / d2;
	dt /= e1->grade;
	for (; t < 1.0; t += dt) {
		x = (c0x * t + c1x) * t + x0;
		y = (c0y * t + c1y) * t + y0;
		vec(x, y);
	}
	vec(x1, y1);
}
