#include "os.h"
#include <mp.h>
#include <libsec.h>

DSApriv*
dsagen(DSApub *opub)
{
	DSApub *pub;
	DSApriv *priv;
	mpint *exp;
	mpint *g;
	mpint *r;
	int bits;

	priv = dsaprivalloc();
	pub = &priv->pub;

	if(opub != nil){
		pub->p = mpcopy(opub->p);
		pub->q = mpcopy(opub->q);
	} else {
		pub->p = mpnew(0);
		pub->q = mpnew(0);
		DSAprimes(pub->q, pub->p, nil);
	}
	bits = Dbits*pub->p->top;

	pub->alpha = mpnew(0);
	pub->key = mpnew(0);
	priv->secret = mpnew(0);

	// find a generator alpha of the multiplicative
	// group Z*p, i.e., of order n = p-1.  We use the
	// fact that q divides p-1 to reduce the exponent.
	exp = mpnew(0);
	g = mpnew(0);
	r = mpnew(0);
	mpsub(pub->p, mpone, exp);
	mpdiv(exp, pub->q, exp, r);
	if(mpcmp(r, mpzero) != 0)
		sysfatal("dsagen foul up");
	while(1){
		mprand(bits, genrandom, g);
		mpmod(g, pub->p, g);
		mpexp(g, exp, pub->p, pub->alpha);
		if(mpcmp(pub->alpha, mpone) != 0)
			break;
	}
	mpfree(g);
	mpfree(exp);

	// create the secret key
	mprand(bits, genrandom, priv->secret);
	mpmod(priv->secret, pub->p, priv->secret);
	mpexp(pub->alpha, priv->secret, pub->p, pub->key);

	return priv;
}
