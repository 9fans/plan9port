#include "os.h"
#include <mp.h>
#include <libsec.h>

DSApub*
dsaprivtopub(DSApriv *priv)
{
	DSApub *pub;

	pub = dsapuballoc();
	pub->p = mpcopy(priv->pub.p);
	pub->q = mpcopy(priv->pub.q);
	pub->alpha = mpcopy(priv->pub.alpha);
	pub->key = mpcopy(priv->pub.key);
	return pub;
}
