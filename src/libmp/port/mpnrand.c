#include "os.h"
#include <mp.h>
#include "dat.h"

/* return uniform random [0..n-1] */
mpint*
mpnrand(mpint *n, void (*gen)(uchar*, int), mpint *b)
{
	int bits;

	bits = mpsignif(n);
	if(bits == 0)
		abort();
	if(b == nil){
		b = mpnew(bits);
		setmalloctag(b, getcallerpc(&n));
	}
	do {
		mprand(bits, gen, b);
	} while(mpmagcmp(b, n) >= 0);

	return b;
}
