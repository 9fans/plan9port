/*
 * 64-bit IEEE not-a-number routines.
 * This is big/little-endian portable assuming that 
 * the 64-bit doubles and 64-bit integers have the
 * same byte ordering.
 */

#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

#if defined (__APPLE__) || (__powerpc__)
#define _NEEDLL
#endif

static uvlong uvnan    = ((uvlong)0x7FF00000<<32)|0x00000001;
static uvlong uvinf    = ((uvlong)0x7FF00000<<32)|0x00000000;
static uvlong uvneginf = ((uvlong)0xFFF00000<<32)|0x00000000;

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
