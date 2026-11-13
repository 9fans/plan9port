#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1B2M(char *ap, int n, uchar key[32]);

int
convA2M(Authenticator *f, char *ap, Ticket *t)
{
	uchar *p;
	int n;

	p = (uchar*)ap;
	*p++ = f->num;
	memmove(p, f->chal, CHALLEN), p += CHALLEN;
	switch(t->form){
	case 1:
		memmove(p, f->rand, NONCELEN), p += NONCELEN;
		return form1B2M(ap, p - (uchar*)ap, t->key);
	case 0:
	default:
		p[0] = f->id;
		p[1] = f->id >> 8;
		p[2] = f->id >> 16;
		p[3] = f->id >> 24;
		p += 4;
		n = p - (uchar*)ap;
		encrypt(t->key, ap, n);
		return n;
	}
}
