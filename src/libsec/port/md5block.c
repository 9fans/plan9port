#include "os.h"
#include <libsec.h>

/*
 *  rfc1321 requires that I include this.  The code is new.  The constants
 *  all come from the rfc (hence the copyright).  We trade a table for the
 *  macros in rfc.  The total size is a lot less. -- presotto
 *
 *	Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 *	rights reserved.
 *
 *	License to copy and use this software is granted provided that it
 *	is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 *	Algorithm" in all material mentioning or referencing this software
 *	or this function.
 *
 *	License is also granted to make and use derivative works provided
 *	that such works are identified as "derived from the RSA Data
 *	Security, Inc. MD5 Message-Digest Algorithm" in all material
 *	mentioning or referencing the derived work.
 *
 *	RSA Data Security, Inc. makes no representations concerning either
 *	the merchantability of this software or the suitability of this
 *	software forany particular purpose. It is provided "as is"
 *	without express or implied warranty of any kind.
 *	These notices must be retained in any copies of any part of this
 *	documentation and/or software.
 */

/*
 *	Rotate ammounts used in the algorithm
 */
enum
{
	S11=	7,
	S12=	12,
	S13=	17,
	S14=	22,

	S21=	5,
	S22=	9,
	S23=	14,
	S24=	20,

	S31=	4,
	S32=	11,
	S33=	16,
	S34=	23,

	S41=	6,
	S42=	10,
	S43=	15,
	S44=	21,
};

static u32int md5tab[] =
{
	/* round 1 */
/*[0]*/	0xd76aa478,	
	0xe8c7b756,	
	0x242070db,	
	0xc1bdceee,	
	0xf57c0faf,	
	0x4787c62a,	
	0xa8304613,	
	0xfd469501,	
	0x698098d8,	
	0x8b44f7af,	
	0xffff5bb1,	
	0x895cd7be,	
	0x6b901122,	
	0xfd987193,	
	0xa679438e,	
	0x49b40821,

	/* round 2 */
/*[16]*/0xf61e2562,	
	0xc040b340,	
	0x265e5a51,	
	0xe9b6c7aa,	
	0xd62f105d,	
	 0x2441453,	
	0xd8a1e681,	
	0xe7d3fbc8,	
	0x21e1cde6,	
	0xc33707d6,	
	0xf4d50d87,	
	0x455a14ed,	
	0xa9e3e905,	
	0xfcefa3f8,	
	0x676f02d9,	
	0x8d2a4c8a,

	/* round 3 */
/*[32]*/0xfffa3942,	
	0x8771f681,	
	0x6d9d6122,	
	0xfde5380c,	
	0xa4beea44,	
	0x4bdecfa9,	
	0xf6bb4b60,	
	0xbebfbc70,	
	0x289b7ec6,	
	0xeaa127fa,	
	0xd4ef3085,	
	 0x4881d05,	
	0xd9d4d039,	
	0xe6db99e5,	
	0x1fa27cf8,	
	0xc4ac5665,	

	/* round 4 */
/*[48]*/0xf4292244,	
	0x432aff97,	
	0xab9423a7,	
	0xfc93a039,	
	0x655b59c3,	
	0x8f0ccc92,	
	0xffeff47d,	
	0x85845dd1,	
	0x6fa87e4f,	
	0xfe2ce6e0,	
	0xa3014314,	
	0x4e0811a1,	
	0xf7537e82,	
	0xbd3af235,	
	0x2ad7d2bb,	
	0xeb86d391,	
};

static void decode(u32int*, uchar*, ulong);
extern void _md5block(uchar *p, ulong len, u32int *s);

void
_md5block(uchar *p, ulong len, u32int *s)
{
	u32int a, b, c, d, sh;
	u32int *t;
	uchar *end;
	u32int x[16];

	for(end = p+len; p < end; p += 64){
		a = s[0];
		b = s[1];
		c = s[2];
		d = s[3];

		decode(x, p, 64);
	
		t = md5tab;
		sh = 0;
		for(; sh != 16; t += 4){
			a += ((c ^ d) & b) ^ d;
			a += x[sh] + t[0];
			a = (a << S11) | (a >> (32 - S11));
			a += b;

			d += ((b ^ c) & a) ^ c;
			d += x[sh + 1] + t[1];
			d = (d << S12) | (d >> (32 - S12));
			d += a;

			c += ((a ^ b) & d) ^ b;
			c += x[sh + 2] + t[2];
			c = (c << S13) | (c >> (32 - S13));
			c += d;

			b += ((d ^ a) & c) ^ a;
			b += x[sh + 3] + t[3];
			b = (b << S14) | (b >> (32 - S14));
			b += c;

			sh += 4;
		}
		sh = 1;
		for(; sh != 1+20*4; t += 4){
			a += ((b ^ c) & d) ^ c;
			a += x[sh & 0xf] + t[0];
			a = (a << S21) | (a >> (32 - S21));
			a += b;

			d += ((a ^ b) & c) ^ b;
			d += x[(sh + 5) & 0xf] + t[1];
			d = (d << S22) | (d >> (32 - S22));
			d += a;

			c += ((d ^ a) & b) ^ a;
			c += x[(sh + 10) & 0xf] + t[2];
			c = (c << S23) | (c >> (32 - S23));
			c += d;

			b += ((c ^ d) & a) ^ d;
			b += x[(sh + 15) & 0xf] + t[3];
			b = (b << S24) | (b >> (32 - S24));
			b += c;

			sh += 20;
		}
		sh = 5;
		for(; sh != 5+12*4; t += 4){
			a += b ^ c ^ d;
			a += x[sh & 0xf] + t[0];
			a = (a << S31) | (a >> (32 - S31));
			a += b;

			d += a ^ b ^ c;
			d += x[(sh + 3) & 0xf] + t[1];
			d = (d << S32) | (d >> (32 - S32));
			d += a;

			c += d ^ a ^ b;
			c += x[(sh + 6) & 0xf] + t[2];
			c = (c << S33) | (c >> (32 - S33));
			c += d;

			b += c ^ d ^ a;
			b += x[(sh + 9) & 0xf] + t[3];
			b = (b << S34) | (b >> (32 - S34));
			b += c;

			sh += 12;
		}
		sh = 0;
		for(; sh != 28*4; t += 4){
			a += c ^ (b | ~d);
			a += x[sh & 0xf] + t[0];
			a = (a << S41) | (a >> (32 - S41));
			a += b;

			d += b ^ (a | ~c);
			d += x[(sh + 7) & 0xf] + t[1];
			d = (d << S42) | (d >> (32 - S42));
			d += a;

			c += a ^ (d | ~b);
			c += x[(sh + 14) & 0xf] + t[2];
			c = (c << S43) | (c >> (32 - S43));
			c += d;

			b += d ^ (c | ~a);
			b += x[(sh + 21) & 0xf] + t[3];
			b = (b << S44) | (b >> (32 - S44));
			b += c;

			sh += 28;
		}

		s[0] += a;
		s[1] += b;
		s[2] += c;
		s[3] += d;
	}
}

/*
 *	decodes input (uchar) into output (u32int). Assumes len is
 *	a multiple of 4.
 */
static void
decode(u32int *output, uchar *input, ulong len)
{
	uchar *e;

	for(e = input+len; input < e; input += 4)
		*output++ = input[0] | (input[1] << 8) |
			(input[2] << 16) | (input[3] << 24);
}
