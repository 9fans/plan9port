#include "os.h"
#include <mp.h>

#define iseven(a)	(((a)->p[0] & 1) == 0)

// extended binary gcd
//
// For a anv b it solves, v = gcd(a,b) and finds x and y s.t.
// ax + by = v
//
// Handbook of Applied Cryptography, Menezes et al, 1997, pg 608.  
void
mpextendedgcd(mpint *a, mpint *b, mpint *v, mpint *x, mpint *y)
{
	mpint *u, *A, *B, *C, *D;
	int g;

	if(a->sign < 0 || b->sign < 0){
		mpassign(mpzero, v);
		mpassign(mpzero, y);
		mpassign(mpzero, x);
		return;
	}

	if(a->top == 0){
		mpassign(b, v);
		mpassign(mpone, y);
		mpassign(mpzero, x);
		return;
	}
	if(b->top == 0){
		mpassign(a, v);
		mpassign(mpone, x);
		mpassign(mpzero, y);
		return;
	}

	g = 0;
	a = mpcopy(a);
	b = mpcopy(b);

	while(iseven(a) && iseven(b)){
		mpright(a, 1, a);
		mpright(b, 1, b);
		g++;
	}

	u = mpcopy(a);
	mpassign(b, v);
	A = mpcopy(mpone);
	B = mpcopy(mpzero);
	C = mpcopy(mpzero);
	D = mpcopy(mpone);

	for(;;) {
//		print("%B %B %B %B %B %B\n", u, v, A, B, C, D);
		while(iseven(u)){
			mpright(u, 1, u);
			if(!iseven(A) || !iseven(B)) {
				mpadd(A, b, A);
				mpsub(B, a, B);
			}
			mpright(A, 1, A);
			mpright(B, 1, B);
		}
	
//		print("%B %B %B %B %B %B\n", u, v, A, B, C, D);
		while(iseven(v)){
			mpright(v, 1, v);
			if(!iseven(C) || !iseven(D)) {
				mpadd(C, b, C);
				mpsub(D, a, D);
			}
			mpright(C, 1, C);
			mpright(D, 1, D);
		}
	
//		print("%B %B %B %B %B %B\n", u, v, A, B, C, D);
		if(mpcmp(u, v) >= 0){
			mpsub(u, v, u);
			mpsub(A, C, A);
			mpsub(B, D, B);
		} else {
			mpsub(v, u, v);
			mpsub(C, A, C);
			mpsub(D, B, D);
		}

		if(u->top == 0)
			break;

	}
	mpassign(C, x);
	mpassign(D, y);
	mpleft(v, g, v);

	mpfree(A);
	mpfree(B);
	mpfree(C);
	mpfree(D);
	mpfree(u);
	mpfree(a);
	mpfree(b);

	return;
}
