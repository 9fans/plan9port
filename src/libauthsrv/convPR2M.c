#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1B2M(char *ap, int n, uchar key[32]);

int
convPR2M(Passwordreq *f, char *ap, Ticket *t)
{
	uchar *p;
	int n;

	p = (uchar*)ap;
	*p++ = f->num;
	memmove(p, f->old, PASSWDLEN), p += PASSWDLEN;
	memmove(p, f->new, PASSWDLEN), p += PASSWDLEN;
	*p++ = f->changesecret;
	memmove(p, f->secret, SECRETLEN), p += SECRETLEN;
	switch(t->form){
	case 1:
		return form1B2M(ap, p - (uchar*)ap, t->key);
	case 0:
	default:
		n = p - (uchar*)ap;
		encrypt(t->key, ap, n);
		return n;
	}
}
