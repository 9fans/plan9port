#include "os.h"
#include <mp.h>

/* chinese remainder theorem */
/* */
/* handbook of applied cryptography, menezes et al, 1997, pp 610 - 613 */

struct CRTpre
{
	int	n;		/* number of moduli */
	mpint	**m;		/* pointer to moduli */
	mpint	**c;		/* precomputed coefficients */
	mpint	**p;		/* precomputed products */
	mpint	*a[1];		/* local storage */
};

/* setup crt info, returns a newly created structure */
CRTpre*
crtpre(int n, mpint **m)
{
	CRTpre *crt;
	int i, j;
	mpint *u;

	crt = malloc(sizeof(CRTpre)+sizeof(mpint)*3*n);
	if(crt == nil)
		sysfatal("crtpre: %r");
	crt->m = crt->a;
	crt->c = crt->a+n;
	crt->p = crt->c+n;
	crt->n = n;

	/* make a copy of the moduli */
	for(i = 0; i < n; i++)
		crt->m[i] = mpcopy(m[i]);

	/* precompute the products */
	u = mpcopy(mpone);
	for(i = 0; i < n; i++){
		mpmul(u, m[i], u);
		crt->p[i] = mpcopy(u);
	}

	/* precompute the coefficients */
	for(i = 1; i < n; i++){
		crt->c[i] = mpcopy(mpone);
		for(j = 0; j < i; j++){
			mpinvert(m[j], m[i], u);
			mpmul(u, crt->c[i], u);
			mpmod(u, m[i], crt->c[i]);
		}
	}

	mpfree(u);

	return crt;
}

void
crtprefree(CRTpre *crt)
{
	int i;

	for(i = 0; i < crt->n; i++){
		if(i != 0)
			mpfree(crt->c[i]);
		mpfree(crt->p[i]);
		mpfree(crt->m[i]);
	}
	free(crt);
}

/* convert to residues, returns a newly created structure */
CRTres*
crtin(CRTpre *crt, mpint *x)
{
	int i;
	CRTres *res;

	res = malloc(sizeof(CRTres)+sizeof(mpint)*crt->n);
	if(res == nil)
		sysfatal("crtin: %r");
	res->n = crt->n;
	for(i = 0; i < res->n; i++){
		res->r[i] = mpnew(0);
		mpmod(x, crt->m[i], res->r[i]);
	}
	return res;
}

/* garners algorithm for converting residue form to linear */
void
crtout(CRTpre *crt, CRTres *res, mpint *x)
{
	mpint *u;
	int i;

	u = mpnew(0);
	mpassign(res->r[0], x);

	for(i = 1; i < crt->n; i++){
		mpsub(res->r[i], x, u);
		mpmul(u, crt->c[i], u);
		mpmod(u, crt->m[i], u);
		mpmul(u, crt->p[i-1], u);
		mpadd(x, u, x);
	}

	mpfree(u);
}

/* free the residue */
void
crtresfree(CRTres *res)
{
	int i;

	for(i = 0; i < res->n; i++)
		mpfree(res->r[i]);
	free(res);
}
