#include "os.h"
#include <mp.h>
#include "dat.h"

// res = b**e
//
// knuth, vol 2, pp 398-400

enum {
	Freeb=	0x1,
	Freee=	0x2,
	Freem=	0x4,
};

//int expdebug;

void
mpexp(mpint *b, mpint *e, mpint *m, mpint *res)
{
	mpint *t[2];
	int tofree;
	mpdigit d, bit;
	int i, j;

	i = mpcmp(e,mpzero);
	if(i==0){
		mpassign(mpone, res);
		return;
	}
	if(i<0)
		sysfatal("mpexp: negative exponent");

	t[0] = mpcopy(b);
	t[1] = res;

	tofree = 0;
	if(res == b){
		b = mpcopy(b);
		tofree |= Freeb;
	}
	if(res == e){
		e = mpcopy(e);
		tofree |= Freee;
	}
	if(res == m){
		m = mpcopy(m);
		tofree |= Freem;
	}

	// skip first bit
	i = e->top-1;
	d = e->p[i];
	for(bit = mpdighi; (bit & d) == 0; bit >>= 1)
		;
	bit >>= 1;

	j = 0;
	for(;;){
		for(; bit != 0; bit >>= 1){
			mpmul(t[j], t[j], t[j^1]);
			if(bit & d)
				mpmul(t[j^1], b, t[j]);
			else
				j ^= 1;
			if(m != nil && t[j]->top > m->top){
				mpmod(t[j], m, t[j^1]);
				j ^= 1;
			}
		}
		if(--i < 0)
			break;
		bit = mpdighi;
		d = e->p[i];
	}
	if(m != nil){
		mpmod(t[j], m, t[j^1]);
		j ^= 1;
	}
	if(t[j] == res){
		mpfree(t[j^1]);
	} else {
		mpassign(t[j], res);
		mpfree(t[j]);
	}

	if(tofree){
		if(tofree & Freeb)
			mpfree(b);
		if(tofree & Freee)
			mpfree(e);
		if(tofree & Freem)
			mpfree(m);
	}
}
