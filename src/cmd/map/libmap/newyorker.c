#include <u.h>
#include <libc.h>
#include "map.h"

static double a;

static int
Xnewyorker(struct place *place, double *x, double *y)
{
	double r = PI/2 - place->nlat.l;
	double s;
	if(r<.001)	/* cheat to plot center */
		s = 0;
	else if(r<a)
		return -1;
	else
		s = log(r/a);
	*x = -s * place->wlon.s;
	*y = -s * place->wlon.c;
	return(1);
}

proj
newyorker(double a0)
{
	a = a0*RAD;
	return(Xnewyorker);
}
