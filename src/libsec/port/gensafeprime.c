#include "os.h"
#include <mp.h>
#include <libsec.h>

// find a prime p of length n and a generator alpha of Z^*_p
// Alg 4.86 Menezes et al () Handbook, p.164
void
gensafeprime(mpint *p, mpint *alpha, int n, int accuracy)
{
	mpint *q, *b;

	q = mpnew(n-1);
	while(1){
		genprime(q, n-1, accuracy);
		mpleft(q, 1, p);
		mpadd(p, mpone, p); // p = 2*q+1
		if(probably_prime(p, accuracy))
			break;
	}
	// now find a generator alpha of the multiplicative
	// group Z*_p of order p-1=2q
	b = mpnew(0);
	while(1){
		mprand(n, genrandom, alpha);
		mpmod(alpha, p, alpha);
		mpmul(alpha, alpha, b);
		mpmod(b, p, b);
		if(mpcmp(b, mpone) == 0)
			continue;
		mpexp(alpha, q, p, b);
		if(mpcmp(b, mpone) != 0)
			break;
	}
	mpfree(b);
	mpfree(q);
}
