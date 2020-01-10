#include "os.h"
#include <libsec.h>

/*
 *  This MD4 is implemented from the description in Stinson's Cryptography,
 *  theory and practice. -- presotto
 */

/*
 *	Rotate ammounts used in the algorithm
 */
enum
{
	S11=	3,
	S12=	7,
	S13=	11,
	S14=	19,

	S21=	3,
	S22=	5,
	S23=	9,
	S24=	13,

	S31=	3,
	S32=	9,
	S33=	11,
	S34=	15
};

typedef struct MD4Table MD4Table;
struct MD4Table
{
	uchar	x;	/* index into data block */
	uchar	rot;	/* amount to rotate left by */
};

static MD4Table tab[] =
{
	/* round 1 */
/*[0]*/	{ 0,	S11},
	{ 1,	S12},
	{ 2,	S13},
	{ 3,	S14},
	{ 4,	S11},
	{ 5,	S12},
	{ 6,	S13},
	{ 7,	S14},
	{ 8,	S11},
	{ 9,	S12},
	{ 10,	S13},
	{ 11,	S14},
	{ 12,	S11},
	{ 13,	S12},
	{ 14,	S13},
	{ 15,	S14},

	/* round 2 */
/*[16]*/{ 0,	S21},
	{ 4,	S22},
	{ 8,	S23},
	{ 12,	S24},
	{ 1,	S21},
	{ 5,	S22},
	{ 9,	S23},
	{ 13,	S24},
	{ 2,	S21},
	{ 6,	S22},
	{ 10,	S23},
	{ 14,	S24},
	{ 3,	S21},
	{ 7,	S22},
	{ 11,	S23},
	{ 15,	S24},

	/* round 3 */
/*[32]*/{ 0,	S31},
	{ 8,	S32},
	{ 4,	S33},
	{ 12,	S34},
	{ 2,	S31},
	{ 10,	S32},
	{ 6,	S33},
	{ 14,	S34},
	{ 1,	S31},
	{ 9,	S32},
	{ 5,	S33},
	{ 13,	S34},
	{ 3,	S31},
	{ 11,	S32},
	{ 7,	S33},
	{ 15,	S34},
};

static void encode(uchar*, u32int*, ulong);
static void decode(u32int*, uchar*, ulong);

static void
md4block(uchar *p, ulong len, MD4state *s)
{
	int i;
	u32int a, b, c, d, tmp;
	MD4Table *t;
	uchar *end;
	u32int x[16];

	for(end = p+len; p < end; p += 64){
		a = s->state[0];
		b = s->state[1];
		c = s->state[2];
		d = s->state[3];

		decode(x, p, 64);

		for(i = 0; i < 48; i++){
			t = tab + i;
			switch(i>>4){
			case 0:
				a += (b & c) | (~b & d);
				break;
			case 1:
				a += ((b & c) | (b & d) | (c & d)) + 0x5A827999;
				break;
			case 2:
				a += (b ^ c ^ d) + 0x6ED9EBA1;
				break;
			}
			a += x[t->x];
			a = (a << t->rot) | (a >> (32 - t->rot));

			/* rotate variables */
			tmp = d;
			d = c;
			c = b;
			b = a;
			a = tmp;
		}

		s->state[0] += a;
		s->state[1] += b;
		s->state[2] += c;
		s->state[3] += d;

		s->len += 64;
	}
}

MD4state*
md4(uchar *p, ulong len, uchar *digest, MD4state *s)
{
	u32int x[16];
	uchar buf[128];
	int i;
	uchar *e;

	if(s == nil){
		s = malloc(sizeof(*s));
		if(s == nil)
			return nil;
		memset(s, 0, sizeof(*s));
		s->malloced = 1;
	}

	if(s->seeded == 0){
		/* seed the state, these constants would look nicer big-endian */
		s->state[0] = 0x67452301;
		s->state[1] = 0xefcdab89;
		s->state[2] = 0x98badcfe;
		s->state[3] = 0x10325476;
		s->seeded = 1;
	}

	/* fill out the partial 64 byte block from previous calls */
	if(s->blen){
		i = 64 - s->blen;
		if(len < i)
			i = len;
		memmove(s->buf + s->blen, p, i);
		len -= i;
		s->blen += i;
		p += i;
		if(s->blen == 64){
			md4block(s->buf, s->blen, s);
			s->blen = 0;
		}
	}

	/* do 64 byte blocks */
	i = len & ~0x3f;
	if(i){
		md4block(p, i, s);
		len -= i;
		p += i;
	}

	/* save the left overs if not last call */
	if(digest == 0){
		if(len){
			memmove(s->buf, p, len);
			s->blen += len;
		}
		return s;
	}

	/*
	 *  this is the last time through, pad what's left with 0x80,
	 *  0's, and the input count to create a multiple of 64 bytes
	 */
	if(s->blen){
		p = s->buf;
		len = s->blen;
	} else {
		memmove(buf, p, len);
		p = buf;
	}
	s->len += len;
	e = p + len;
	if(len < 56)
		i = 56 - len;
	else
		i = 120 - len;
	memset(e, 0, i);
	*e = 0x80;
	len += i;

	/* append the count */
	x[0] = s->len<<3;
	x[1] = s->len>>29;
	encode(p+len, x, 8);

	/* digest the last part */
	md4block(p, len+8, s);

	/* return result and free state */
	encode(digest, s->state, MD4dlen);
	if(s->malloced == 1)
		free(s);
	return nil;
}

/*
 *	encodes input (u32int) into output (uchar). Assumes len is
 *	a multiple of 4.
 */
static void
encode(uchar *output, u32int *input, ulong len)
{
	u32int x;
	uchar *e;

	for(e = output + len; output < e;) {
		x = *input++;
		*output++ = x;
		*output++ = x >> 8;
		*output++ = x >> 16;
		*output++ = x >> 24;
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
