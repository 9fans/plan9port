#include "os.h"
#include <mp.h>
#include <libsec.h>

RSApriv*
rsafill(mpint *n, mpint *e, mpint *d, mpint *p, mpint *q)
{
	mpint *c2, *kq, *kp, *x;
	RSApriv *rsa;

	// make sure we're not being hoodwinked
	if(!probably_prime(p, 10) || !probably_prime(q, 10)){
		werrstr("rsafill: p or q not prime");
		return nil;
	}
	x = mpnew(0);
	mpmul(p, q, x);
	if(mpcmp(n, x) != 0){
		werrstr("rsafill: n != p*q");
		mpfree(x);
		return nil;
	}
	c2 = mpnew(0);
	mpsub(p, mpone, c2);
	mpsub(q, mpone, x);
	mpmul(c2, x, x);
	mpmul(e, d, c2);
	mpmod(c2, x, x);
	if(mpcmp(x, mpone) != 0){
		werrstr("rsafill: e*d != 1 mod (p-1)*(q-1)");
		mpfree(x);
		mpfree(c2);
		return nil;
	}

	// compute chinese remainder coefficient
	mpinvert(p, q, c2);

	// for crt a**k mod p == (a**(k mod p-1)) mod p
	kq = mpnew(0);
	kp = mpnew(0);
	mpsub(p, mpone, x);
	mpmod(d, x, kp);
	mpsub(q, mpone, x);
	mpmod(d, x, kq);

	rsa = rsaprivalloc();
	rsa->pub.ek = mpcopy(e);
	rsa->pub.n = mpcopy(n);
	rsa->dk = mpcopy(d);
	rsa->kp = kp;
	rsa->kq = kq;
	rsa->p = mpcopy(p);
	rsa->q = mpcopy(q);
	rsa->c2 = c2;

	mpfree(x);

	return rsa;
}

