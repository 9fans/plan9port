#include <u.h>
#include <libc.h>
#include "map.h"
/* refractive fisheye, not logarithmic */

static double n;

static int
Xfisheye(struct place *place, double *x, double *y)
{
	double r;
	double u = sin(PI/4-place->nlat.l/2)/n;
	if(fabs(u) > .97)
		return -1;
	r = tan(asin(u));
	*x = -r*place->wlon.s;
	*y = -r*place->wlon.c;
	return 1;
}

proj
fisheye(double par)
{
	n = par;
	return n<.1? 0: Xfisheye;
}
