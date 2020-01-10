#include <u.h>
#include <libc.h>
#include "map.h"

/* elliptic integral routine, R.Bulirsch,
 *	Numerische Mathematik 7(1965) 78-90
 *	calculate integral from 0 to x+iy of
 *	(a+b*t^2)/((1+t^2)*sqrt((1+t^2)*(1+kc^2*t^2)))
 *	yields about D valid figures, where CC=10e-D
 *	for a*b>=0, except at branchpoints x=0,y=+-i,+-i/kc;
 *	there the accuracy may be reduced.
 *	fails for kc=0 or x<0
 *	return(1) for success, return(0) for fail
 *
 *	special case a=b=1 is equivalent to
 *	standard elliptic integral of first kind
 *	from 0 to atan(x+iy) of
 *	1/sqrt(1-k^2*(sin(t))^2) where k^2=1-kc^2
*/

#define ROOTINF 10.e18
#define CC 1.e-6

int
elco2(double x, double y, double kc, double a, double b, double *u, double *v)
{
	double c,d,dn1,dn2,e,e1,e2,f,f1,f2,h,k,m,m1,m2,sy;
	double d1[13],d2[13];
	int i,l;
	if(kc==0||x<0)
		return(0);
	sy = y>0? 1: y==0? 0: -1;
	y = fabs(y);
	csq(x,y,&c,&e2);
	d = kc*kc;
	k = 1-d;
	e1 = 1+c;
	cdiv2(1+d*c,d*e2,e1,e2,&f1,&f2);
	f2 = -k*x*y*2/f2;
	csqr(f1,f2,&dn1,&dn2);
	if(f1<0) {
		f1 = dn1;
		dn1 = -dn2;
		dn2 = -f1;
	}
	if(k<0) {
		dn1 = fabs(dn1);
		dn2 = fabs(dn2);
	}
	c = 1+dn1;
	cmul(e1,e2,c,dn2,&f1,&f2);
	cdiv(x,y,f1,f2,&d1[0],&d2[0]);
	h = a-b;
	d = f = m = 1;
	kc = fabs(kc);
	e = a;
	a += b;
	l = 4;
	for(i=1;;i++) {
		m1 = (kc+m)/2;
		m2 = m1*m1;
		k *= f/(m2*4);
		b += e*kc;
		e = a;
		cdiv2(kc+m*dn1,m*dn2,c,dn2,&f1,&f2);
		csqr(f1/m1,k*dn2*2/f2,&dn1,&dn2);
		cmul(dn1,dn2,x,y,&f1,&f2);
		x = fabs(f1);
		y = fabs(f2);
		a += b/m1;
		l *= 2;
		c = 1 +dn1;
		d *= k/2;
		cmul(x,y,x,y,&e1,&e2);
		k *= k;

		cmul(c,dn2,1+e1*m2,e2*m2,&f1,&f2);
		cdiv(d*x,d*y,f1,f2,&d1[i],&d2[i]);
		if(k<=CC)
			break;
		kc = sqrt(m*kc);
		f = m2;
		m = m1;
	}
	f1 = f2 = 0;
	for(;i>=0;i--) {
		f1 += d1[i];
		f2 += d2[i];
	}
	x *= m1;
	y *= m1;
	cdiv2(1-y,x,1+y,-x,&e1,&e2);
	e2 = x*2/e2;
	d = a/(m1*l);
	*u = atan2(e2,e1);
	if(*u<0)
		*u += PI;
	a = d*sy/2;
	*u = d*(*u) + f1*h;
	*v = (-1-log(e1*e1+e2*e2))*a + f2*h*sy + a;
	return(1);
}

void
cdiv2(double c1, double c2, double d1, double d2, double *e1, double *e2)
{
	double t;
	if(fabs(d2)>fabs(d1)) {
		t = d1, d1 = d2, d2 = t;
		t = c1, c1 = c2, c2 = t;
	}
	if(fabs(d1)>ROOTINF)
		*e2 = ROOTINF*ROOTINF;
	else
		*e2 = d1*d1 + d2*d2;
	t = d2/d1;
	*e1 = (c1+t*c2)/(d1+t*d2); /* (c1*d1+c2*d2)/(d1*d1+d2*d2) */
}

/* complex square root of |x|+iy */
void
csqr(double c1, double c2, double *e1, double *e2)
{
	double r2;
	r2 = c1*c1 + c2*c2;
	if(r2<=0) {
		*e1 = *e2 = 0;
		return;
	}
	*e1 = sqrt((sqrt(r2) + fabs(c1))/2);
	*e2 = c2/(*e1*2);
}
