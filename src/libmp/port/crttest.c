#include "os.h"
#include <mp.h>
#include <libsec.h>

void
testcrt(mpint **p)
{
	CRTpre *crt;
	CRTres *res;
	mpint *m, *x, *y;
	int i;

	fmtinstall('B', mpconv);

	// get a modulus and a test number
	m = mpnew(1024+160);
	mpmul(p[0], p[1], m);
	x = mpnew(1024+160);
	mpadd(m, mpone, x);

	// do the precomputation for crt conversion
	crt = crtpre(2, p);

	// convert x to residues
	res = crtin(crt, x);

	// convert back
	y = mpnew(1024+160);
	crtout(crt, res, y);
	print("x %B\ny %B\n", x, y);
	mpfree(m);
	mpfree(x);
	mpfree(y);
}

void
main(void)
{
	int i;
	mpint *p[2];
	long start;

	start = time(0);
	for(i = 0; i < 10; i++){
		p[0] = mpnew(1024);
		p[1] = mpnew(1024);
		DSAprimes(p[0], p[1], nil);
		testcrt(p);
		mpfree(p[0]);
		mpfree(p[1]);
	}
	print("%d secs with more\n", time(0)-start);
	exits(0);
}
