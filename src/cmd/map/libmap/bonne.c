#include <u.h>
#include <libc.h>
#include "map.h"

static struct coord stdpar;
static double r0;

static int
Xbonne(struct place *place, double *x, double *y)
{
	double r, alpha;
	r = r0 - place->nlat.l;
	if(r<.001)
		if(fabs(stdpar.c)<1e-10)
			alpha = place->wlon.l;
		else if(fabs(place->nlat.c)==0)
			alpha = 0;
		else
			alpha = place->wlon.l/(1+
				stdpar.c*stdpar.c*stdpar.c/place->nlat.c/3);
	else
		alpha = place->wlon.l * place->nlat.c / r;
	*x = - r*sin(alpha);
	*y = - r*cos(alpha);
	return(1);
}

proj
bonne(double par)
{
	if(fabs(par*RAD) < .01)
		return(Xsinusoidal);
	deg2rad(par, &stdpar);
	r0 = stdpar.c/stdpar.s + stdpar.l;
	return(Xbonne);
}
