#include "os.h"
#include <mp.h>
#include <libsec.h>

EGpub*
egpuballoc(void)
{
	EGpub *eg;

	eg = mallocz(sizeof(*eg), 1);
	if(eg == nil)
		sysfatal("egpuballoc");
	return eg;
}

void
egpubfree(EGpub *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->p);
	mpfree(eg->alpha);
	mpfree(eg->key);
	free(eg);
}


EGpriv*
egprivalloc(void)
{
	EGpriv *eg;

	eg = mallocz(sizeof(*eg), 1);
	if(eg == nil)
		sysfatal("egprivalloc");
	return eg;
}

void
egprivfree(EGpriv *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->pub.p);
	mpfree(eg->pub.alpha);
	mpfree(eg->pub.key);
	mpfree(eg->secret);
	free(eg);
}

EGsig*
egsigalloc(void)
{
	EGsig *eg;

	eg = mallocz(sizeof(*eg), 1);
	if(eg == nil)
		sysfatal("egsigalloc");
	return eg;
}

void
egsigfree(EGsig *eg)
{
	if(eg == nil)
		return;
	mpfree(eg->r);
	mpfree(eg->s);
	free(eg);
}
