#include "os.h"
#include <mp.h>
#include "dat.h"

// return neg, 0, pos as abs(b1)-abs(b2) is neg, 0, pos
int
mpmagcmp(mpint *b1, mpint *b2)
{
	int i;

	i = b1->top - b2->top;
	if(i)
		return i;

	return mpveccmp(b1->p, b1->top, b2->p, b2->top);
}

// return neg, 0, pos as b1-b2 is neg, 0, pos
int
mpcmp(mpint *b1, mpint *b2)
{
	if(b1->sign != b2->sign)
		return b1->sign - b2->sign;
	if(b1->sign < 0)
		return mpmagcmp(b2, b1);
	else
		return mpmagcmp(b1, b2);
}
