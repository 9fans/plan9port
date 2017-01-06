#include <u.h>
#include <libc.h>
#include "map.h"

#define ORTHRAD 1000
static double viewpt;

static int
Xperspective(struct place *place, double *x, double *y)
{
	double r;
	if(viewpt<=1+FUZZ && fabs(place->nlat.s)<=viewpt+.01)
		return(-1);
	r = place->nlat.c*(viewpt - 1.)/(viewpt - place->nlat.s);
	*x = - r*place->wlon.s;
	*y = - r*place->wlon.c;
	if(r>4.)
		return(-1);
	if(fabs(viewpt)>1 && place->nlat.s<1/viewpt ||
	   fabs(viewpt)<=1 && place->nlat.s<viewpt)
			return 0;
	return(1);
}

proj
perspective(double radius)
{
	viewpt = radius;
	if(viewpt >= ORTHRAD)
		return(Xorthographic);
	if(fabs(viewpt-1.)<.0001)
		return(0);
	return(Xperspective);
}

	/* called from various conformal projections,
           but not from stereographic itself */
int
Xstereographic(struct place *place, double *x, double *y)
{
	double v = viewpt;
	int retval;
	viewpt = -1;
	retval = Xperspective(place, x, y);
	viewpt = v;
	return retval;
}

proj
stereographic(void)
{
	viewpt = -1.;
	return(Xperspective);
}

proj
gnomonic(void)
{
	viewpt = 0.;
	return(Xperspective);
}

int
plimb(double *lat, double *lon, double res)
{
	static int first = 1;
	if(viewpt >= ORTHRAD)
		return olimb(lat, lon, res);
	if(first) {
		first = 0;
		*lon = -180;
		if(fabs(viewpt) < .01)
			*lat = 0;
		else if(fabs(viewpt)<=1)
			*lat = asin(viewpt)/RAD;
		else
			*lat = asin(1/viewpt)/RAD;
	} else
		*lon += res;
	if(*lon <= 180)
		return 1;
	first = 1;
	return -1;
}
