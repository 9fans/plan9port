#include "os.h"
#include <libsec.h>

/* rfc2104 */
DigestState*
hmac_x(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s,
	DigestState*(*x)(uchar*, ulong, uchar*, DigestState*), int xlen)
{
	int i;
	uchar pad[Hmacblksz+1], innerdigest[256];

	if(xlen > sizeof(innerdigest))
		return nil;

	if(klen > Hmacblksz){
		if(xlen > Hmacblksz)
			return nil;
		(*x)(key, klen, innerdigest, nil);
		key = innerdigest;
		klen = xlen;
	}

	/* first time through */
	if(s == nil || s->seeded == 0){
		memset(pad, 0x36, Hmacblksz);
		pad[Hmacblksz] = 0;
		for(i = 0; i < klen; i++)
			pad[i] ^= key[i];
		s = (*x)(pad, Hmacblksz, nil, s);
		if(s == nil)
			return nil;
	}

	s = (*x)(p, len, nil, s);
	if(digest == nil)
		return s;

	/* last time through */
	memset(pad, 0x5c, Hmacblksz);
	pad[Hmacblksz] = 0;
	for(i = 0; i < klen; i++)
		pad[i] ^= key[i];
	(*x)(nil, 0, innerdigest, s);
	s = (*x)(pad, Hmacblksz, nil, nil);
	(*x)(innerdigest, xlen, digest, s);
	return nil;
}

DigestState*
hmac_sha1(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, sha1, SHA1dlen);
}

DigestState*
hmac_md5(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, md5, MD5dlen);
}

DigestState*
hmac_sha2_256(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest,
	DigestState *s)
{
	return hmac_x(p, len, key, klen, digest, s, sha2_256, SHA2_256dlen);
}
