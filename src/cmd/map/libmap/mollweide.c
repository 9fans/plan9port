#include <u.h>
#include <libc.h>
#include "map.h"

static int
Xmollweide(struct place *place, double *x, double *y)
{
	double z;
	double w;
	z = place->nlat.l;
	if(fabs(z)<89.9*RAD)
		do {	/*newton for 2z+sin2z=pi*sin(lat)*/
			w = (2*z+sin(2*z)-PI*place->nlat.s)/(2+2*cos(2*z));
			z -= w;
		} while(fabs(w)>=.00001);
	*y = sin(z);
	*x = - (2/PI)*cos(z)*place->wlon.l;
	return(1);
}

proj
mollweide(void)
{
	return(Xmollweide);
}
