#ifdef PLAN9
#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#else
#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<errno.h>
#include	"plan9.h"
#endif
#include	"hdr.h"

/*
	the our_* routines are implementations for the corresponding library
	routines. for a while, i tried to actually name them wctomb etc
	but stopped that after i found a system which made wchar_t an
	unsigned char.
*/

#ifdef PLAN9
long getrune(Biobuf *);
long getisorune(Biobuf *);
#else
long getrune(FILE *);
long getisorune(FILE *);
#endif
int our_wctomb(char *s, unsigned long wc);
int our_mbtowc(unsigned long *p, char *s, unsigned n);
int runetoisoutf(char *str, Rune *rune);
int fullisorune(char *str, int n);
int isochartorune(Rune *rune, char *str);

void
utf_in(int fd, long *notused, struct convert *out)
{
#ifndef PLAN9
	FILE *fp;
#else /* PLAN9 */
	Biobuf b;
#endif /* PLAN9 */
	Rune *r;
	long l;

	USED(notused);
#ifndef PLAN9
	if((fp = fdopen(fd, "r")) == NULL){
		EPR "%s: input setup error: %s\n", argv0, strerror(errno));
#else /* PLAN9 */
	if(Binit(&b, fd, OREAD) < 0){
		EPR "%s: input setup error: %r\n", argv0);
#endif /* PLAN9 */
		EXIT(1, "input error");
	}
	r = runes;
	for(;;)
#ifndef PLAN9
		switch(l = getrune(fp))
#else /* PLAN9 */
		switch(l = getrune(&b))
#endif /* PLAN9 */
		{
		case -1:
			goto done;
		case -2:
			if(squawk)
				EPR "%s: bad UTF sequence near byte %ld in input\n", argv0, ninput);
			if(clean)
				continue;
			nerrors++;
			l = Runeerror;
		default:
			*r++ = l;
			if(r >= &runes[N]){
				OUT(out, runes, r-runes);
				r = runes;
			}
		}
done:
	if(r > runes)
		OUT(out, runes, r-runes);
}

void
utf_out(Rune *base, int n, long *notused)
{
	char *p;
	Rune *r;

	USED(notused);
	nrunes += n;
	for(r = base, p = obuf; n-- > 0; r++){
		p += our_wctomb(p, *r);
	}
	noutput += p-obuf;
	write(1, obuf, p-obuf);
}

void
isoutf_in(int fd, long *notused, struct convert *out)
{
#ifndef PLAN9
	FILE *fp;
#else /* PLAN9 */
	Biobuf b;
#endif /* PLAN9 */
	Rune *r;
	long l;

	USED(notused);
#ifndef PLAN9
	if((fp = fdopen(fd, "r")) == 0){
		EPR "%s: input setup error: %s\n", argv0, strerror(errno));
#else /* PLAN9 */
	if(Binit(&b, fd, OREAD) < 0){
		EPR "%s: input setup error: %r\n", argv0);
#endif /* PLAN9 */
		EXIT(1, "input error");
	}
	r = runes;
	for(;;)
#ifndef PLAN9
		switch(l = getisorune(fp))
#else /* PLAN9 */
		switch(l = getisorune(&b))
#endif /* PLAN9 */
		{
		case -1:
			goto done;
		case -2:
			if(squawk)
				EPR "%s: bad UTF sequence near byte %ld in input\n", argv0, ninput);
			if(clean)
				continue;
			nerrors++;
			l = Runeerror;
		default:
			*r++ = l;
			if(r >= &runes[N]){
				OUT(out, runes, r-runes);
				r = runes;
			}
		}
done:
	if(r > runes)
		OUT(out, runes, r-runes);
}

void
isoutf_out(Rune *base, int n, long *notused)
{
	char *p;
	Rune *r;

	USED(notused);
	nrunes += n;
	for(r = base, p = obuf; n-- > 0; r++)
		p += runetoisoutf(p, r);
	noutput += p-obuf;
	write(1, obuf, p-obuf);
}

long
#ifndef PLAN9
getrune(FILE *fp)
#else /* PLAN9 */
getrune(Biobuf *bp)
#endif /* PLAN9 */
{
	int c, i;
	char str[UTFmax];	/* MB_LEN_MAX really */
	unsigned long l;
	int n;

	for(i = 0;;){
#ifndef PLAN9
		c = getc(fp);
#else /* PLAN9 */
		c = Bgetc(bp);
#endif /* PLAN9 */
		if(c < 0)
			return(c);
		ninput++;
		str[i++] = c;
		n = our_mbtowc(&l, str, i);
		if(n == -1)
			return(-2);
		if(n > 0)
			return(l);
	}
}

long
#ifndef PLAN9
getisorune(FILE *fp)
#else /* PLAN9 */
getisorune(Biobuf *bp)
#endif /* PLAN9 */
{
	int c, i;
	Rune rune;
	char str[UTFmax];	/* MB_LEN_MAX really */

	for(i = 0;;){
#ifndef PLAN9
		c = getc(fp);
#else /* PLAN9 */
		c = Bgetc(bp);
#endif /* PLAN9 */
		if(c < 0)
			return(c);
		ninput++;
		str[i++] = c;
		if(fullisorune(str, i))
			break;
	}
	isochartorune(&rune, str);
	if(rune == Runeerror)
		return -2;
	return(rune);
}

enum
{
	Char1	= Runeself,	Rune1	= Runeself,
	Char21	= 0xA1,		Rune21	= 0x0100,
	Char22	= 0xF6,		Rune22	= 0x4016,
	Char3	= 0xFC,		Rune3	= 0x10000,	/* really 0x38E2E */
	Esc	= 0xBE,		Bad	= Runeerror
};

static	uchar	U[256];
static	uchar	T[256];

static
void
mktable(void)
{
	int i, u;

	for(i=0; i<256; i++) {
		u = i + (0x5E - 0xA0);
		if(i < 0xA0)
			u = i + (0xDF - 0x7F);
		if(i < 0x7F)
			u = i + (0x00 - 0x21);
		if(i < 0x21)
			u = i + (0xBE - 0x00);
		U[i] = u;
		T[u] = i;
	}
}

int
isochartorune(Rune *rune, char *str)
{
	int c, c1, c2;
	long l;

	if(U[0] == 0)
		mktable();

	/*
	 * one character sequence
	 *	00000-0009F => 00-9F
	 */
	c = *(uchar*)str;
	if(c < Char1) {
		*rune = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	000A0-000FF => A0; A0-FF
	 */
	c1 = *(uchar*)(str+1);
	if(c < Char21) {
		if(c1 >= Rune1 && c1 < Rune21) {
			*rune = c1;
			return 2;
		}
		goto bad;
	}

	/*
	 * two character sequence
	 *	00100-04015 => A1-F5; 21-7E/A0-FF
	 */
	c1 = U[c1];
	if(c1 >= Esc)
		goto bad;
	if(c < Char22) {
		*rune =  (c-Char21)*Esc + c1 + Rune21;
		return 2;
	}

	/*
	 * three character sequence
	 *	04016-38E2D => A6-FB; 21-7E/A0-FF
	 */
	c2 = U[*(uchar*)(str+2)];
	if(c2 >= Esc)
		goto bad;
	if(c < Char3) {
		l = (c-Char22)*Esc*Esc + c1*Esc + c2 + Rune22;
		if(l >= Rune3)
			goto bad;
		*rune = l;
		return 3;
	}

	/*
	 * bad decoding
	 */
bad:
	*rune = Bad;
	return 1;
}

int
runetoisoutf(char *str, Rune *rune)
{
	long c;

	if(T[0] == 0)
		mktable();

	/*
	 * one character sequence
	 *	00000-0009F => 00-9F
	 */
	c = *rune;
	if(c < Rune1) {
		str[0] = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	000A0-000FF => A0; A0-FF
	 */
	if(c < Rune21) {
		str[0] = (uchar)Char1;
		str[1] = c;
		return 2;
	}

	/*
	 * two character sequence
	 *	00100-04015 => A1-F5; 21-7E/A0-FF
	 */
	if(c < Rune22) {
		c -= Rune21;
		str[0] = c/Esc + Char21;
		str[1] = T[c%Esc];
		return 2;
	}

	/*
	 * three character sequence
	 *	04016-38E2D => A6-FB; 21-7E/A0-FF
	 */
	c -= Rune22;
	str[0] = c/(Esc*Esc) + Char22;
	str[1] = T[c/Esc%Esc];
	str[2] = T[c%Esc];
	return 3;
}

int
fullisorune(char *str, int n)
{
	int c;

	if(n > 0) {
		c = *(uchar*)str;
		if(c < Char1)
			return 1;
		if(n > 1)
			if(c < Char22 || n > 2)
				return 1;
	}
	return 0;
}

#ifdef PLAN9
int	errno;
#endif

enum
{
	T1	= 0x00,
	Tx	= 0x80,
	T2	= 0xC0,
	T3	= 0xE0,
	T4	= 0xF0,
	T5	= 0xF8,
	T6	= 0xFC,

	Bit1	= 7,
	Bitx	= 6,
	Bit2	= 5,
	Bit3	= 4,
	Bit4	= 3,
	Bit5	= 2,
	Bit6	= 2,

	Mask1	= (1<<Bit1)-1,
	Maskx	= (1<<Bitx)-1,
	Mask2	= (1<<Bit2)-1,
	Mask3	= (1<<Bit3)-1,
	Mask4	= (1<<Bit4)-1,
	Mask5	= (1<<Bit5)-1,
	Mask6	= (1<<Bit6)-1,

	Wchar1	= (1UL<<Bit1)-1,
	Wchar2	= (1UL<<(Bit2+Bitx))-1,
	Wchar3	= (1UL<<(Bit3+2*Bitx))-1,
	Wchar4	= (1UL<<(Bit4+3*Bitx))-1,
	Wchar5	= (1UL<<(Bit5+4*Bitx))-1

#ifndef	EILSEQ
	, /* we hate ansi c's comma rules */
	EILSEQ	= 123
#endif /* PLAN9 */
};

int
our_wctomb(char *s, unsigned long wc)
{
	if(s == 0)
		return 0;		/* no shift states */
	if(wc & ~Wchar2) {
		if(wc & ~Wchar4) {
			if(wc & ~Wchar5) {
				/* 6 bytes */
				s[0] = T6 | ((wc >> 5*Bitx) & Mask6);
				s[1] = Tx | ((wc >> 4*Bitx) & Maskx);
				s[2] = Tx | ((wc >> 3*Bitx) & Maskx);
				s[3] = Tx | ((wc >> 2*Bitx) & Maskx);
				s[4] = Tx | ((wc >> 1*Bitx) & Maskx);
				s[5] = Tx |  (wc & Maskx);
				return 6;
			}
			/* 5 bytes */
			s[0] = T5 |  (wc >> 4*Bitx);
			s[1] = Tx | ((wc >> 3*Bitx) & Maskx);
			s[2] = Tx | ((wc >> 2*Bitx) & Maskx);
			s[3] = Tx | ((wc >> 1*Bitx) & Maskx);
			s[4] = Tx |  (wc & Maskx);
			return 5;
		}
		if(wc & ~Wchar3) {
			/* 4 bytes */
			s[0] = T4 |  (wc >> 3*Bitx);
			s[1] = Tx | ((wc >> 2*Bitx) & Maskx);
			s[2] = Tx | ((wc >> 1*Bitx) & Maskx);
			s[3] = Tx |  (wc & Maskx);
			return 4;
		}
		/* 3 bytes */
		s[0] = T3 |  (wc >> 2*Bitx);
		s[1] = Tx | ((wc >> 1*Bitx) & Maskx);
		s[2] = Tx |  (wc & Maskx);
		return 3;
	}
	if(wc & ~Wchar1) {
		/* 2 bytes */
		s[0] = T2 | (wc >> 1*Bitx);
		s[1] = Tx | (wc & Maskx);
		return 2;
	}
	/* 1 byte */
	s[0] = T1 | wc;
	return 1;
}

int
our_mbtowc(unsigned long *p, char *s, unsigned n)
{
	uchar *us;
	int c0, c1, c2, c3, c4, c5;
	unsigned long wc;

	if(s == 0)
		return 0;		/* no shift states */

	if(n < 1)
		goto badlen;
	us = (uchar*)s;
	c0 = us[0];
	if(c0 >= T3) {
		if(n < 3)
			goto badlen;
		c1 = us[1] ^ Tx;
		c2 = us[2] ^ Tx;
		if((c1|c2) & T2)
			goto bad;
		if(c0 >= T5) {
			if(n < 5)
				goto badlen;
			c3 = us[3] ^ Tx;
			c4 = us[4] ^ Tx;
			if((c3|c4) & T2)
				goto bad;
			if(c0 >= T6) {
				/* 6 bytes */
				if(n < 6)
					goto badlen;
				c5 = us[5] ^ Tx;
				if(c5 & T2)
					goto bad;
				wc = ((((((((((c0 & Mask6) << Bitx) |
					c1) << Bitx) | c2) << Bitx) |
					c3) << Bitx) | c4) << Bitx) | c5;
				if(wc <= Wchar5)
					goto bad;
				*p = wc;
				return 6;
			}
			/* 5 bytes */
			wc = ((((((((c0 & Mask5) << Bitx) |
				c1) << Bitx) | c2) << Bitx) |
				c3) << Bitx) | c4;
			if(wc <= Wchar4)
				goto bad;
			*p = wc;
			return 5;
		}
		if(c0 >= T4) {
			/* 4 bytes */
			if(n < 4)
				goto badlen;
			c3 = us[3] ^ Tx;
			if(c3 & T2)
				goto bad;
			wc = ((((((c0 & Mask4) << Bitx) |
				c1) << Bitx) | c2) << Bitx) |
				c3;
			if(wc <= Wchar3)
				goto bad;
			*p = wc;
			return 4;
		}
		/* 3 bytes */
		wc = ((((c0 & Mask3) << Bitx) |
			c1) << Bitx) | c2;
		if(wc <= Wchar2)
			goto bad;
		*p = wc;
		return 3;
	}
	if(c0 >= T2) {
		/* 2 bytes */
		if(n < 2)
			goto badlen;
		c1 = us[1] ^ Tx;
		if(c1 & T2)
			goto bad;
		wc = ((c0 & Mask2) << Bitx) |
			c1;
		if(wc <= Wchar1)
			goto bad;
		*p = wc;
		return 2;
	}
	/* 1 byte */
	if(c0 >= Tx)
		goto bad;
	*p = c0;
	return 1;

bad:
	errno = EILSEQ;
	return -1;
badlen:
	return -2;
}
