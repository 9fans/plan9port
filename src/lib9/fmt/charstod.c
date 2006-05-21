/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

/*
 * Reads a floating-point number by interpreting successive characters
 * returned by (*f)(vp).  The last call it makes to f terminates the
 * scan, so is not a character in the number.  It may therefore be
 * necessary to back up the input stream up one byte after calling charstod.
 */

double
fmtcharstod(int(*f)(void*), void *vp)
{
	double num, dem;
	int neg, eneg, dig, exp, c;

	num = 0;
	neg = 0;
	dig = 0;
	exp = 0;
	eneg = 0;

	c = (*f)(vp);
	while(c == ' ' || c == '\t')
		c = (*f)(vp);
	if(c == '-' || c == '+'){
		if(c == '-')
			neg = 1;
		c = (*f)(vp);
	}
	while(c >= '0' && c <= '9'){
		num = num*10 + c-'0';
		c = (*f)(vp);
	}
	if(c == '.')
		c = (*f)(vp);
	while(c >= '0' && c <= '9'){
		num = num*10 + c-'0';
		dig++;
		c = (*f)(vp);
	}
	if(c == 'e' || c == 'E'){
		c = (*f)(vp);
		if(c == '-' || c == '+'){
			if(c == '-'){
				dig = -dig;
				eneg = 1;
			}
			c = (*f)(vp);
		}
		while(c >= '0' && c <= '9'){
			exp = exp*10 + c-'0';
			c = (*f)(vp);
		}
	}
	exp -= dig;
	if(exp < 0){
		exp = -exp;
		eneg = !eneg;
	}
	dem = __fmtpow10(exp);
	if(eneg)
		num /= dem;
	else
		num *= dem;
	if(neg)
		return -num;
	return num;
}
