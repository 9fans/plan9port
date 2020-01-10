#include "std.h"
#include "dat.h"

/*
 * PKCS #1 v2.0 signatures (aka RSASSA-PKCS1-V1_5)
 *
 * You don't want to read the spec.
 * Here is what you need to know.
 *
 * RSA sign (aka RSASP1) is just an RSA encryption.
 * RSA verify (aka RSAVP1) is just an RSA decryption.
 *
 * We sign hashes of messages instead of the messages
 * themselves.
 *
 * The hashes are encoded in ASN.1 DER to identify
 * the signature type, and then prefixed with 0x01 PAD 0x00
 * where PAD is as many 0xFF bytes as desired.
 */

static int mkasn1(uchar *asn1, DigestAlg *alg, uchar *d, uint dlen);

int
rsasign(RSApriv *key, DigestAlg *hash, uchar *digest, uint dlen,
	uchar *sig, uint siglen)
{
	uchar asn1[64], *buf;
	int n, len, pad;
	mpint *m, *s;

	/*
	 * Create ASN.1
	 */
	n = mkasn1(asn1, hash, digest, dlen);

	/*
	 * Create number to sign.
	 */
	len = (mpsignif(key->pub.n)+7)/8 - 1;
	if(len < n+2){
		werrstr("rsa key too short");
		return -1;
	}
	pad = len - (n+2);
	if(siglen < len){
		werrstr("signature buffer too short");
		return -1;
	}
	buf = malloc(len);
	if(buf == nil)
		return -1;
	buf[0] = 0x01;
	memset(buf+1, 0xFF, pad);
	buf[1+pad] = 0x00;
	memmove(buf+1+pad+1, asn1, n);
	m = betomp(buf, len, nil);
	free(buf);
	if(m == nil)
		return -1;

	/*
	 * Sign it.
	 */
	s = rsadecrypt(key, m, nil);
	mpfree(m);
	if(s == nil)
		return -1;
	mptoberjust(s, sig, len+1);
	mpfree(s);
	return len+1;
}

int
rsaverify(RSApub *key, DigestAlg *hash, uchar *digest, uint dlen,
	uchar *sig, uint siglen)
{
	uchar asn1[64], xasn1[64];
	int n, nn;
	mpint *m, *s;

	/*
	 * Create ASN.1
	 */
	n = mkasn1(asn1, hash, digest, dlen);

	/*
	 * Extract plaintext of signature.
	 */
	s = betomp(sig, siglen, nil);
	if(s == nil)
		return -1;
	m = rsaencrypt(key, s, nil);
	mpfree(s);
	if(m == nil)
		return -1;
	nn = mptobe(m, xasn1, sizeof xasn1, nil);
	mpfree(m);
	if(n != nn || memcmp(asn1, xasn1, n) != 0){
		werrstr("signature did not verify");
		return -1;
	}
	return 0;
}

/*
 * Mptobe but shift right to fill buffer.
 */
void
mptoberjust(mpint *b, uchar *buf, uint len)
{
	int n;

	n = mptobe(b, buf, len, nil);
	assert(n >= 0);
	if(n < len){
		len -= n;
		memmove(buf+len, buf, n);
		memset(buf, 0, len);
	}
}

/*
 * Simple ASN.1 encodings.
 * Lengths < 128 are encoded as 1-bytes constants,
 * making our life easy.
 */

/*
 * Hash OIDs
 *
 * SHA1 = 1.3.14.3.2.26
 * MDx = 1.2.840.113549.2.x
 */
#define O0(a,b)	((a)*40+(b))
#define O2(x)	\
	(((x)>>7)&0x7F)|0x80, \
	((x)&0x7F)
#define O3(x)	\
	(((x)>>14)&0x7F)|0x80, \
	(((x)>>7)&0x7F)|0x80, \
	((x)&0x7F)
uchar oidsha1[] = { O0(1, 3), 14, 3, 2, 26 };
uchar oidmd2[] = { O0(1, 2), O2(840), O3(113549), 2, 2 };
uchar oidmd5[] = { O0(1, 2), O2(840), O3(113549), 2, 5 };

/*
 *	DigestInfo ::= SEQUENCE {
 *		digestAlgorithm AlgorithmIdentifier,
 *		digest OCTET STRING
 *	}
 *
 * except that OpenSSL seems to sign
 *
 *	DigestInfo ::= SEQUENCE {
 *		SEQUENCE{ digestAlgorithm AlgorithmIdentifier, NULL }
 *		digest OCTET STRING
 *	}
 *
 * instead.  Sigh.
 */
static int
mkasn1(uchar *asn1, DigestAlg *alg, uchar *d, uint dlen)
{
	uchar *obj, *p;
	uint olen;

	if(alg == sha1){
		obj = oidsha1;
		olen = sizeof(oidsha1);
	}else if(alg == md5){
		obj = oidmd5;
		olen = sizeof(oidmd5);
	}else{
		sysfatal("bad alg in mkasn1");
		return -1;
	}

	p = asn1;
	*p++ = 0x30;	/* sequence */
	p++;

	*p++ = 0x30;	/* another sequence */
	p++;

	*p++ = 0x06;	/* object id */
	*p++ = olen;
	memmove(p, obj, olen);
	p += olen;

	*p++ = 0x05;	/* null */
	*p++ = 0;

	asn1[3] = p - (asn1+4);	/* end of inner sequence */

	*p++ = 0x04;	/* octet string */
	*p++ = dlen;
	memmove(p, d, dlen);
	p += dlen;

	asn1[1] = p - (asn1+2);	/* end of outer sequence */
	return p-asn1;
}
