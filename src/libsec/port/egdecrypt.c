#include "os.h"
#include <mp.h>
#include <libsec.h>

mpint*
egdecrypt(EGpriv *priv, mpint *in, mpint *out)
{
	EGpub *pub = &priv->pub;
	mpint *gamma, *delta;
	mpint *p = pub->p;
	int plen = mpsignif(p)+1;
	int shift = ((plen+Dbits-1)/Dbits)*Dbits;

	if(out == nil)
		out = mpnew(0);
	gamma = mpnew(0);
	delta = mpnew(0);
	mpright(in, shift, gamma);
	mpleft(gamma, shift, delta);
	mpsub(in, delta, delta);	
	mpexp(gamma, priv->secret, p, out);
	mpinvert(out, p, gamma);
	mpmul(gamma, delta, out);
	mpmod(out, p, out);
	mpfree(gamma);
	mpfree(delta);
	return out;
}
