/*
 *	Data Encryption Standard
 *	D.P.Mitchell  83/06/08.
 *
 *	block_cipher(key, block, decrypting)
 *
 *	these routines use the non-standard 7 byte format
 *	for DES keys.
 */
#include <u.h>
#include <libc.h>
#include <auth.h>
#include <libsec.h>

/*
 * destructively encrypt the buffer, which
 * must be at least 8 characters long.
 */
int
encrypt(void *key, void *vbuf, int n)
{
	ulong ekey[32];
	uchar *buf;
	int i, r;

	if(n < 8)
		return 0;
	key_setup(key, ekey);
	buf = vbuf;
	n--;
	r = n % 7;
	n /= 7;
	for(i = 0; i < n; i++){
		block_cipher(ekey, buf, 0);
		buf += 7;
	}
	if(r)
		block_cipher(ekey, buf - 7 + r, 0);
	return 1;
}

/*
 * destructively decrypt the buffer, which
 * must be at least 8 characters long.
 */
int
decrypt(void *key, void *vbuf, int n)
{
	ulong ekey[128];
	uchar *buf;
	int i, r;

	if(n < 8)
		return 0;
	key_setup(key, ekey);
	buf = vbuf;
	n--;
	r = n % 7;
	n /= 7;
	buf += n * 7;
	if(r)
		block_cipher(ekey, buf - 7 + r, 1);
	for(i = 0; i < n; i++){
		buf -= 7;
		block_cipher(ekey, buf, 1);
	}
	return 1;
}
