#include "os.h"
#include <mp.h>
#include "dat.h"

// remainder = b mod m
//
// knuth, vol 2, pp 398-400

void
mpmod(mpint *b, mpint *m, mpint *remainder)
{
	mpdiv(b, m, nil, remainder);
	if(remainder->sign < 0)
		mpadd(m, remainder, remainder);
}
