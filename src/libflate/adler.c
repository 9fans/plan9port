#include <u.h>
#include <libc.h>
#include <flate.h>

enum
{
	ADLERITERS	= 5552,	/* max iters before can overflow 32 bits */
	ADLERBASE	= 65521 /* largest prime smaller than 65536 */
};

ulong
adler32(ulong adler, void *vbuf, int n)
{
	ulong s1, s2;
	uchar *buf, *ebuf;
	int m;

	buf = vbuf;
	s1 = adler & 0xffff;
	s2 = (adler >> 16) & 0xffff;
	for(; n >= 16; n -= m){
		m = n;
		if(m > ADLERITERS)
			m = ADLERITERS;
		m &= ~15;
		for(ebuf = buf + m; buf < ebuf; buf += 16){
			s1 += buf[0];
			s2 += s1;
			s1 += buf[1];
			s2 += s1;
			s1 += buf[2];
			s2 += s1;
			s1 += buf[3];
			s2 += s1;
			s1 += buf[4];
			s2 += s1;
			s1 += buf[5];
			s2 += s1;
			s1 += buf[6];
			s2 += s1;
			s1 += buf[7];
			s2 += s1;
			s1 += buf[8];
			s2 += s1;
			s1 += buf[9];
			s2 += s1;
			s1 += buf[10];
			s2 += s1;
			s1 += buf[11];
			s2 += s1;
			s1 += buf[12];
			s2 += s1;
			s1 += buf[13];
			s2 += s1;
			s1 += buf[14];
			s2 += s1;
			s1 += buf[15];
			s2 += s1;
		}
		s1 %= ADLERBASE;
		s2 %= ADLERBASE;
	}
	if(n){
		for(ebuf = buf + n; buf < ebuf; buf++){
			s1 += buf[0];
			s2 += s1;
		}
		s1 %= ADLERBASE;
		s2 %= ADLERBASE;
	}
	return (s2 << 16) + s1;
}
