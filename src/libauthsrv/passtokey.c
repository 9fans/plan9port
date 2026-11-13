#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <libsec.h>

void
passtodeskey(char key[DESKEYLEN], char *p)
{
	uchar buf[PASSWDLEN], *t;
	int i, n;

	n = strlen(p);
	if(n >= sizeof(buf))
		n = sizeof(buf)-1;
	memset(buf, ' ', 8);
	t = buf;
	memmove(t, p, n);
	t[n] = 0;
	memset(key, 0, DESKEYLEN);
	for(;;){
		for(i = 0; i < DESKEYLEN; i++)
			key[i] = (t[i] >> i) + (t[i+1] << (8 - (i+1)));
		if(n <= 8)
			return;
		n -= 8;
		t += 8;
		if(n < 8){
			t -= 8 - n;
			n = 8;
		}
		encrypt(key, t, 8);
	}
}

void
passtoaeskey(uchar key[AESKEYLEN], char *p)
{
	static char salt[] = "Plan 9 key derivation";

	pbkdf2_x((uchar*)p, strlen(p), (uchar*)salt, sizeof(salt)-1, 9001,
		key, AESKEYLEN, hmac_sha1, SHA1dlen);
}

void
passtokey(Authkey *key, char *pw)
{
	memset(key, 0, sizeof(*key));
	passtodeskey(key->des, pw);
	passtoaeskey(key->aes, pw);
}
