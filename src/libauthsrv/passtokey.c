#include <u.h>
#include <libc.h>
#include <authsrv.h>

int
passtokey(char *key, char *p)
{
	uchar buf[ANAMELEN], *t;
	int i, n;

	n = strlen(p);
	if(n >= ANAMELEN)
		n = ANAMELEN-1;
	memset(buf, ' ', 8);
	t = buf;
	strncpy((char*)t, p, n);
	t[n] = 0;
	memset(key, 0, DESKEYLEN);
	for(;;){
		for(i = 0; i < DESKEYLEN; i++)
			key[i] = (t[i] >> i) + (t[i+1] << (8 - (i+1)));
		if(n <= 8)
			return 1;
		n -= 8;
		t += 8;
		if(n < 8){
			t -= 8 - n;
			n = 8;
		}
		encrypt(key, t, 8);
	}
	return 1;	/* not reached */
}
