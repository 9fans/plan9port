#include <u.h>
#include <libc.h>
#include <authsrv.h>

#define	CHAR(x)		f->x = *p++
#define	SHORT(x)	f->x = (p[0] | (p[1]<<8)); p += 2
#define	VLONG(q)	q = (p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); p += 4
#define	LONG(x)		VLONG(f->x)
#define	STRING(x,n)	memmove(f->x, p, n); p += n

void
convM2PR(char *ap, Passwordreq *f, char *key)
{
	uchar *p;

	p = (uchar*)ap;
	if(key)
		decrypt(key, ap, PASSREQLEN);
	CHAR(num);
	STRING(old, ANAMELEN);
	f->old[ANAMELEN-1] = 0;
	STRING(new, ANAMELEN);
	f->new[ANAMELEN-1] = 0;
	CHAR(changesecret);
	STRING(secret, SECRETLEN);
	f->secret[SECRETLEN-1] = 0;
	USED(p);
}
