#include "os.h"
#include <mp.h>
#include <libsec.h>

// Gordon's algorithm for generating a strong prime
//	Menezes et al () Handbook, p.150
void
genstrongprime(mpint *p, int n, int accuracy)
{
	mpint *s, *t, *r, *i;

	if(n < 64)
		n = 64;

	s = mpnew(n/2);
	genprime(s, (n/2)-16, accuracy);
	t = mpnew(n/2);
	genprime(t, n-mpsignif(s)-32, accuracy);

	// first r = 2it + 1 that's prime
	i = mpnew(16);
	r = mpnew(0);
	itomp(0x8000, i);
	mpleft(t, 1, t);	// 2t
	mpmul(i, t, r);		// 2it
	mpadd(r, mpone, r);	// 2it + 1
	for(;;){
		if(probably_prime(r, 18))
			break;
		mpadd(r, t, r);	// r += 2t
	}

	// p0 = 2(s**(r-2) mod r)s - 1
	itomp(2, p);
	mpsub(r, p, p);
	mpexp(s, p, r, p);
	mpmul(s, p, p);
	mpleft(p, 1, p);
	mpsub(p, mpone, p);

	// first p = p0 + 2irs that's prime
	itomp(0x8000, i);
	mpleft(r, 1, r);	// 2r
	mpmul(r, s, r);		// 2rs
	mpmul(r, i, i);		// 2irs
	mpadd(p, i, p);		// p0 + 2irs
	for(;;){
		if(probably_prime(p, accuracy))
			break;
		mpadd(p, r, p); // p += 2rs
	}

	mpfree(i);
	mpfree(s);
	mpfree(r);
	mpfree(t);
}
