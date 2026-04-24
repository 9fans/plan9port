/*
 * sha2_256 block cipher - unrolled version
 *
 * note: the following upper and lower case macro names are distinct
 * and reflect the functions defined in FIPS pub. 180-2.
 */

#include "os.h"

#define ROTR(x,n)	(((x) >> (n)) | ((x) << (32-(n))))
#define sigma0(x)	(ROTR((x),7) ^ ROTR((x),18) ^ ((x) >> 3))
#define sigma1(x)	(ROTR((x),17) ^ ROTR((x),19) ^ ((x) >> 10))
#define SIGMA0(x)	(ROTR((x),2) ^ ROTR((x),13) ^ ROTR((x),22))
#define SIGMA1(x)	(ROTR((x),6) ^ ROTR((x),11) ^ ROTR((x),25))
#define Ch(x,y,z)	((z) ^ ((x) & ((y) ^ (z))))
#define Maj(x,y,z)	(((x) | (y)) & ((z) | ((x) & (y))))

/*
 * First 32 bits of the fractional parts of cube roots of
 * the first 64 primes (2..311).
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
	u32int w[16], a, b, c, d, e, f, g, h;
	uchar *end;

	for(end = p + len; p < end;){
		a = s[0];
		b = s[1];
		c = s[2];
		d = s[3];
		e = s[4];
		f = s[5];
		g = s[6];
		h = s[7];

#define STEP(a,b,c,d,e,f,g,h,i) \
	if(i < 16) {\
		w[i] = p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3]; \
		p += 4; \
	} else { \
		w[i&15] += sigma1(w[i-2&15]) + w[i-7&15] + sigma0(w[i-15&15]); \
	} \
	h += SIGMA1(e) + Ch(e,f,g) + K256[i] + w[i&15]; \
	d += h; \
	h += SIGMA0(a) + Maj(a,b,c);

		STEP(a,b,c,d,e,f,g,h,0);
		STEP(h,a,b,c,d,e,f,g,1);
		STEP(g,h,a,b,c,d,e,f,2);
		STEP(f,g,h,a,b,c,d,e,3);
		STEP(e,f,g,h,a,b,c,d,4);
		STEP(d,e,f,g,h,a,b,c,5);
		STEP(c,d,e,f,g,h,a,b,6);
		STEP(b,c,d,e,f,g,h,a,7);

		STEP(a,b,c,d,e,f,g,h,8);
		STEP(h,a,b,c,d,e,f,g,9);
		STEP(g,h,a,b,c,d,e,f,10);
		STEP(f,g,h,a,b,c,d,e,11);
		STEP(e,f,g,h,a,b,c,d,12);
		STEP(d,e,f,g,h,a,b,c,13);
		STEP(c,d,e,f,g,h,a,b,14);
		STEP(b,c,d,e,f,g,h,a,15);

		STEP(a,b,c,d,e,f,g,h,16);
		STEP(h,a,b,c,d,e,f,g,17);
		STEP(g,h,a,b,c,d,e,f,18);
		STEP(f,g,h,a,b,c,d,e,19);
		STEP(e,f,g,h,a,b,c,d,20);
		STEP(d,e,f,g,h,a,b,c,21);
		STEP(c,d,e,f,g,h,a,b,22);
		STEP(b,c,d,e,f,g,h,a,23);

		STEP(a,b,c,d,e,f,g,h,24);
		STEP(h,a,b,c,d,e,f,g,25);
		STEP(g,h,a,b,c,d,e,f,26);
		STEP(f,g,h,a,b,c,d,e,27);
		STEP(e,f,g,h,a,b,c,d,28);
		STEP(d,e,f,g,h,a,b,c,29);
		STEP(c,d,e,f,g,h,a,b,30);
		STEP(b,c,d,e,f,g,h,a,31);

		STEP(a,b,c,d,e,f,g,h,32);
		STEP(h,a,b,c,d,e,f,g,33);
		STEP(g,h,a,b,c,d,e,f,34);
		STEP(f,g,h,a,b,c,d,e,35);
		STEP(e,f,g,h,a,b,c,d,36);
		STEP(d,e,f,g,h,a,b,c,37);
		STEP(c,d,e,f,g,h,a,b,38);
		STEP(b,c,d,e,f,g,h,a,39);

		STEP(a,b,c,d,e,f,g,h,40);
		STEP(h,a,b,c,d,e,f,g,41);
		STEP(g,h,a,b,c,d,e,f,42);
		STEP(f,g,h,a,b,c,d,e,43);
		STEP(e,f,g,h,a,b,c,d,44);
		STEP(d,e,f,g,h,a,b,c,45);
		STEP(c,d,e,f,g,h,a,b,46);
		STEP(b,c,d,e,f,g,h,a,47);

		STEP(a,b,c,d,e,f,g,h,48);
		STEP(h,a,b,c,d,e,f,g,49);
		STEP(g,h,a,b,c,d,e,f,50);
		STEP(f,g,h,a,b,c,d,e,51);
		STEP(e,f,g,h,a,b,c,d,52);
		STEP(d,e,f,g,h,a,b,c,53);
		STEP(c,d,e,f,g,h,a,b,54);
		STEP(b,c,d,e,f,g,h,a,55);

		STEP(a,b,c,d,e,f,g,h,56);
		STEP(h,a,b,c,d,e,f,g,57);
		STEP(g,h,a,b,c,d,e,f,58);
		STEP(f,g,h,a,b,c,d,e,59);
		STEP(e,f,g,h,a,b,c,d,60);
		STEP(d,e,f,g,h,a,b,c,61);
		STEP(c,d,e,f,g,h,a,b,62);
		STEP(b,c,d,e,f,g,h,a,63);

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
