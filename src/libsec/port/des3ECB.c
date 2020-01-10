#include "os.h"
#include <mp.h>
#include <libsec.h>

/* I wasn't sure what to do when the buffer was not */
/* a multiple of 8.  I did what lacy's cryptolib did */
/* to be compatible, but it looks dangerous to me */
/* since its encrypting plain text with the key. -- presotto */

void
des3ECBencrypt(uchar *p, int len, DES3state *s)
{
	int i;
	uchar tmp[8];

	for(; len >= 8; len -= 8){
		triple_block_cipher(s->expanded, p, DES3EDE);
		p += 8;
	}

	if(len > 0){
		for (i=0; i<8; i++)
			tmp[i] = i;
		triple_block_cipher(s->expanded, tmp, DES3EDE);
		for (i = 0; i < len; i++)
			p[i] ^= tmp[i];
	}
}

void
des3ECBdecrypt(uchar *p, int len, DES3state *s)
{
	int i;
	uchar tmp[8];

	for(; len >= 8; len -= 8){
		triple_block_cipher(s->expanded, p, DES3DED);
		p += 8;
	}

	if(len > 0){
		for (i=0; i<8; i++)
			tmp[i] = i;
		triple_block_cipher(s->expanded, tmp, DES3EDE);
		for (i = 0; i < len; i++)
			p[i] ^= tmp[i];
	}
}
