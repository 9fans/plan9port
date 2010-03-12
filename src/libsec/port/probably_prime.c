#include "os.h"
#include <mp.h>
#include <libsec.h>

/* Miller-Rabin probabilistic primality testing */
/*	Knuth (1981) Seminumerical Algorithms, p.379 */
/*	Menezes et al () Handbook, p.39 */
/* 0 if composite; 1 if almost surely prime, Pr(err)<1/4**nrep */
int
probably_prime(mpint *n, int nrep)
{
	int j, k, rep, nbits, isprime;
	mpint *nm1, *q, *x, *y, *r;

	if(n->sign < 0)
		sysfatal("negative prime candidate");

	if(nrep <= 0)
		nrep = 18;

	k = mptoi(n);
	if(k == 2)		/* 2 is prime */
		return 1;
	if(k < 2)		/* 1 is not prime */
		return 0;
	if((n->p[0] & 1) == 0)	/* even is not prime */
		return 0;

	/* test against small prime numbers */
	if(smallprimetest(n) < 0)
		return 0;

	/* fermat test, 2^n mod n == 2 if p is prime */
	x = uitomp(2, nil);
	y = mpnew(0);
	mpexp(x, n, n, y);
	k = mptoi(y);
	if(k != 2){
		mpfree(x);
		mpfree(y);
		return 0;
	}

	nbits = mpsignif(n);
	nm1 = mpnew(nbits);
	mpsub(n, mpone, nm1);	/* nm1 = n - 1 */
	k = mplowbits0(nm1);
	q = mpnew(0);
	mpright(nm1, k, q);	/* q = (n-1)/2**k */

	for(rep = 0; rep < nrep; rep++){
		for(;;){
			/* find x = random in [2, n-2] */
			r = mprand(nbits, prng, nil);
			mpmod(r, nm1, x);
			mpfree(r);
			if(mpcmp(x, mpone) > 0)
				break;
		}

		/* y = x**q mod n */
		mpexp(x, q, n, y);

		if(mpcmp(y, mpone) == 0 || mpcmp(y, nm1) == 0)
			continue;

		for(j = 1;; j++){
			if(j >= k) {
				isprime = 0;
				goto done;
			}
			mpmul(y, y, x);
			mpmod(x, n, y);	/* y = y*y mod n */
			if(mpcmp(y, nm1) == 0)
				break;
			if(mpcmp(y, mpone) == 0){
				isprime = 0;
				goto done;
			}
		}
	}
	isprime = 1;
done:
	mpfree(y);
	mpfree(x);
	mpfree(q);
	mpfree(nm1);
	return isprime;
}
