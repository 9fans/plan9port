#include "os.h"
#include <mp.h>
#include <libsec.h>

//  generate a probable prime.  accuracy is the miller-rabin interations
void
genprime(mpint *p, int n, int accuracy)
{
	mpdigit x;

	// generate n random bits with high and low bits set
	mpbits(p, n);
	genrandom((uchar*)p->p, (n+7)/8);
	p->top = (n+Dbits-1)/Dbits;
	x = 1;
	x <<= ((n-1)%Dbits);
	p->p[p->top-1] &= (x-1);
	p->p[p->top-1] |= x;
	p->p[0] |= 1;

	// keep icrementing till it looks prime
	for(;;){
		if(probably_prime(p, accuracy))
			break;
		mpadd(p, mptwo, p);
	}
}
