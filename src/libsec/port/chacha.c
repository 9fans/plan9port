/*
Adapted from chacha-merged.c version 20080118
D. J. Bernstein
Public domain.

Modified for Plan 9 style code, including the RFC 7539 nonce layout.
*/

#include "os.h"
#include <libsec.h>

extern void _chachablock(u32int x[16], int rounds);

#define GET4(p) ((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))
#define PUT4(p,v) (p)[0]=(v),(p)[1]=(v)>>8,(p)[2]=(v)>>16,(p)[3]=(v)>>24

#define ENCRYPT(s, x, y, d) { \
	u32int v; \
	v = GET4(s); \
	v ^= (x)+(y); \
	PUT4(d, v); \
}

static uchar sigma[16] = "expand 32-byte k";
static uchar tau[16] = "expand 16-byte k";

static void
load(u32int *d, uchar *s, int nw)
{
	int i;

	for(i = 0; i < nw; i++, s += 4)
		d[i] = GET4(s);
}

static void
hchachablock(uchar h[32], Chachastate *s)
{
	u32int x[16];

	x[0] = s->input[0];
	x[1] = s->input[1];
	x[2] = s->input[2];
	x[3] = s->input[3];
	x[4] = s->input[4];
	x[5] = s->input[5];
	x[6] = s->input[6];
	x[7] = s->input[7];
	x[8] = s->input[8];
	x[9] = s->input[9];
	x[10] = s->input[10];
	x[11] = s->input[11];
	x[12] = s->input[12];
	x[13] = s->input[13];
	x[14] = s->input[14];
	x[15] = s->input[15];

	_chachablock(x, s->rounds);

	PUT4(h+0*4, x[0]);
	PUT4(h+1*4, x[1]);
	PUT4(h+2*4, x[2]);
	PUT4(h+3*4, x[3]);
	PUT4(h+4*4, x[12]);
	PUT4(h+5*4, x[13]);
	PUT4(h+6*4, x[14]);
	PUT4(h+7*4, x[15]);
}

void
setupChachastate(Chachastate *s, uchar *key, ulong keylen, uchar *iv, ulong ivlen, int rounds)
{
	if(keylen != 256/8 && keylen != 128/8)
		sysfatal("invalid chacha key length");
	if(ivlen != 64/8 && ivlen != 96/8 && ivlen != 128/8 && ivlen != 192/8)
		sysfatal("invalid chacha iv length");
	if(rounds == 0)
		rounds = 20;
	s->rounds = rounds;
	if(keylen == 256/8){
		load(&s->input[0], sigma, 4);
		load(&s->input[4], key, 8);
	}else{
		load(&s->input[0], tau, 4);
		load(&s->input[4], key, 4);
		load(&s->input[8], key, 4);
	}
	s->xkey[0] = s->input[4];
	s->xkey[1] = s->input[5];
	s->xkey[2] = s->input[6];
	s->xkey[3] = s->input[7];
	s->xkey[4] = s->input[8];
	s->xkey[5] = s->input[9];
	s->xkey[6] = s->input[10];
	s->xkey[7] = s->input[11];

	s->ivwords = ivlen/4;
	s->input[12] = 0;
	s->input[13] = 0;
	if(iv == nil){
		s->input[14] = 0;
		s->input[15] = 0;
	}else
		chacha_setiv(s, iv);
}

void
chacha_setiv(Chachastate *s, uchar *iv)
{
	if(s->ivwords == 192/32){
		u32int counter[2];
		uchar h[32];

		s->input[4] = s->xkey[0];
		s->input[5] = s->xkey[1];
		s->input[6] = s->xkey[2];
		s->input[7] = s->xkey[3];
		s->input[8] = s->xkey[4];
		s->input[9] = s->xkey[5];
		s->input[10] = s->xkey[6];
		s->input[11] = s->xkey[7];

		counter[0] = s->input[12];
		counter[1] = s->input[13];

		load(&s->input[12], iv, 4);
		hchachablock(h, s);
		load(&s->input[4], h, 8);
		memset(h, 0, sizeof(h));

		s->input[12] = counter[0];
		s->input[13] = counter[1];
		load(&s->input[14], iv+16, 2);
		return;
	}
	load(&s->input[16 - s->ivwords], iv, s->ivwords);
}

void
chacha_setblock(Chachastate *s, u64int blockno)
{
	s->input[12] = blockno;
	if(s->ivwords != 3)
		s->input[13] = blockno>>32;
}

static void
encryptblock(Chachastate *s, uchar *src, uchar *dst)
{
	u32int x[16];
	int i;

	x[0] = s->input[0];
	x[1] = s->input[1];
	x[2] = s->input[2];
	x[3] = s->input[3];
	x[4] = s->input[4];
	x[5] = s->input[5];
	x[6] = s->input[6];
	x[7] = s->input[7];
	x[8] = s->input[8];
	x[9] = s->input[9];
	x[10] = s->input[10];
	x[11] = s->input[11];
	x[12] = s->input[12];
	x[13] = s->input[13];
	x[14] = s->input[14];
	x[15] = s->input[15];
	_chachablock(x, s->rounds);

	for(i = 0; i < nelem(x); i += 4){
		ENCRYPT(src, x[i], s->input[i], dst);
		ENCRYPT(src+4, x[i+1], s->input[i+1], dst+4);
		ENCRYPT(src+8, x[i+2], s->input[i+2], dst+8);
		ENCRYPT(src+12, x[i+3], s->input[i+3], dst+12);
		src += 16;
		dst += 16;
	}

	if(++s->input[12] == 0 && s->ivwords != 3)
		s->input[13]++;
}

void
chacha_encrypt2(uchar *src, uchar *dst, ulong bytes, Chachastate *s)
{
	uchar tmp[ChachaBsize];

	for(; bytes >= ChachaBsize; bytes -= ChachaBsize){
		encryptblock(s, src, dst);
		src += ChachaBsize;
		dst += ChachaBsize;
	}
	if(bytes > 0){
		memmove(tmp, src, bytes);
		encryptblock(s, tmp, tmp);
		memmove(dst, tmp, bytes);
	}
}

void
chacha_encrypt(uchar *buf, ulong bytes, Chachastate *s)
{
	chacha_encrypt2(buf, buf, bytes, s);
}
