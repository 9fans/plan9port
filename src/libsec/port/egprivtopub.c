#include "os.h"
#include <mp.h>
#include <libsec.h>

EGpub*
egprivtopub(EGpriv *priv)
{
	EGpub *pub;

	pub = egpuballoc();
	if(pub == nil)
		return nil;
	pub->p = mpcopy(priv->pub.p);
	pub->alpha = mpcopy(priv->pub.alpha);
	pub->key = mpcopy(priv->pub.key);
	return pub;
}
