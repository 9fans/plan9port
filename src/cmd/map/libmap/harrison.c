#include <u.h>
#include <libc.h>
#include "map.h"

static double v3,u2,u3,a,b; /*v=view,p=obj,u=unit.y*/

static int
Xharrison(struct place *place, double *x, double *y)
{
	double p1 = -place->nlat.c*place->wlon.s;
	double p2 = -place->nlat.c*place->wlon.c;
	double p3 = place->nlat.s;
	double d = b + u3*p2 - u2*p3;
	double t;
	if(d < .01)
		return -1;
	t = a/d;
	if(v3*place->nlat.s < 1.)
		return -1;
	*y = t*p2*u2 + (v3-t*(v3-p3))*u3;
	*x = t*p1;
	if(t < 0)
		return 0;
	if(*x * *x + *y * *y > 16)
		return -1;
	return 1;
}

proj
harrison(double r, double alpha)
{
	u2 = cos(alpha*RAD);
	u3 = sin(alpha*RAD);
	v3 = r;
	b = r*u2;
	a = 1 + b;
	if(r<1.001 || a<sqrt(r*r-1))
		return 0;
	return Xharrison;
}
