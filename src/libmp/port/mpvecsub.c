#include "os.h"
#include <mp.h>
#include "dat.h"

// prereq: a >= b, alen >= blen, diff has at least alen digits
void
mpvecsub(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *diff)
{
	int i, borrow;
	mpdigit x, y;

	borrow = 0;
	for(i = 0; i < blen; i++){
		x = *a++;
		y = *b++;
		y += borrow;
		if(y < borrow)
			borrow = 1;
		else
			borrow = 0;
		if(x < y)
			borrow++;
		*diff++ = x - y;
	}
	for(; i < alen; i++){
		x = *a++;
		y = x - borrow;
		if(y > x)
			borrow = 1;
		else
			borrow = 0;
		*diff++ = y;
	}
}
