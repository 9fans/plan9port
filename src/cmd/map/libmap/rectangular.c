#include <u.h>
#include <libc.h>
#include "map.h"

static double scale;

static int
Xrectangular(struct place *place, double *x, double *y)
{
	*x = -scale*place->wlon.l;
	*y = place->nlat.l;
	return(1);
}

proj
rectangular(double par)
{
	scale = cos(par*RAD);
	if(scale<.1)
		return 0;
	return(Xrectangular);
}
