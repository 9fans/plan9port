#include <u.h>
#include <libc.h>
#include "map.h"

static double a;

static int
Xcylequalarea(struct place *place, double *x, double *y)
{
	*x = - place->wlon.l * a;
	*y = place->nlat.s;
	return(1);
}

proj
cylequalarea(double par)
{
	struct coord stdp0;
	if(par > 89.0)
		return(0);
	deg2rad(par, &stdp0);
	a = stdp0.c*stdp0.c;
	return(Xcylequalarea);
}
