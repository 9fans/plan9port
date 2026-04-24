#include "os.h"
#include <mp.h>
#include "dat.h"

// return neg, 0, pos as abs(b1)-abs(b2) is neg, 0, pos
int
mpmagcmp(mpint *b1, mpint *b2)
{
	int i;

	i = b1->flags | b2->flags;
	if(i & MPtimesafe)
		return mpvectscmp(b1->p, b1->top, b2->p, b2->top);
	if(i & MPnorm){
		i = b1->top - b2->top;
		if(i)
			return i;
	}
	return mpveccmp(b1->p, b1->top, b2->p, b2->top);
}

// return neg, 0, pos as b1-b2 is neg, 0, pos
int
mpcmp(mpint *b1, mpint *b2)
{
	int sign;

	sign = (b1->sign - b2->sign) >> 1;
	return sign | (((sign&1)-1) & mpmagcmp(b1, b2)*b1->sign);
}
