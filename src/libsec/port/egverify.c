#include "os.h"
#include <mp.h>
#include <libsec.h>

int
egverify(EGpub *pub, EGsig *sig, mpint *m)
{
	mpint *p = pub->p, *alpha = pub->alpha;
	mpint *r = sig->r, *s = sig->s;
	mpint *v1, *v2, *rs;
	int rv = -1;

	if(mpcmp(r, mpone) < 0 || mpcmp(r, p) >= 0)
		return rv;
	v1 = mpnew(0);
	rs = mpnew(0);
	v2 = mpnew(0);
	mpexp(pub->key, r, p, v1);
	mpexp(r, s, p, rs);
	mpmul(v1, rs, v1);
	mpmod(v1, p, v1);
	mpexp(alpha, m, p, v2);
	if(mpcmp(v1, v2) == 0)
		rv = 0;
	mpfree(v1);
	mpfree(rs);
	mpfree(v2);
	return rv;
}
