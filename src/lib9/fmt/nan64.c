/*
 * 64-bit IEEE not-a-number routines.
 * This is big/little-endian portable assuming that 
 * the 64-bit doubles and 64-bit integers have the
 * same byte ordering.
 */

#include "nan.h"

#ifdef __APPLE__
#define _NEEDLL
#endif

typedef unsigned long long uvlong;
typedef unsigned long ulong;

#ifdef _NEEDLL
static uvlong uvnan    = 0x7FF0000000000001LL;
static uvlong uvinf    = 0x7FF0000000000000LL;
static uvlong uvneginf = 0xFFF0000000000000LL;
#else
static uvlong uvnan    = 0x7FF0000000000001;
static uvlong uvinf    = 0x7FF0000000000000;
static uvlong uvneginf = 0xFFF0000000000000;
#endif

double
__NaN(void)
{
	uvlong *p;

	/* gcc complains about "return *(double*)&uvnan;" */
	p = &uvnan;
	return *(double*)p;
}

int
__isNaN(double d)
{
	uvlong x;
	double *p;

	p = &d;
	x = *(uvlong*)p;
	return (ulong)(x>>32)==0x7FF00000 && !__isInf(d, 0);
}

double
__Inf(int sign)
{
	uvlong *p;

	if(sign < 0)
		p = &uvinf;
	else
		p = &uvneginf;
	return *(double*)p;
}

int
__isInf(double d, int sign)
{
	uvlong x;
	double *p;

	p = &d;
	x = *(uvlong*)p;
	if(sign == 0)
		return x==uvinf || x==uvneginf;
	else if(sign > 0)
		return x==uvinf;
	else
		return x==uvneginf;
}


