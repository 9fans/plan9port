#include "os.h"
#include <mp.h>
#include <libsec.h>

// I wasn't sure what to do when the buffer was not
// a multiple of 8.  I did what lacy's cryptolib did
// to be compatible, but it looks dangerous to me
// since its encrypting plain text with the key. -- presotto

void
desECBencrypt(uchar *p, int len, DESstate *s)
{
	int i;
	uchar tmp[8];

	for(; len >= 8; len -= 8){
		block_cipher(s->expanded, p, 0);
		p += 8;
	}
	
	if(len > 0){
		for (i=0; i<8; i++)
			tmp[i] = i;
		block_cipher(s->expanded, tmp, 0);
		for (i = 0; i < len; i++)
			p[i] ^= tmp[i];
	}
}

void
desECBdecrypt(uchar *p, int len, DESstate *s)
{
	int i;
	uchar tmp[8];

	for(; len >= 8; len -= 8){
		block_cipher(s->expanded, p, 1);
		p += 8;
	}
	
	if(len > 0){
		for (i=0; i<8; i++)
			tmp[i] = i;
		block_cipher(s->expanded, tmp, 0);
		for (i = 0; i < len; i++)
			p[i] ^= tmp[i];
	}
}
