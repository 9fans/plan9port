#include <u.h>
#include <libc.h>
#include "map.h"

static struct coord stdpar0, stdpar1;
static double k;
static double yeq;

static int
Xtrapezoidal(struct place *place, double *x, double *y)
{
	*y = yeq + place->nlat.l;
	*x = *y*k*place->wlon.l;
	return 1;
}

proj
trapezoidal(double par0, double par1)
{
	if(fabs(fabs(par0)-fabs(par1))<.1)
		return rectangular(par0);
	deg2rad(par0,&stdpar0);
	deg2rad(par1,&stdpar1);
	if(fabs(par1-par0) < .1)
		k = stdpar1.s;
	else
		k = (stdpar1.c-stdpar0.c)/(stdpar0.l-stdpar1.l);
	yeq = -stdpar1.l - stdpar1.c/k;
	return Xtrapezoidal;
}
