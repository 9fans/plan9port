#include "os.h"
#include <mp.h>
#include "dat.h"

#define VLDIGITS (sizeof(vlong)/sizeof(mpdigit))

/*
 *  this code assumes that a vlong is an integral number of
 *  mpdigits long.
 */
mpint*
vtomp(vlong v, mpint *b)
{
	int s;
	uvlong uv;

	if(b == nil)
		b = mpnew(VLDIGITS*sizeof(mpdigit));
	else
		mpbits(b, VLDIGITS*sizeof(mpdigit));
	mpassign(mpzero, b);
	if(v == 0)
		return b;
	if(v < 0){
		b->sign = -1;
		uv = -v;
	} else
		uv = v;
	for(s = 0; s < VLDIGITS && uv != 0; s++){
		b->p[s] = uv;
		uv >>= sizeof(mpdigit)*8;
	}
	b->top = s;
	return b;
}

vlong
mptov(mpint *b)
{
	uvlong v;
	int s;

	if(b->top == 0)
		return 0LL;

	mpnorm(b);
	if(b->top > VLDIGITS){
		if(b->sign > 0)
			return (vlong)MAXVLONG;
		else
			return (vlong)MINVLONG;
	}

	v = 0ULL;
	for(s = 0; s < b->top; s++)
		v |= b->p[s]<<(s*sizeof(mpdigit)*8);

	if(b->sign > 0){
		if(v > MAXVLONG)
			v = MAXVLONG;
	} else {
		if(v > MINVLONG)
			v = MINVLONG;
		else
			v = -(vlong)v;
	}

	return (vlong)v;
}
