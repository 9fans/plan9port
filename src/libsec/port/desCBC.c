#include "os.h"
#include <mp.h>
#include <libsec.h>

// Because of the way that non multiple of 8
// buffers are handled, the decryptor must
// be fed buffers of the same size as the
// encryptor


// If the length is not a multiple of 8, I encrypt
// the overflow to be compatible with lacy's cryptlib
void
desCBCencrypt(uchar *p, int len, DESstate *s)
{
	uchar *p2, *ip, *eip;

	for(; len >= 8; len -= 8){
		p2 = p;
		ip = s->ivec;
		for(eip = ip+8; ip < eip; )
			*p2++ ^= *ip++;
		block_cipher(s->expanded, p, 0);
		memmove(s->ivec, p, 8);
		p += 8;
	}

	if(len > 0){
		ip = s->ivec;
		block_cipher(s->expanded, ip, 0);
		for(eip = ip+len; ip < eip; )
			*p++ ^= *ip++;
	}
}

void
desCBCdecrypt(uchar *p, int len, DESstate *s)
{
	uchar *ip, *eip, *tp;
	uchar tmp[8];

	for(; len >= 8; len -= 8){
		memmove(tmp, p, 8);
		block_cipher(s->expanded, p, 1);
		tp = tmp;
		ip = s->ivec;
		for(eip = ip+8; ip < eip; ){
			*p++ ^= *ip;
			*ip++ = *tp++;
		}
	}

	if(len > 0){
		ip = s->ivec;
		block_cipher(s->expanded, ip, 0);
		for(eip = ip+len; ip < eip; )
			*p++ ^= *ip++;
	}
}
