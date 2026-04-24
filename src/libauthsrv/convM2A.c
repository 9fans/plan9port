#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1M2B(char *ap, int n, uchar key[32]);

void
convM2A(char *ap, Authenticator *f, Ticket *t)
{
	uchar buf[MAXAUTHENTLEN], *p;

	memset(f, 0, sizeof(*f));
	if(t->form == 1){
		memmove(buf, ap, MAXAUTHENTLEN);
		if(form1M2B((char*)buf, MAXAUTHENTLEN, t->key) < 0)
			return;
	}else{
		memmove(buf, ap, AUTHENTLEN);
		decrypt(t->key, (char*)buf, AUTHENTLEN);
	}

	p = buf;
	f->num = *p++;
	memmove(f->chal, p, CHALLEN), p += CHALLEN;
	if(t->form == 1)
		memmove(f->rand, p, NONCELEN);
	else
		f->id = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}
