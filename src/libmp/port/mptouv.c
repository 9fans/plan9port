#include "os.h"
#include <mp.h>
#include "dat.h"

#define VLDIGITS (sizeof(vlong)/sizeof(mpdigit))

/*
 *  this code assumes that a vlong is an integral number of
 *  mpdigits long.
 */
mpint*
uvtomp(uvlong v, mpint *b)
{
	int s;

	if(b == nil)
		b = mpnew(VLDIGITS*sizeof(mpdigit));
	else
		mpbits(b, VLDIGITS*sizeof(mpdigit));
	mpassign(mpzero, b);
	if(v == 0)
		return b;
	for(s = 0; s < VLDIGITS && v != 0; s++){
		b->p[s] = v;
		v >>= sizeof(mpdigit)*8;
	}
	b->top = s;
	return b;
}

uvlong
mptouv(mpint *b)
{
	uvlong v;
	int s;

	if(b->top == 0)
		return 0LL;

	mpnorm(b);
	if(b->top > VLDIGITS)
		return MAXVLONG;

	v = 0ULL;
	for(s = 0; s < b->top; s++)
		v |= b->p[s]<<(s*sizeof(mpdigit)*8);

	return v;
}
