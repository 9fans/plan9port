#include "os.h"
#include <libsec.h>

#define U8TO32(p) ((u32int)(p)[0] | (u32int)(p)[1]<<8 | (u32int)(p)[2]<<16 | (u32int)(p)[3]<<24)
#define U32TO8(p, v) (p)[0]=(v), (p)[1]=(v)>>8, (p)[2]=(v)>>16, (p)[3]=(v)>>24

DigestState*
poly1305(uchar *m, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	u32int r0,r1,r2,r3,r4, s1,s2,s3,s4, h0,h1,h2,h3,h4, g0,g1,g2,g3,g4;
	u64int d0,d1,d2,d3,d4, f;
	u32int hibit, mask, c;

	if(s == nil){
		s = malloc(sizeof(*s));
		if(s == nil)
			return nil;
		memset(s, 0, sizeof(*s));
		s->malloced = 1;
	}

	if(s->seeded == 0){
		assert(klen == 32);

		s->state[0] = (U8TO32(&key[0])) & 0x3ffffff;
		s->state[1] = (U8TO32(&key[3]) >> 2) & 0x3ffff03;
		s->state[2] = (U8TO32(&key[6]) >> 4) & 0x3ffc0ff;
		s->state[3] = (U8TO32(&key[9]) >> 6) & 0x3f03fff;
		s->state[4] = (U8TO32(&key[12]) >> 8) & 0x00fffff;

		s->state[5] = 0;
		s->state[6] = 0;
		s->state[7] = 0;
		s->state[8] = 0;
		s->state[9] = 0;

		s->state[10] = U8TO32(&key[16]);
		s->state[11] = U8TO32(&key[20]);
		s->state[12] = U8TO32(&key[24]);
		s->state[13] = U8TO32(&key[28]);

		s->seeded = 1;
	}

	if(s->blen){
		c = 16 - s->blen;
		if(c > len)
			c = len;
		memmove(s->buf + s->blen, m, c);
		len -= c, m += c;
		s->blen += c;
		if(s->blen == 16){
			s->blen = 0;
			poly1305(s->buf, 16, key, klen, nil, s);
		}else if(len == 0){
			m = s->buf;
			len = s->blen;
			s->blen = 0;
		}
	}

	r0 = s->state[0];
	r1 = s->state[1];
	r2 = s->state[2];
	r3 = s->state[3];
	r4 = s->state[4];

	h0 = s->state[5];
	h1 = s->state[6];
	h2 = s->state[7];
	h3 = s->state[8];
	h4 = s->state[9];

	s1 = r1 * 5;
	s2 = r2 * 5;
	s3 = r3 * 5;
	s4 = r4 * 5;

	hibit = 1<<24;

	while(len >= 16){
Block:
		h0 += (U8TO32(&m[0])) & 0x3ffffff;
		h1 += (U8TO32(&m[3]) >> 2) & 0x3ffffff;
		h2 += (U8TO32(&m[6]) >> 4) & 0x3ffffff;
		h3 += (U8TO32(&m[9]) >> 6) & 0x3ffffff;
		h4 += (U8TO32(&m[12]) >> 8) | hibit;

		d0 = ((u64int)h0 * r0) + ((u64int)h1 * s4) + ((u64int)h2 * s3) + ((u64int)h3 * s2) + ((u64int)h4 * s1);
		d1 = ((u64int)h0 * r1) + ((u64int)h1 * r0) + ((u64int)h2 * s4) + ((u64int)h3 * s3) + ((u64int)h4 * s2);
		d2 = ((u64int)h0 * r2) + ((u64int)h1 * r1) + ((u64int)h2 * r0) + ((u64int)h3 * s4) + ((u64int)h4 * s3);
		d3 = ((u64int)h0 * r3) + ((u64int)h1 * r2) + ((u64int)h2 * r1) + ((u64int)h3 * r0) + ((u64int)h4 * s4);
		d4 = ((u64int)h0 * r4) + ((u64int)h1 * r3) + ((u64int)h2 * r2) + ((u64int)h3 * r1) + ((u64int)h4 * r0);

		c = (u32int)(d0 >> 26); h0 = (u32int)d0 & 0x3ffffff;
		d1 += c; c = (u32int)(d1 >> 26); h1 = (u32int)d1 & 0x3ffffff;
		d2 += c; c = (u32int)(d2 >> 26); h2 = (u32int)d2 & 0x3ffffff;
		d3 += c; c = (u32int)(d3 >> 26); h3 = (u32int)d3 & 0x3ffffff;
		d4 += c; c = (u32int)(d4 >> 26); h4 = (u32int)d4 & 0x3ffffff;
		h0 += c * 5; c = (h0 >> 26); h0 &= 0x3ffffff;
		h1 += c;

		len -= 16, m += 16;
	}

	if(len){
		s->blen = len;
		memmove(s->buf, m, len);
	}

	if(digest == nil){
		s->state[5] = h0;
		s->state[6] = h1;
		s->state[7] = h2;
		s->state[8] = h3;
		s->state[9] = h4;
		return s;
	}

	if(len){
		m = s->buf;
		m[len++] = 1;
		while(len < 16)
			m[len++] = 0;
		hibit = 0;
		goto Block;
	}

	c = h1 >> 26; h1 &= 0x3ffffff;
	h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
	h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
	h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
	h1 += c;

	g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
	g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
	g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
	g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
	g4 = h4 + c - (1 << 26);

	mask = (g4 >> 31) - 1;
	g0 &= mask;
	g1 &= mask;
	g2 &= mask;
	g3 &= mask;
	g4 &= mask;
	mask = ~mask;
	h0 = (h0 & mask) | g0;
	h1 = (h1 & mask) | g1;
	h2 = (h2 & mask) | g2;
	h3 = (h3 & mask) | g3;
	h4 = (h4 & mask) | g4;

	h0 = (h0) | (h1 << 26);
	h1 = (h1 >> 6) | (h2 << 20);
	h2 = (h2 >> 12) | (h3 << 14);
	h3 = (h3 >> 18) | (h4 << 8);

	f = (u64int)h0 + s->state[10]; h0 = (u32int)f;
	f = (u64int)h1 + s->state[11] + (f >> 32); h1 = (u32int)f;
	f = (u64int)h2 + s->state[12] + (f >> 32); h2 = (u32int)f;
	f = (u64int)h3 + s->state[13] + (f >> 32); h3 = (u32int)f;

	U32TO8(&digest[0], h0);
	U32TO8(&digest[4], h1);
	U32TO8(&digest[8], h2);
	U32TO8(&digest[12], h3);

	if(s->malloced){
		memset(s, 0, sizeof(*s));
		free(s);
		return nil;
	}

	memset(s, 0, sizeof(*s));
	return nil;
}
