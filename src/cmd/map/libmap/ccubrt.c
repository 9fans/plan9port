#include <u.h>
#include <libc.h>
#include "map.h"

void
ccubrt(double zr, double zi, double *wr, double *wi)
{
	double r, theta;
	theta = atan2(zi,zr);
	r = cubrt(hypot(zr,zi));
	*wr = r*cos(theta/3);
	*wi = r*sin(theta/3);
}
