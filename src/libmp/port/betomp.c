#include "os.h"
#include <mp.h>
#include "dat.h"

// convert a big-endian byte array (most significant byte first) to an mpint
mpint*
betomp(uchar *p, uint n, mpint *b)
{
	int m, s;
	mpdigit x;

	if(b == nil)
		b = mpnew(0);

	// dump leading zeros
	while(*p == 0 && n > 1){
		p++;
		n--;
	}

	// get the space
	mpbits(b, n*8);
	b->top = DIGITS(n*8);
	m = b->top-1;

	// first digit might not be Dbytes long
	s = ((n-1)*8)%Dbits;
	x = 0;
	for(; n > 0; n--){
		x |= ((mpdigit)(*p++)) << s;
		s -= 8;
		if(s < 0){
			b->p[m--] = x;
			s = Dbits-8;
			x = 0;
		}
	}

	return b;
}
