#include "os.h"
#include <libsec.h>

/* rfc2104 */
static DigestState*
hmac_x(uchar *p, ulong len, uchar *key, ulong klen, uchar *digest, DigestState *s,
	DigestState*(*x)(uchar*, ulong, uchar*, DigestState*), int xlen)
{
	int i;
	uchar pad[65], innerdigest[256];

	if(xlen > sizeof(innerdigest))
		return nil;

	if(klen>64)
		return nil;

	/* first time through */
	if(s == nil){
		for(i=0; i<64; i++)
			pad[i] = 0x36;
		pad[64] = 0;
		for(i=0; i<klen; i++)
			pad[i] ^= key[i];
		s = (*x)(pad, 64, nil, nil);
		if(s == nil)
			return nil;
	}

	s = (*x)(p, len, nil, s);
	if(digest == nil)
		return s;

	/* last time through */
	for(i=0; i<64; i++)
		pad[i] = 0x5c;
	pad[64] = 0;
	for(i=0; i<klen; i++)
		pad[i] ^= key[i];
	(*x)(nil, 0, innerdigest, s);
	s = (*x)(pad, 64, nil, nil);
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
