#include "os.h"
#include <mp.h>

// extended euclid
//
// For a and b it solves, d = gcd(a,b) and finds x and y s.t.
// ax + by = d
//
// Handbook of Applied Cryptography, Menezes et al, 1997, pg 67

void
mpeuclid(mpint *a, mpint *b, mpint *d, mpint *x, mpint *y)
{
	mpint *tmp, *x0, *x1, *x2, *y0, *y1, *y2, *q, *r;

	if(a->sign<0 || b->sign<0)
		sysfatal("mpeuclid: negative arg");

	if(mpcmp(a, b) < 0){
		tmp = a;
		a = b;
		b = tmp;
		tmp = x;
		x = y;
		y = tmp;
	}

	if(b->top == 0){
		mpassign(a, d);
		mpassign(mpone, x);
		mpassign(mpzero, y);
		return;
	}

	a = mpcopy(a);
	b = mpcopy(b);
	x0 = mpnew(0);
	x1 = mpcopy(mpzero);
	x2 = mpcopy(mpone);
	y0 = mpnew(0);
	y1 = mpcopy(mpone);
	y2 = mpcopy(mpzero);
	q = mpnew(0);
	r = mpnew(0);

	while(b->top != 0 && b->sign > 0){
		// q = a/b
		// r = a mod b
		mpdiv(a, b, q, r);
		// x0 = x2 - qx1
		mpmul(q, x1, x0);
		mpsub(x2, x0, x0);
		// y0 = y2 - qy1
		mpmul(q, y1, y0);
		mpsub(y2, y0, y0);
		// rotate values
		tmp = a;
		a = b;
		b = r;
		r = tmp;
		tmp = x2;
		x2 = x1;
		x1 = x0;
		x0 = tmp;
		tmp = y2;
		y2 = y1;
		y1 = y0;
		y0 = tmp;
	}

	mpassign(a, d);
	mpassign(x2, x);
	mpassign(y2, y);

	mpfree(x0);
	mpfree(x1);
	mpfree(x2);
	mpfree(y0);
	mpfree(y1);
	mpfree(y2);
	mpfree(q);
	mpfree(r);
	mpfree(a);
	mpfree(b);
}
