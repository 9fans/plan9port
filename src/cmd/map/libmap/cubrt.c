#include <u.h>
#include <libc.h>
#include "map.h"

double
cubrt(double a)
{
	double x,y,x1;
	if(a==0)
		return(0.);
	y = 1;
	if(a<0) {
		y = -y;
		a = -a;
	}
	while(a<1) {
		a *= 8;
		y /= 2;
	}
	while(a>1) {
		a /= 8;
		y *= 2;
	}
	x = 1;
	do {
		x1 = x;
		x = (2*x1+a/(x1*x1))/3;
	} while(fabs(x-x1)>10.e-15);
	return(x*y);
}
