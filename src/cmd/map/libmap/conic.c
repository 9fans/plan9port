#include <u.h>
#include <libc.h>
#include "map.h"

static struct coord stdpar;

static int
Xconic(struct place *place, double *x, double *y)
{
	double r;
	if(fabs(place->nlat.l-stdpar.l) > 80.*RAD)
		return(-1);
	r = stdpar.c/stdpar.s - tan(place->nlat.l - stdpar.l);
	*x = - r*sin(place->wlon.l * stdpar.s);
	*y = - r*cos(place->wlon.l * stdpar.s);
	if(r>3) return(0);
	return(1);
}

proj
conic(double par)
{
	if(fabs(par) <.1)
		return(Xcylindrical);
	deg2rad(par, &stdpar);
	return(Xconic);
}
