#include "os.h"
#include <mp.h>
#include <libsec.h>

/* decrypt rsa using garner's algorithm for the chinese remainder theorem */
/*	seminumerical algorithms, knuth, pp 253-254 */
/*	applied cryptography, menezes et al, pg 612 */
mpint*
rsadecrypt(RSApriv *rsa, mpint *in, mpint *out)
{
	mpint *v1, *v2;

	if(out == nil)
		out = mpnew(0);

	/* convert in to modular representation */
	v1 = mpnew(0);
	mpmod(in, rsa->p, v1);
	v2 = mpnew(0);
	mpmod(in, rsa->q, v2);

	/* exponentiate the modular rep */
	mpexp(v1, rsa->kp, rsa->p, v1);
	mpexp(v2, rsa->kq, rsa->q, v2);

	/* out = v1 + p*((v2-v1)*c2 mod q) */
	mpsub(v2, v1, v2);
	mpmul(v2, rsa->c2, v2);
	mpmod(v2, rsa->q, v2);
	mpmul(v2, rsa->p, out);
	mpadd(v1, out, out);

	mpfree(v1);
	mpfree(v2);

	return out;
}
