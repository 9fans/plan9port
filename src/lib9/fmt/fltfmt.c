/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include "fmt.h"
#include "fmtdef.h"
#include "nan.h"

enum
{
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
xaddexp(char *p, int e)
{
	char se[9];
	int i;

	*p++ = 'e';
	if(e < 0) {
		*p++ = '-';
		e = -e;
	}
	i = 0;
	while(e) {
		se[i++] = e % 10 + '0';
		e /= 10;
	}
	if(i == 0) {
		*p++ = '0';
	} else {
		while(i > 0)
			*p++ = se[--i];
	}
	*p++ = '\0';
}

static char*
xdodtoa(char *s1, double f, int chr, int prec, int *decpt, int *rsign)
{
	char s2[NSIGNIF+10];
	double g, h;
	int e, d, i;
	int c2, sign, oerr;

	if(chr == 'F')
		chr = 'f';
	if(prec > NSIGNIF)
		prec = NSIGNIF;
	if(prec < 0)
		prec = 0;
	if(__isNaN(f)) {
		*decpt = 9999;
		*rsign = 0;
		strcpy(s1, "nan");
		return &s1[3];
	}
	sign = 0;
	if(f < 0) {
		f = -f;
		sign++;
	}
	*rsign = sign;
	if(__isInf(f, 1) || __isInf(f, -1)) {
		*decpt = 9999;
		strcpy(s1, "inf");
		return &s1[3];
	}

	e = 0;
	g = f;
	if(g != 0) {
		frexp(f, &e);
		e = (int)(e * .301029995664);
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
		d = (int)g;
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
	oerr = errno;
	if(c2 >= NSIGNIF-2) {
		strcpy(s2, s1);
		d = e;
		s1[NSIGNIF-2] = '0';
		s1[NSIGNIF-1] = '0';
		xaddexp(s1+NSIGNIF, e-NSIGNIF+1);
		g = fmtstrtod(s1, nil);
		if(g == f)
			goto found;
		if(xadd(s1, NSIGNIF-3, 1)) {
			e++;
			xaddexp(s1+NSIGNIF, e-NSIGNIF+1);
		}
		g = fmtstrtod(s1, nil);
		if(g == f)
			goto found;
		strcpy(s1, s2);
		e = d;
	}

	/*
	 * convert back so s1 gets exact answer
	 */
	for(d = 0; d < 10; d++) {
		xaddexp(s1+NSIGNIF, e-NSIGNIF+1);
		g = fmtstrtod(s1, nil);
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
	errno = oerr;

	/*
	 * sign
	 */
	d = 0;
	i = 0;

	/*
	 * round & adjust 'f' digits
	 */
	c2 = prec + 1;
	if(chr == 'f'){
		if(xadd(s1, c2+e, 5))
			e++;
		c2 += e;
		if(c2 < 0){
			c2 = 0;
			e = -prec - 1;
		}
	}else{
		if(xadd(s1, c2, 5))
			e++;
	}
	if(c2 > NSIGNIF){
		c2 = NSIGNIF;
	}

	*decpt = e + 1;

	/*
	 * terminate the converted digits
	 */
	s1[c2] = '\0';
	return &s1[c2];
}

/*
 * this function works like the standard dtoa, if you want it.
 */
#if 0
static char*
__dtoa(double f, int mode, int ndigits, int *decpt, int *rsign, char **rve)
{
	static char s2[NSIGNIF + 10];
	char *es;
	int chr, prec;

	switch(mode) {
	/* like 'e' */
	case 2:
	case 4:
	case 6:
	case 8:
		chr = 'e';
		break;
	/* like 'g' */
	case 0:
	case 1:
	default:
		chr = 'g';
		break;
	/* like 'f' */
	case 3:
	case 5:
	case 7:
	case 9:
		chr = 'f';
		break;
	}

	if(chr != 'f' && ndigits){
		ndigits--;
	}
	prec = ndigits;
	if(prec > NSIGNIF)
		prec = NSIGNIF;
	if(ndigits == 0)
		prec = NSIGNIF;
	es = xdodtoa(s2, f, chr, prec, decpt, rsign);

	/*
	 * strip trailing 0
	 */
	for(; es > s2 + 1; es--){
		if(es[-1] != '0'){
			break;
		}
	}
	*es = '\0';
	if(rve != NULL)
		*rve = es;
	return s2;
}
#endif

static int
fmtzdotpad(Fmt *f, int n, int pt)
{
	char *t, *s;
	int i;
	Rune *rt, *rs;

	if(f->runes){
		rt = (Rune*)f->to;
		rs = (Rune*)f->stop;
		for(i = 0; i < n; i++){
			if(i == pt){
				FMTRCHAR(f, rt, rs, '.');
			}
			FMTRCHAR(f, rt, rs, '0');
		}
		f->nfmt += rt - (Rune*)f->to;
		f->to = rt;
	}else{
		t = (char*)f->to;
		s = (char*)f->stop;
		for(i = 0; i < n; i++){
			if(i == pt){
				FMTCHAR(f, t, s, '.');
			}
			FMTCHAR(f, t, s, '0');
		}
		f->nfmt += t - (char *)f->to;
		f->to = t;
	}
	return 0;
}

int
__efgfmt(Fmt *fmt)
{
	double f;
	char s1[NSIGNIF+10];
	int e, d, n;
	int c1, c2, c3, c4, ucase, sign, chr, prec, fl;

	f = va_arg(fmt->args, double);
	prec = FDEFLT;
	fl = fmt->flags;
	fmt->flags = 0;
	if(fl & FmtPrec)
		prec = fmt->prec;
	chr = fmt->r;
	ucase = 0;
	if(chr == 'E'){
		chr = 'e';
		ucase = 1;
	}else if(chr == 'F'){
		chr = 'f';
		ucase = 1;
	}else if(chr == 'G'){
		chr = 'g';
		ucase = 1;
	}
	if(prec > 0 && chr == 'g')
		prec--;
	if(prec < 0)
		prec = 0;

	xdodtoa(s1, f, chr, prec, &e, &sign);
	e--;
	if(*s1 == 'i' || *s1 == 'n'){
		if(ucase){
			if(*s1 == 'i'){
				strcpy(s1, "INF");
			}else{
				strcpy(s1, "NAN");
			}
		}
		fmt->flags = fl & (FmtWidth|FmtLeft);
		return __fmtcpy(fmt, (const void*)s1, 3, 3);
	}

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
		chr = 'e';
		break;
	case 'g':
		/*
		 * decide on 'e' of 'f' style convers
		 */
		if(e >= -4 && e <= prec) {
			c1 = -e;
			c4 = prec - e;
			chr = 'h';	/* flag for 'f' style */
		}
		break;
	case 'f':
		c1 = -e;
		if(c1 > prec)
			c1 = prec + 1;
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
	 * trim trailing zeros for %g
	 */
	if(!(fl & FmtSharp)
	&& (chr == 'g' || chr == 'h')){
		if(c4 >= c3){
			c4 -= c3;
			c3 = 0;
		}else{
			c3 -= c4;
			c4 = 0;
		}
		while(c4 && c2 > 1 && s1[c2 - 1] == '0'){
			c4--;
			c2--;
		}
	}

	/*
	 * calculate the total length
	 */
	n = c1 + c2 + c3;
	if(sign || (fl & (FmtSign|FmtSpace)))
		n++;
	if(c4 || (fl & FmtSharp)){
		n++;
	}
	if(chr == 'e' || chr == 'g'){
		n += 4;
		if(e >= 100)
			n++;
	}

	/*
	 * pad to width if right justified
	 */
	if((fl & (FmtWidth|FmtLeft)) == FmtWidth && n < fmt->width){
		if(fl & FmtZero){
			c1 += fmt->width - n;
		}else{
			if(__fmtpad(fmt, fmt->width - n) < 0){
				return -1;
			}
		}
	}

	/*
	 * sign
	 */
	d = 0;
	if(sign)
		d = '-';
	else if(fl & FmtSign)
		d = '+';
	else if(fl & FmtSpace)
		d = ' ';
	if(d && fmtrune(fmt, d) < 0){
		return -1;
	}

	/*
	 * copy digits
	 */
	c4 = c1 + c2 + c3 - c4;
	if(c1 > 0){
		if(fmtzdotpad(fmt, c1, c4) < 0){
			return -1;
		}
		c4 -= c1;
	}
	d = 0;
	if(c4 >= 0 && c4 < c2){
		if(__fmtcpy(fmt, s1, c4, c4) < 0 || fmtrune(fmt, '.') < 0)
			return -1;
		d = c4;
		c2 -= c4;
		c4 = -1;
	}
	if(__fmtcpy(fmt, (const void*)(s1 + d), c2, c2) < 0){
		return -1;
	}
	c4 -= c2;
	if(c3 > 0){
		if(fmtzdotpad(fmt, c3, c4) < 0){
			return -1;
		}
		c4 -= c3;
	}

	/*
	 * strip trailing '0' on g conv
	 */
	if((fl & FmtSharp) && c4 == 0 && fmtrune(fmt, '.') < 0){
		return -1;
	}
	if(chr == 'e' || chr == 'g') {
		d = 0;
		if(ucase)
			s1[d++] = 'E';
		else
			s1[d++] = 'e';
		c1 = e;
		if(c1 < 0) {
			s1[d++] = '-';
			c1 = -c1;
		} else
			s1[d++] = '+';
		if(c1 >= 100) {
			s1[d++] = c1/100 + '0';
			c1 = c1%100;
		}
		s1[d++] = c1/10 + '0';
		s1[d++] = c1%10 + '0';
		if(__fmtcpy(fmt, s1, d, d) < 0){
			return -1;
		}
	}
	if((fl & (FmtWidth|FmtLeft)) == (FmtWidth|FmtLeft) && n < fmt->width){
		if(__fmtpad(fmt, fmt->width - n) < 0){
			return -1;
		}
	}
	return 0;
}
