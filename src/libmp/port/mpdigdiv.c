#include "os.h"
#include <mp.h>
#include "dat.h"

//
//	divide two digits by one and return quotient
//
void
mpdigdiv(mpdigit *dividend, mpdigit divisor, mpdigit *quotient)
{
	mpdigit hi, lo, q, x, y;
	int i;

	hi = dividend[1];
	lo = dividend[0];

	// return highest digit value if the result >= 2**32
	if(hi >= divisor || divisor == 0){
		divisor = 0;
		*quotient = ~divisor;
		return;
	}

	// at this point we know that hi < divisor
	// just shift and subtract till we're done
	q = 0;
	x = divisor;
	for(i = Dbits-1; hi > 0 && i >= 0; i--){
		x >>= 1;
		if(x > hi)
			continue;
		y = divisor<<i;
		if(x == hi && y > lo)
			continue;
		if(y > lo)
			hi--;
		lo -= y;
		hi -= x;
		q |= 1<<i;
	}
	q += lo/divisor;
	*quotient = q;
}
