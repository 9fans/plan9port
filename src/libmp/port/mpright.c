#include "os.h"
#include <mp.h>
#include "dat.h"

// res = b >> shift
void
mpright(mpint *b, int shift, mpint *res)
{
	int d, l, r, i;
	mpdigit this, last;

	res->sign = b->sign;
	if(b->top==0){
		res->top = 0;
		return;
	}

	// a negative right shift is a left shift
	if(shift < 0){
		mpleft(b, -shift, res);
		return;
	}

	if(res != b)
		mpbits(res, b->top*Dbits - shift);
	d = shift/Dbits;
	r = shift - d*Dbits;
	l = Dbits - r;

	//  shift all the bits out == zero
	if(d>=b->top){
		res->top = 0;
		return;
	}

	// special case digit shifts
	if(r == 0){
		for(i = 0; i < b->top-d; i++)
			res->p[i] = b->p[i+d];
	} else {
		last = b->p[d];
		for(i = 0; i < b->top-d-1; i++){
			this = b->p[i+d+1];
			res->p[i] = (this<<l) | (last>>r);
			last = this;
		}
		res->p[i++] = last>>r;
	}
	while(i > 0 && res->p[i-1] == 0)
		i--;
	res->top = i;
	if(i==0)
		res->sign = 1;
}
