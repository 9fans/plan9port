/*
	sqrt returns the square root of its floating
	point argument. Newton's method.

	calls frexp
*/

#include <u.h>
#include <libc.h>

double
sqrt(double arg)
{
	double x, temp;
	int exp, i;

	if(arg <= 0) {
		if(arg < 0)
			return 0.;
		return 0;
	}
	x = frexp(arg, &exp);
	while(x < 0.5) {
		x *= 2;
		exp--;
	}
	/*
	 * NOTE
	 * this wont work on 1's comp
	 */
	if(exp & 1) {
		x *= 2;
		exp--;
	}
	temp = 0.5 * (1.0+x);

	while(exp > 60) {
		temp *= (1L<<30);
		exp -= 60;
	}
	while(exp < -60) {
		temp /= (1L<<30);
		exp += 60;
	}
	if(exp >= 0)
		temp *= 1L << (exp/2);
	else
		temp /= 1L << (-exp/2);
	for(i=0; i<=4; i++)
		temp = 0.5*(temp + arg/temp);
	return temp;
}
