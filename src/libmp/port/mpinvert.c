#include "os.h"
#include <mp.h>

#define iseven(a)	(((a)->p[0] & 1) == 0)

// use extended gcd to find the multiplicative inverse
// res = b**-1 mod m
void
mpinvert(mpint *b, mpint *m, mpint *res)
{
	mpint *dc1, *dc2;	// don't care

	dc1 = mpnew(0);
	dc2 = mpnew(0);
	mpextendedgcd(b, m, dc1, res, dc2);
	if(mpcmp(dc1, mpone) != 0)
		abort();
	mpmod(res, m, res);
	mpfree(dc1);
	mpfree(dc2);
}
