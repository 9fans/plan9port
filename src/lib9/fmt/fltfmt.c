/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <fmt.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

enum
{
	FDIGIT	= 30,
	FDEFLT	= 6,
	NSIGNIF	= 17
};

/*
 * first few powers of 10, enough for about 1/2 of the
 * total space for doubles.
 */
static double pows10[] =
{
	  1e0,   1e1,   1e2,   1e3,   1e4,   1e5,   1e6,   1e7,   1e8,   1e9,  
	 1e10,  1e11,  1e12,  1e13,  1e14,  1e15,  1e16,  1e17,  1e18,  1e19,  
	 1e20,  1e21,  1e22,  1e23,  1e24,  1e25,  1e26,  1e27,  1e28,  1e29,  
	 1e30,  1e31,  1e32,  1e33,  1e34,  1e35,  1e36,  1e37,  1e38,  1e39,  
	 1e40,  1e41,  1e42,  1e43,  1e44,  1e45,  1e46,  1e47,  1e48,  1e49,  
	 1e50,  1e51,  1e52,  1e53,  1e54,  1e55,  1e56,  1e57,  1e58,  1e59,  
	 1e60,  1e61,  1e62,  1e63,  1e64,  1e65,  1e66,  1e67,  1e68,  1e69,  
	 1e70,  1e71,  1e72,  1e73,  1e74,  1e75,  1e76,  1e77,  1e78,  1e79,  
	 1e80,  1e81,  1e82,  1e83,  1e84,  1e85,  1e86,  1e87,  1e88,  1e89,  
	 1e90,  1e91,  1e92,  1e93,  1e94,  1e95,  1e96,  1e97,  1e98,  1e99,  
	1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109, 
	1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119, 
	1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129, 
	1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139, 
	1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149, 
	1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159, 
};

#define  pow10(x)  fmtpow10(x)

static double
pow10(int n)
{
	double d;
	int neg;

	neg = 0;
	if(n < 0){
		if(n < DBL_MIN_10_EXP){
			return 0.;
		}
		neg = 1;
		n = -n;
	}else if(n > DBL_MAX_10_EXP){
		return HUGE_VAL;
	}
	if(n < (int)(sizeof(pows10)/sizeof(pows10[0])))
		d = pows10[n];
	else{
		d = pows10[sizeof(pows10)/sizeof(pows10[0]) - 1];
		for(;;){
			n -= sizeof(pows10)/sizeof(pows10[0]) - 1;
			if(n < (int)(sizeof(pows10)/sizeof(pows10[0]))){
				d *= pows10[n];
				break;
			}
			d *= pows10[sizeof(pows10)/sizeof(pows10[0]) - 1];
		}
	}
	if(neg){
		return 1./d;
	}
	return d;
}

static int
xadd(char *a, int n, int v)
{
	char *b;
	int c;

	if(n < 0 || n >= NSIGNIF)
		return 0;
	for(b = a+n; b >= a; b--) {
		c = *b + v;
		if(c <= '9') {
			*b = c;
			return 0;
		}
		*b = '0';
		v = 1;
	}
	*a = '1';	/* overflow adding */
	return 1;
}

static int
xsub(char *a, int n, int v)
{
	char *b;
	int c;

	for(b = a+n; b >= a; b--) {
		c = *b - v;
		if(c >= '0') {
			*b = c;
			return 0;
		}
		*b = '9';
		v = 1;
	}
	*a = '9';	/* underflow subtracting */
	return 1;
}

static void
xdtoa(Fmt *fmt, char *s2, double f)
{
	char s1[NSIGNIF+10];
	double g, h;
	int e, d, i, n;
	int c1, c2, c3, c4, ucase, sign, chr, prec;

	prec = FDEFLT;
	if(fmt->flags & FmtPrec)
		prec = fmt->prec;
	if(prec > FDIGIT)
		prec = FDIGIT;
	if(__isNaN(f)) {
		strcpy(s2, "NaN");
		return;
	}
	if(__isInf(f, 1)) {
		strcpy(s2, "+Inf");
		return;
	}
	if(__isInf(f, -1)) {
		strcpy(s2, "-Inf");
		return;
	}
	sign = 0;
	if(f < 0) {
		f = -f;
		sign++;
	}
	ucase = 0;
	chr = fmt->r;
	if(isupper(chr)) {
		ucase = 1;
		chr = tolower(chr);
	}

	e = 0;
	g = f;
	if(g != 0) {
		frexp(f, &e);
		e = e * .301029995664;
		if(e >= -150 && e <= +150) {
			d = 0;
			h = f;
		} else {
			d = e/2;
			h = f * pow10(-d);
		}
		g = h * pow10(d-e);
		while(g < 1) {
			e--;
			g = h * pow10(d-e);
		}
		while(g >= 10) {
			e++;
			g = h * pow10(d-e);
		}
	}

	/*
	 * convert NSIGNIF digits and convert
	 * back to get accuracy.
	 */
	for(i=0; i<NSIGNIF; i++) {
		d = g;
		s1[i] = d + '0';
		g = (g - d) * 10;
	}
	s1[i] = 0;

	/*
	 * try decimal rounding to eliminate 9s
	 */
	c2 = prec + 1;
	if(chr == 'f')
		c2 += e;
	if(c2 >= NSIGNIF-2) {
		strcpy(s2, s1);
		d = e;
		s1[NSIGNIF-2] = '0';
		s1[NSIGNIF-1] = '0';
		sprint(s1+NSIGNIF, "e%d", e-NSIGNIF+1);
		g = strtod(s1, nil);
		if(g == f)
			goto found;
		if(xadd(s1, NSIGNIF-3, 1)) {
			e++;
			sprint(s1+NSIGNIF, "e%d", e-NSIGNIF+1);
		}
		g = strtod(s1, nil);
		if(g == f)
			goto found;
		strcpy(s1, s2);
		e = d;
	}

	/*
	 * convert back so s1 gets exact answer
	 */
	for(;;) {
		sprint(s1+NSIGNIF, "e%d", e-NSIGNIF+1);
		g = strtod(s1, nil);
		if(f > g) {
			if(xadd(s1, NSIGNIF-1, 1))
				e--;
			continue;
		}
		if(f < g) {
			if(xsub(s1, NSIGNIF-1, 1))
				e++;
			continue;
		}
		break;
	}

found:
	/*
	 * sign
	 */
	d = 0;
	i = 0;
	if(sign)
		s2[d++] = '-';
	else if(fmt->flags & FmtSign)
		s2[d++] = '+';
	else if(fmt->flags & FmtSpace)
		s2[d++] = ' ';

	/*
	 * copy into final place
	 * c1 digits of leading '0'
	 * c2 digits from conversion
	 * c3 digits of trailing '0'
	 * c4 digits after '.'
	 */
	c1 = 0;
	c2 = prec + 1;
	c3 = 0;
	c4 = prec;
	switch(chr) {
	default:
		if(xadd(s1, c2, 5))
			e++;
		break;
	case 'g':
		/*
		 * decide on 'e' of 'f' style convers
		 */
		if(xadd(s1, c2, 5))
			e++;
		if(e >= -5 && e <= prec) {
			c1 = -e - 1;
			c4 = prec - e;
			chr = 'h';	// flag for 'f' style
		}
		break;
	case 'f':
		if(xadd(s1, c2+e, 5))
			e++;
		c1 = -e;
		if(c1 > prec)
			c1 = c2;
		c2 += e;
		break;
	}

	/*
	 * clean up c1 c2 and c3
	 */
	if(c1 < 0)
		c1 = 0;
	if(c2 < 0)
		c2 = 0;
	if(c2 > NSIGNIF) {
		c3 = c2-NSIGNIF;
		c2 = NSIGNIF;
	}

	/*
	 * copy digits
	 */
	while(c1 > 0) {
		if(c1+c2+c3 == c4)
			s2[d++] = '.';
		s2[d++] = '0';
		c1--;
	}
	while(c2 > 0) {
		if(c2+c3 == c4)
			s2[d++] = '.';
		s2[d++] = s1[i++];
		c2--;
	}
	while(c3 > 0) {
		if(c3 == c4)
			s2[d++] = '.';
		s2[d++] = '0';
		c3--;
	}

	/*
	 * strip trailing '0' on g conv
	 */
	if(fmt->flags & FmtSharp) {
		if(0 == c4)
			s2[d++] = '.';
	} else
	if(chr == 'g' || chr == 'h') {
		for(n=d-1; n>=0; n--)
			if(s2[n] != '0')
				break;
		for(i=n; i>=0; i--)
			if(s2[i] == '.') {
				d = n;
				if(i != n)
					d++;
				break;
			}
	}
	if(chr == 'e' || chr == 'g') {
		if(ucase)
			s2[d++] = 'E';
		else
			s2[d++] = 'e';
		c1 = e;
		if(c1 < 0) {
			s2[d++] = '-';
			c1 = -c1;
		} else
			s2[d++] = '+';
		if(c1 >= 100) {
			s2[d++] = c1/100 + '0';
			c1 = c1%100;
		}
		s2[d++] = c1/10 + '0';
		s2[d++] = c1%10 + '0';
	}
	s2[d] = 0;
}

static int
floatfmt(Fmt *fmt, double f)
{
	char s[FDIGIT+10];

	xdtoa(fmt, s, f);
	fmt->flags &= FmtWidth|FmtLeft;
	__fmtcpy(fmt, s, strlen(s), strlen(s));
	return 0;
}

int
__efgfmt(Fmt *f)
{
	double d;

	d = va_arg(f->args, double);
	return floatfmt(f, d);
}
