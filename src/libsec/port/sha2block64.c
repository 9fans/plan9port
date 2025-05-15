/*
 * sha2_256 block cipher
 *
 * Implementation straight from Federal Information Processing Standards
 * publication 180-2 (+Change Notice to include SHA-224) August 1, 2002
 *   note: the following upper and lower case macro names are distinct
 *	   and reflect the functions defined in FIPS pub. 180-2.
 */

#include <u.h>
#include <libc.h>

#define ROTR(x,n)	(((x) >> (n)) | ((x) << (32-(n))))
#define sigma0(x)	(ROTR((x),7) ^ ROTR((x),18) ^ ((x) >> 3))
#define sigma1(x)	(ROTR((x),17) ^ ROTR((x),19) ^ ((x) >> 10))
#define SIGMA0(x)	(ROTR((x),2) ^ ROTR((x),13) ^ ROTR((x),22))
#define SIGMA1(x)	(ROTR((x),6) ^ ROTR((x),11) ^ ROTR((x),25))
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/*
 * first 32 bits of the fractional parts of cube roots of
 * first 64 primes (2..311).
 */
static u32int K256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
	0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

void
_sha2block64(uchar *p, ulong len, u32int *s)
{
	u32int a, b, c, d, e, f, g, h, t1, t2;
	u32int *kp, *wp;
	u32int w[64];
	uchar *end;

	/* at this point, we have a multiple of 64 bytes */
	for(end = p+len; p < end;){
		a = s[0];
		b = s[1];
		c = s[2];
		d = s[3];
		e = s[4];
		f = s[5];
		g = s[6];
		h = s[7];

		for(wp = w; wp < &w[16]; wp++, p += 4)
			wp[0] = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
		for(; wp < &w[64]; wp++)
			wp[0] = sigma1(wp[-2]) + wp[-7] +
				sigma0(wp[-15]) + wp[-16];

		for(kp = K256, wp = w; wp < &w[64]; ) {
			t1 = h + SIGMA1(e) + Ch(e,f,g) + *kp++ + *wp++;
			t2 = SIGMA0(a) + Maj(a,b,c);
			h = g;
			g = f;
			f = e;
			e = d + t1;
			d = c;
			c = b;
			b = a;
			a = t1 + t2;
		}

		/* save state */
		s[0] += a;
		s[1] += b;
		s[2] += c;
		s[3] += d;
		s[4] += e;
		s[5] += f;
		s[6] += g;
		s[7] += h;
	}
}
