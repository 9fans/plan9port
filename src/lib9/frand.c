#include	<u.h>
#include	<libc.h>

#define	MASK	0x7fffffffL
#define	NORM	(1.0/(1.0+MASK))

double
p9frand(void)
{
	double x;

	do {
		x = lrand() * NORM;
		x = (x + lrand()) * NORM;
	} while(x >= 1);
	return x;
}
