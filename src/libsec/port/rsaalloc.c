#include "os.h"
#include <mp.h>
#include <libsec.h>

RSApub*
rsapuballoc(void)
{
	RSApub *rsa;

	rsa = mallocz(sizeof(*rsa), 1);
	if(rsa == nil)
		sysfatal("rsapuballoc");
	return rsa;
}

void
rsapubfree(RSApub *rsa)
{
	if(rsa == nil)
		return;
	mpfree(rsa->ek);
	mpfree(rsa->n);
	free(rsa);
}


RSApriv*
rsaprivalloc(void)
{
	RSApriv *rsa;

	rsa = mallocz(sizeof(*rsa), 1);
	if(rsa == nil)
		sysfatal("rsaprivalloc");
	return rsa;
}

void
rsaprivfree(RSApriv *rsa)
{
	if(rsa == nil)
		return;
	mpfree(rsa->pub.ek);
	mpfree(rsa->pub.n);
	mpfree(rsa->dk);
	mpfree(rsa->p);
	mpfree(rsa->q);
	mpfree(rsa->kp);
	mpfree(rsa->kq);
	mpfree(rsa->c2);
	free(rsa);
}
