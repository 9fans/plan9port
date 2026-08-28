#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1B2M(char *ap, int n, uchar key[32]);

int
convT2M(Ticket *f, char *ap, char *key)
{
	Authkey *ak;
	uchar *p;
	int n;

	p = (uchar*)ap;
	*p++ = f->num;
	memmove(p, f->chal, CHALLEN), p += CHALLEN;
	memmove(p, f->cuid, ANAMELEN), p += ANAMELEN;
	memmove(p, f->suid, ANAMELEN), p += ANAMELEN;

	switch(f->form){
	case 1:
		memmove(p, f->key, NONCELEN), p += NONCELEN;
		ak = (Authkey*)key;
		if(ak == nil)
			return 0;
		return form1B2M(ap, p - (uchar*)ap, ak->pakkey);
	case 0:
	default:
		memmove(p, f->key, DESKEYLEN), p += DESKEYLEN;
		n = p - (uchar*)ap;
		if(key)
			encrypt(key, ap, n);
		return n;
	}
}
