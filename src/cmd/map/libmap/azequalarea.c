#include <u.h>
#include <libc.h>
#include "map.h"

int
Xazequalarea(struct place *place, double *x, double *y)
{
	double r;
	r = sqrt(1. - place->nlat.s);
	*x = - r * place->wlon.s;
	*y = - r * place->wlon.c;
	return(1);
}

proj
azequalarea(void)
{
	return(Xazequalarea);
}
