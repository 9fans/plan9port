#include <u.h>
#include <libc.h>
#include "map.h"

int
Xgilbert(struct place *p, double *x, double *y)
{
/* the interesting part - map the sphere onto a hemisphere */
	struct place q;
	q.nlat.s = tan(0.5*(p->nlat.l));
	if(q.nlat.s > 1) q.nlat.s = 1;
	if(q.nlat.s < -1) q.nlat.s = -1;
	q.nlat.c = sqrt(1 - q.nlat.s*q.nlat.s);
	q.wlon.l = p->wlon.l/2;
	sincos(&q.wlon);
/* the dull part: present the hemisphere orthogrpahically */
	*y = q.nlat.s;
	*x = -q.wlon.s*q.nlat.c;
	return(1);
}

proj
gilbert(void)
{
	return(Xgilbert);
}

/* derivation of the interesting part:
   map the sphere onto the plane by stereographic projection;
   map the plane onto a half plane by sqrt;
   map the half plane back to the sphere by stereographic
   projection

   n,w are original lat and lon
   r is stereographic radius
   primes are transformed versions

   r = cos(n)/(1+sin(n))
   r' = sqrt(r) = cos(n')/(1+sin(n'))

   r'^2 = (1-sin(n')^2)/(1+sin(n')^2) = cos(n)/(1+sin(n))

   this is a linear equation for sin n', with solution

   sin n' = (1+sin(n)-cos(n))/(1+sin(n)+cos(n))

   use standard formula: tan x/2 = (1-cos x)/sin x = sin x/(1+cos x)
   to show that the right side of the last equation is tan(n/2)
*/
