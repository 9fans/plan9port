/*
 * sha2_512 block cipher
 *
 * Implementation straight from Federal Information Processing Standards
 * publication 180-2 (+Change Notice to include SHA-224) August 1, 2002
 *   note: the following upper and lower case macro names are distinct
 *	   and reflect the functions defined in FIPS pub. 180-2.
 */
#include <u.h>
#include <libc.h>

#define ROTR(x,n)	(((x) >> (n)) | ((x) << (64-(n))))
#define sigma0(x)	(ROTR((x),1) ^ ROTR((x),8) ^ ((x) >> 7))
#define sigma1(x)	(ROTR((x),19) ^ ROTR((x),61) ^ ((x) >> 6))
#define SIGMA0(x)	(ROTR((x),28) ^ ROTR((x),34) ^ ROTR((x),39))
#define SIGMA1(x)	(ROTR((x),14) ^ ROTR((x),18) ^ ROTR((x),41))
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/*
 * first 64 bits of the fractional parts of cube roots of
 * first 80 primes (2..311).
 */
static u64int K512[80] = {
	0x428a2f98d728ae22LL, 0x7137449123ef65cdLL, 0xb5c0fbcfec4d3b2fLL, 0xe9b5dba58189dbbcLL,
	0x3956c25bf348b538LL, 0x59f111f1b605d019LL, 0x923f82a4af194f9bLL, 0xab1c5ed5da6d8118LL,
	0xd807aa98a3030242LL, 0x12835b0145706fbeLL, 0x243185be4ee4b28cLL, 0x550c7dc3d5ffb4e2LL,
	0x72be5d74f27b896fLL, 0x80deb1fe3b1696b1LL, 0x9bdc06a725c71235LL, 0xc19bf174cf692694LL,
	0xe49b69c19ef14ad2LL, 0xefbe4786384f25e3LL, 0x0fc19dc68b8cd5b5LL, 0x240ca1cc77ac9c65LL,
	0x2de92c6f592b0275LL, 0x4a7484aa6ea6e483LL, 0x5cb0a9dcbd41fbd4LL, 0x76f988da831153b5LL,
	0x983e5152ee66dfabLL, 0xa831c66d2db43210LL, 0xb00327c898fb213fLL, 0xbf597fc7beef0ee4LL,
	0xc6e00bf33da88fc2LL, 0xd5a79147930aa725LL, 0x06ca6351e003826fLL, 0x142929670a0e6e70LL,
	0x27b70a8546d22ffcLL, 0x2e1b21385c26c926LL, 0x4d2c6dfc5ac42aedLL, 0x53380d139d95b3dfLL,
	0x650a73548baf63deLL, 0x766a0abb3c77b2a8LL, 0x81c2c92e47edaee6LL, 0x92722c851482353bLL,
	0xa2bfe8a14cf10364LL, 0xa81a664bbc423001LL, 0xc24b8b70d0f89791LL, 0xc76c51a30654be30LL,
	0xd192e819d6ef5218LL, 0xd69906245565a910LL, 0xf40e35855771202aLL, 0x106aa07032bbd1b8LL,
	0x19a4c116b8d2d0c8LL, 0x1e376c085141ab53LL, 0x2748774cdf8eeb99LL, 0x34b0bcb5e19b48a8LL,
	0x391c0cb3c5c95a63LL, 0x4ed8aa4ae3418acbLL, 0x5b9cca4f7763e373LL, 0x682e6ff3d6b2b8a3LL,
	0x748f82ee5defb2fcLL, 0x78a5636f43172f60LL, 0x84c87814a1f0ab72LL, 0x8cc702081a6439ecLL,
	0x90befffa23631e28LL, 0xa4506cebde82bde9LL, 0xbef9a3f7b2c67915LL, 0xc67178f2e372532bLL,
	0xca273eceea26619cLL, 0xd186b8c721c0c207LL, 0xeada7dd6cde0eb1eLL, 0xf57d4f7fee6ed178LL,
	0x06f067aa72176fbaLL, 0x0a637dc5a2c898a6LL, 0x113f9804bef90daeLL, 0x1b710b35131c471bLL,
	0x28db77f523047d84LL, 0x32caab7b40c72493LL, 0x3c9ebe0a15c9bebcLL, 0x431d67c49c100d4cLL,
	0x4cc5d4becb3e42b6LL, 0x597f299cfc657e2aLL, 0x5fcb6fab3ad6faecLL, 0x6c44198c4a475817LL };

void
_sha2block128(uchar *p, ulong len, u64int *s)
{
	u64int a, b, c, d, e, f, g, h, t1, t2;
	u64int *kp, *wp;
	u64int w[80];
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

		for(wp = w; wp < &w[16]; wp++, p += 8)
			wp[0] = ((vlong)p[0])<<56 | ((vlong)p[1])<<48 |
				((vlong)p[2])<<40 | ((vlong)p[3])<<32 |
				p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7];
		for(; wp < &w[80]; wp++) {
			u64int s0, s1;

			s0 = sigma0(wp[-15]);
			s1 = sigma1(wp[-2]);
//			wp[0] = sigma1(wp[-2]) + wp[-7] + sigma0(wp[-15]) + wp[-16];
			wp[0] = s1 + wp[-7] + s0 + wp[-16];
		}

		for(kp = K512, wp = w; wp < &w[80]; ) {
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
