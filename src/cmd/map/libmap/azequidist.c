#include <u.h>
#include <libc.h>
#include "map.h"

int
Xazequidistant(struct place *place, double *x, double *y)
{
	double colat;
	colat = PI/2 - place->nlat.l;
	*x = -colat * place->wlon.s;
	*y = -colat * place->wlon.c;
	return(1);
}

proj
azequidistant(void)
{
	return(Xazequidistant);
}
