#include "os.h"
#include <mp.h>
#include <libsec.h>

EGpriv*
eggen(int nlen, int rounds)
{
	EGpub *pub;
	EGpriv *priv;

	priv = egprivalloc();
	pub = &priv->pub;
	pub->p = mpnew(0);
	pub->alpha = mpnew(0);
	pub->key = mpnew(0);
	priv->secret = mpnew(0);
	gensafeprime(pub->p, pub->alpha, nlen, rounds);
	mprand(nlen-1, genrandom, priv->secret);
	mpexp(pub->alpha, priv->secret, pub->p, pub->key);
	return priv;
}
