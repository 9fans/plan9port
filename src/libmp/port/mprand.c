#include "os.h"
#include <mp.h>
#include <libsec.h>
#include "dat.h"

mpint*
mprand(int bits, void (*gen)(uchar*, int), mpint *b)
{
	int n, m;
	mpdigit mask;
	uchar *p;

	n = DIGITS(bits);
	if(b == nil)
		b = mpnew(bits);
	else
		mpbits(b, bits);

	p = malloc(n*Dbytes);
	if(p == nil)
		return nil;
	(*gen)(p, n*Dbytes);
	betomp(p, n*Dbytes, b);
	free(p);

	// make sure we don't give too many bits
	m = bits%Dbits;
	n--;
	if(m > 0){
		mask = 1;
		mask <<= m;
		mask--;
		b->p[n] &= mask;
	}

	for(; n >= 0; n--)
		if(b->p[n] != 0)
			break;
	b->top = n+1;
	b->sign = 1;
	return b;
}
