#include <u.h>
#include <libc.h>
#include "map.h"

/*complex divide, defensive against overflow from
 *	* and /, but not from + and -
 *	assumes underflow yields 0.0
 *	uses identities:
 *	(a + bi)/(c + di) = ((a + bd/c) + (b - ad/c)i)/(c + dd/c)
 *	(a + bi)/(c + di) = (b - ai)/(d - ci)
*/
void
cdiv(double a, double b, double c, double d, double *u, double *v)
{
	double r,t;
	if(fabs(c)<fabs(d)) {
		t = -c; c = d; d = t;
		t = -a; a = b; b = t;
	}
	r = d/c;
	t = c + r*d;
	*u = (a + r*b)/t;
	*v = (b - r*a)/t;
}

void
cmul(double c1, double c2, double d1, double d2, double *e1, double *e2)
{
	*e1 = c1*d1 - c2*d2;
	*e2 = c1*d2 + c2*d1;
}

void
csq(double c1, double c2, double *e1, double *e2)
{
	*e1 = c1*c1 - c2*c2;
	*e2 = c1*c2*2;
}

/* complex square root
 *	assumes underflow yields 0.0
 *	uses these identities:
 *	sqrt(x+_iy) = sqrt(r(cos(t)+_isin(t))
 *	           = sqrt(r)(cos(t/2)+_isin(t/2))
 *	cos(t/2) = sin(t)/2sin(t/2) = sqrt((1+cos(t)/2)
 *	sin(t/2) = sin(t)/2cos(t/2) = sqrt((1-cos(t)/2)
*/
void
csqrt(double c1, double c2, double *e1, double *e2)
{
	double r,s;
	double x,y;
	x = fabs(c1);
	y = fabs(c2);
	if(x>=y) {
		if(x==0) {
			*e1 = *e2 = 0;
			return;
		}
		r = x;
		s = y/x;
	} else {
		r = y;
		s = x/y;
	}
	r *= sqrt(1+ s*s);
	if(c1>0) {
		*e1 = sqrt((r+c1)/2);
		*e2 = c2/(2* *e1);
	} else {
		*e2 = sqrt((r-c1)/2);
		if(c2<0)
			*e2 = -*e2;
		*e1 = c2/(2* *e2);
	}
}


void cpow(double c1, double c2, double *d1, double *d2, double pwr)
{
	double theta = pwr*atan2(c2,c1);
	double r = pow(hypot(c1,c2), pwr);
	*d1 = r*cos(theta);
	*d2 = r*sin(theta);
}
