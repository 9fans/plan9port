#include "os.h"
#include <mp.h>
#include "dat.h"

/*
 *  this code assumes that mpdigit is at least as
 *  big as an int.
 */

mpint*
uitomp(uint i, mpint *b)
{
	if(b == nil)
		b = mpnew(0);
	mpassign(mpzero, b);
	if(i != 0)
		b->top = 1;
	*b->p = i;
	return b;
}

uint
mptoui(mpint *b)
{
	uint x;

	x = *b->p;
	if(b->sign < 0){
		x = 0;
	} else {
		if(b->top > 1 || x > MAXUINT)
			x =  MAXUINT;
	}
	return x;
}
