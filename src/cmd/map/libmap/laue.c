#include <u.h>
#include <libc.h>
#include "map.h"


static int
Xlaue(struct place *place, double *x, double *y)
{
	double r;
	if(place->nlat.l<PI/4+FUZZ)
		return(-1);
	r = tan(PI-2*place->nlat.l);
	if(r>3)
		return(-1);
	*x = - r * place->wlon.s;
	*y = - r * place->wlon.c;
	return(1);
}

proj
laue(void)
{
	return(Xlaue);
}
