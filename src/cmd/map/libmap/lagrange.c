#include <u.h>
#include <libc.h>
#include "map.h"

static int
Xlagrange(struct place *place, double *x, double *y)
{
	double z1,z2;
	double w1,w2,t1,t2;
	struct place p;
	copyplace(place,&p);
	if(place->nlat.l<0) {
		p.nlat.l = -p.nlat.l;
		p.nlat.s = -p.nlat.s;
	}
	Xstereographic(&p,&z1,&z2);
	csqrt(-z2/2,z1/2,&w1,&w2);
	cdiv(w1-1,w2,w1+1,w2,&t1,&t2);
	*y = -t1;
	*x = t2;
	if(place->nlat.l<0)
		*y = -*y;
	return(1);
}

proj
lagrange(void)
{
	return(Xlagrange);
}
