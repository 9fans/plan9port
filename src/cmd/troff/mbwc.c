#include <stdlib.h>

/*
 * Use the FSS-UTF transformation proposed by posix.
 *	We define 7 byte types:
 *	T0	0xxxxxxx	7 free bits
 *	Tx	10xxxxxx	6 free bits
 *	T1	110xxxxx	5 free bits
 *	T2	1110xxxx	4 free bits
 *
 *	Encoding is as follows.
 *	From hex	Thru hex	Sequence		Bits
 *	00000000	0000007F	T0			7
 *	00000080	000007FF	T1 Tx			11
 *	00000800	0000FFFF	T2 Tx Tx		16
 */

int
mblen(const char *s, size_t n)
{

	return mbtowc(0, s, n);
}

int
mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	int c, c1, c2;
	long l;

	if(!s)
		return 0;

	if(n < 1)
		goto bad;
	c = s[0] & 0xff;
	if((c & 0x80) == 0x00) {
		if(pwc)
			*pwc = c;
		if(c == 0)
			return 0;
		return 1;
	}

	if(n < 2)
		goto bad;
	c1 = (s[1] ^ 0x80) & 0xff;
	if((c1 & 0xC0) != 0x00)
		goto bad;
	if((c & 0xE0) == 0xC0) {
		l = ((c << 6) | c1) & 0x7FF;
		if(l < 0x080)
			goto bad;
		if(pwc)
			*pwc = l;
		return 2;
	}

	if(n < 3)
		goto bad;
	c2 = (s[2] ^ 0x80) & 0xff;
	if((c2 & 0xC0) != 0x00)
		goto bad;
	if((c & 0xF0) == 0xE0) {
		l = ((((c << 6) | c1) << 6) | c2) & 0xFFFF;
		if(l < 0x0800)
			goto bad;
		if(pwc)
			*pwc = l;
		return 3;
	}

	/*
	 * bad decoding
	 */
bad:
	return -1;

}

int
wctomb(char *s, wchar_t wchar)
{
	long c;

	if(!s)
		return 0;

	c = wchar & 0xFFFF;
	if(c < 0x80) {
		s[0] = c;
		return 1;
	}

	if(c < 0x800) {
		s[0] = 0xC0 | (c >> 6);
		s[1] = 0x80 | (c & 0x3F);
		return 2;
	}

	s[0] = 0xE0 |  (c >> 12);
	s[1] = 0x80 | ((c >> 6) & 0x3F);
	s[2] = 0x80 |  (c & 0x3F);
	return 3;
}

size_t
mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	int i, d, c;

	for(i=0; i < n; i++) {
		c = *s & 0xff;
		if(c < 0x80) {
			*pwcs = c;
			if(c == 0)
				break;
			s++;
		} else {
			d = mbtowc(pwcs, s, 3);
			if(d <= 0)
				return (size_t)((d<0) ? -1 : i);
			s += d;
		}
		pwcs++;
	}
	return i;
}

size_t
wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	int d;
	long c;
	char *p, *pe;
	char buf[3];

	p = s;
	pe = p+n-3;
	while(p < pe) {
		c = *pwcs++;
		if(c < 0x80)
			*p++ = c;
		else
			p += wctomb(p, c);
		if(c == 0)
			return p-s;
	}
	while(p < pe+3) {
		c = *pwcs++;
		d = wctomb(buf, c);
		if(p+d <= pe+3) {
			*p++ = buf[0];
			if(d > 1) {
				*p++ = buf[1];
				if(d > 2)
					*p++ = buf[2];
			}
		}
		if(c == 0)
			break;
	}
	return p-s;
}
