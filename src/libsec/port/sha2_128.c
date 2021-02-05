/*
 * sha2 128-bit
 */
#include <u.h>
#include <libc.h>
#include <libsec.h>

static void encode64(uchar*, u64int*, ulong);
static DigestState* sha2_128(uchar *, ulong, uchar *, SHA2_256state *, int);

extern void _sha2block128(uchar*, ulong, u64int*);

/*
 *  for sha2_384 and sha2_512, len must be multiple of 128 for all but
 *  the last call.  There must be room in the input buffer to pad.
 *
 *  Note: sha2_384 calls sha2_512block as sha2_384; it just uses a different
 *  initial seed to produce a truncated 384b hash result.  otherwise
 *  it's the same as sha2_512.
 */
SHA2_384state*
sha2_384(uchar *p, ulong len, uchar *digest, SHA2_384state *s)
{
	if(s == nil) {
		s = mallocz(sizeof(*s), 1);
		if(s == nil)
			return nil;
		s->malloced = 1;
	}
	if(s->seeded == 0){
		/*
		 * seed the state with the first 64 bits of the fractional
		 * parts of the square roots of the 9th thru 16th primes.
		 */
 		s->bstate[0] = 0xcbbb9d5dc1059ed8LL;
		s->bstate[1] = 0x629a292a367cd507LL;
		s->bstate[2] = 0x9159015a3070dd17LL;
		s->bstate[3] = 0x152fecd8f70e5939LL;
		s->bstate[4] = 0x67332667ffc00b31LL;
		s->bstate[5] = 0x8eb44a8768581511LL;
		s->bstate[6] = 0xdb0c2e0d64f98fa7LL;
		s->bstate[7] = 0x47b5481dbefa4fa4LL;
		s->seeded = 1;
	}
	return sha2_128(p, len, digest, s, SHA2_384dlen);
}

SHA2_512state*
sha2_512(uchar *p, ulong len, uchar *digest, SHA2_512state *s)
{

	if(s == nil) {
		s = mallocz(sizeof(*s), 1);
		if(s == nil)
			return nil;
		s->malloced = 1;
	}
	if(s->seeded == 0){
		/*
		 * seed the state with the first 64 bits of the fractional
		 * parts of the square roots of the first 8 primes 2..19).
		 */
 		s->bstate[0] = 0x6a09e667f3bcc908LL;
		s->bstate[1] = 0xbb67ae8584caa73bLL;
		s->bstate[2] = 0x3c6ef372fe94f82bLL;
		s->bstate[3] = 0xa54ff53a5f1d36f1LL;
		s->bstate[4] = 0x510e527fade682d1LL;
		s->bstate[5] = 0x9b05688c2b3e6c1fLL;
		s->bstate[6] = 0x1f83d9abfb41bd6bLL;
		s->bstate[7] = 0x5be0cd19137e2179LL;
		s->seeded = 1;
	}
	return sha2_128(p, len, digest, s, SHA2_512dlen);
}

/* common 128 byte block padding and count code for SHA2_384 and SHA2_512 */
static DigestState*
sha2_128(uchar *p, ulong len, uchar *digest, SHA2_512state *s, int dlen)
{
	int i;
	u64int x[16];
	uchar buf[256];
	uchar *e;

	/* fill out the partial 128 byte block from previous calls */
	if(s->blen){
		i = 128 - s->blen;
		if(len < i)
			i = len;
		memmove(s->buf + s->blen, p, i);
		len -= i;
		s->blen += i;
		p += i;
		if(s->blen == 128){
			_sha2block128(s->buf, s->blen, s->bstate);
			s->len += s->blen;
			s->blen = 0;
		}
	}

	/* do 128 byte blocks */
	i = len & ~(128-1);
	if(i){
		_sha2block128(p, i, s->bstate);
		s->len += i;
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
	 *  0's, and the input count to create a multiple of 128 bytes.
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
	if(len < 112)
		i = 112 - len;
	else
		i = 240 - len;
	memset(e, 0, i);
	*e = 0x80;
	len += i;

	/* append the count */
	x[0] = 0;			/* assume 32b length, i.e. < 4GB */
	x[1] = s->len<<3;
	encode64(p+len, x, 16);

	/* digest the last part */
	_sha2block128(p, len+16, s->bstate);
	s->len += len+16;

	/* return result and free state */
	encode64(digest, s->bstate, dlen);
	if(s->malloced == 1)
		free(s);
	return nil;
}

/*
 * Encodes input (ulong long) into output (uchar).
 * Assumes len is a multiple of 8.
 */
static void
encode64(uchar *output, u64int *input, ulong len)
{
	u64int x;
	uchar *e;

	for(e = output + len; output < e;) {
		x = *input++;
		*output++ = x >> 56;
		*output++ = x >> 48;
		*output++ = x >> 40;
		*output++ = x >> 32;
		*output++ = x >> 24;
		*output++ = x >> 16;
		*output++ = x >> 8;
		*output++ = x;
	}
}

DigestState*
hmac_sha2_384(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest,
	DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, sha2_384, SHA2_384dlen);
}

DigestState*
hmac_sha2_512(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest,
	DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, sha2_512, SHA2_512dlen);
}
