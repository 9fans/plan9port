#include <u.h>
#include <libc.h>
#include "map.h"

int
Xsinusoidal(struct place *place, double *x, double *y)
{
	*x = - place->wlon.l * place->nlat.c;
	*y = place->nlat.l;
	return(1);
}

proj
sinusoidal(void)
{
	return(Xsinusoidal);
}
