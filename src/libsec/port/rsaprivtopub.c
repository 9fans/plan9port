#include "os.h"
#include <mp.h>
#include <libsec.h>

RSApub*
rsaprivtopub(RSApriv *priv)
{
	RSApub *pub;

	pub = rsapuballoc();
	if(pub == nil)
		return nil;
	pub->n = mpcopy(priv->pub.n);
	pub->ek = mpcopy(priv->pub.ek);
	return pub;
}
