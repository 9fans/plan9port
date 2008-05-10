/*
 * 64-bit IEEE not-a-number routines.
 * This is big/little-endian portable assuming that 
 * the 64-bit doubles and 64-bit integers have the
 * same byte ordering.
 */

#include "plan9.h"
#include <assert.h>
#include "fmt.h"
#include "fmtdef.h"

static uvlong uvnan    = ((uvlong)0x7FF00000<<32)|0x00000001;
static uvlong uvinf    = ((uvlong)0x7FF00000<<32)|0x00000000;
static uvlong uvneginf = ((uvlong)0xFFF00000<<32)|0x00000000;

/* gcc sees through the obvious casts. */
static uvlong
d2u(double d)
{
	union {
		uvlong v;
		double d;
	} u;
	assert(sizeof(u.d) == sizeof(u.v));
	u.d = d;
	return u.v;
}

static double
u2d(uvlong v)
{
	union {
		uvlong v;
		double d;
	} u;
	assert(sizeof(u.d) == sizeof(u.v));
	u.v = v;
	return u.d;
}

double
__NaN(void)
{
	return u2d(uvnan);
}

int
__isNaN(double d)
{
	uvlong x;
	
	x = d2u(d);
	/* IEEE 754: exponent bits 0x7FF and non-zero mantissa */
	return (x&uvinf) == uvinf && (x&~uvneginf) != 0;
}

double
__Inf(int sign)
{
	return u2d(sign < 0 ? uvneginf : uvinf);
}

int
__isInf(double d, int sign)
{
	uvlong x;
	
	x = d2u(d);
	if(sign == 0)
		return x==uvinf || x==uvneginf;
	else if(sign > 0)
		return x==uvinf;
	else
		return x==uvneginf;
}
