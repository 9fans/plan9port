#include <u.h>
#include <libc.h>
#include <authsrv.h>

#define	CHAR(x)		*p++ = f->x
#define	SHORT(x)	p[0] = f->x; p[1] = f->x>>8; p += 2
#define	VLONG(q)	p[0] = (q); p[1] = (q)>>8; p[2] = (q)>>16; p[3] = (q)>>24; p += 4
#define	LONG(x)		VLONG(f->x)
#define	STRING(x,n)	memmove(p, f->x, n); p += n

int
convTR2M(Ticketreq *f, char *ap)
{
	int n;
	uchar *p;

	p = (uchar*)ap;
	CHAR(type);
	STRING(authid, 28);	/* BUG */
	STRING(authdom, DOMLEN);
	STRING(chal, CHALLEN);
	STRING(hostid, 28);	/* BUG */
	STRING(uid, 28);	/* BUG */
	n = p - (uchar*)ap;
	return n;
}
