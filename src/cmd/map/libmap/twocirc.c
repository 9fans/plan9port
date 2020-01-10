#include <u.h>
#include <libc.h>
#include "map.h"

static double
quadratic(double a, double b, double c)
{
	double disc = b*b - 4*a*c;
	return disc<0? 0: (-b-sqrt(disc))/(2*a);
}

/* for projections with meridians being circles centered
on the x axis and parallels being circles centered on the y
axis.  Find the intersection of the meridian thru (m,0), (90,0),
with the parallel thru (0,p), (p1,p2) */

static int
twocircles(double m, double p, double p1, double p2, double *x, double *y)
{
	double a;	/* center of meridian circle, a>0 */
	double b;	/* center of parallel circle, b>0 */
	double t,bb;
	if(m > 0) {
		twocircles(-m,p,p1,p2,x,y);
		*x = -*x;
	} else if(p < 0) {
		twocircles(m,-p,p1,-p2,x,y);
		*y = -*y;
	} else if(p < .01) {
		*x = m;
		t = m/p1;
		*y = p + (p2-p)*t*t;
	} else if(m > -.01) {
		*y = p;
		*x = m - m*p*p;
	} else {
		b = p>=1? 1: p>.99? 0.5*(p+1 + p1*p1/(1-p)):
			0.5*(p*p-p1*p1-p2*p2)/(p-p2);
		a = .5*(m - 1/m);
		t = m*m-p*p+2*(b*p-a*m);
		bb = b*b;
		*x = quadratic(1+a*a/bb, -2*a + a*t/bb,
			t*t/(4*bb) - m*m + 2*a*m);
		*y = (*x*a+t/2)/b;
	}
	return 1;
}

static int
Xglobular(struct place *place, double *x, double *y)
{
	twocircles(-2*place->wlon.l/PI,
		2*place->nlat.l/PI, place->nlat.c, place->nlat.s, x, y);
	return 1;
}

proj
globular(void)
{
	return Xglobular;
}

static int
Xvandergrinten(struct place *place, double *x, double *y)
{
	double t = 2*place->nlat.l/PI;
	double abst = fabs(t);
	double pval = abst>=1? 1: abst/(1+sqrt(1-t*t));
	double p2 = 2*pval/(1+pval);
	twocircles(-place->wlon.l/PI, pval, sqrt(1-p2*p2), p2, x, y);
	if(t < 0)
		*y = -*y;
	return 1;
}

proj
vandergrinten(void)
{
	return Xvandergrinten;
}
