#include <u.h>
#include <libc.h>
#include "map.h"


int
Xorthographic(struct place *place, double *x, double *y)
{
	*x = - place->nlat.c * place->wlon.s;
	*y = - place->nlat.c * place->wlon.c;
	return(place->nlat.l<0.? 0 : 1);
}

proj
orthographic(void)
{
	return(Xorthographic);
}

int
olimb(double *lat, double *lon, double res)
{
	static int first  = 1;
	if(first) {
		*lat = 0;
		*lon = -180;
		first = 0;
		return 0;
	}
	*lon += res;
	if(*lon <= 180)
		return 1;
	first = 1;
	return -1;
}
