#include <u.h>
#include <libc.h>
#include <authsrv.h>

#define	CHAR(x)		*p++ = f->x
#define	SHORT(x)	p[0] = f->x; p[1] = f->x>>8; p += 2
#define	VLONG(q)	p[0] = (q); p[1] = (q)>>8; p[2] = (q)>>16; p[3] = (q)>>24; p += 4
#define	LONG(x)		VLONG(f->x)
#define	STRING(x,n)	memmove(p, f->x, n); p += n

int
convPR2M(Passwordreq *f, char *ap, char *key)
{
	int n;
	uchar *p;

	p = (uchar*)ap;
	CHAR(num);
	STRING(old, ANAMELEN);
	STRING(new, ANAMELEN);
	CHAR(changesecret);
	STRING(secret, SECRETLEN);
	n = p - (uchar*)ap;
	if(key)
		encrypt(key, ap, n);
	return n;
}
