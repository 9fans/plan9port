#include "os.h"
#include <mp.h>
#include "dat.h"

// sum = abs(b1) + abs(b2), i.e., add the magnitudes
void
mpmagadd(mpint *b1, mpint *b2, mpint *sum)
{
	int m, n;
	mpint *t;

	// get the sizes right
	if(b2->top > b1->top){
		t = b1;
		b1 = b2;
		b2 = t;
	}
	n = b1->top;
	m = b2->top;
	if(n == 0){
		mpassign(mpzero, sum);
		return;
	}
	if(m == 0){
		mpassign(b1, sum);
		return;
	}
	mpbits(sum, (n+1)*Dbits);
	sum->top = n+1;

	mpvecadd(b1->p, n, b2->p, m, sum->p);
	sum->sign = 1;

	mpnorm(sum);
}

// sum = b1 + b2
void
mpadd(mpint *b1, mpint *b2, mpint *sum)
{
	int sign;

	if(b1->sign != b2->sign){
		if(b1->sign < 0)
			mpmagsub(b2, b1, sum);
		else
			mpmagsub(b1, b2, sum);
	} else {
		sign = b1->sign;
		mpmagadd(b1, b2, sum);
		if(sum->top != 0)
			sum->sign = sign;
	}
}
