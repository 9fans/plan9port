#include "os.h"
#include <mp.h>
#include <libsec.h>

mpint*
egencrypt(EGpub *pub, mpint *in, mpint *out)
{
	mpint *m, *k, *gamma, *delta, *pm1;
	mpint *p = pub->p, *alpha = pub->alpha;
	int plen = mpsignif(p);
	int shift = ((plen+Dbits)/Dbits)*Dbits;
	// in libcrypt version, (int)(LENGTH(pub->p)*sizeof(NumType)*CHARBITS);

	if(out == nil)
		out = mpnew(0);
	pm1 = mpnew(0);
	m = mpnew(0);
	gamma = mpnew(0);
	delta = mpnew(0);
	mpmod(in, p, m);
	while(1){
		k = mprand(plen, genrandom, nil);
		if((mpcmp(mpone, k) <= 0) && (mpcmp(k, pm1) < 0))
			break;
	}
	mpexp(alpha, k, p, gamma);
	mpexp(pub->key, k, p, delta);
	mpmul(m, delta, delta);
	mpmod(delta, p, delta);
	mpleft(gamma, shift, out);
	mpadd(delta, out, out);
	mpfree(pm1);
	mpfree(m);
	mpfree(k);
	mpfree(gamma);
	mpfree(delta);
	return out;
}
