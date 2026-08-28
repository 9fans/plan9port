#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1M2B(char *ap, int n, uchar key[32]);

void
convM2PR(char *ap, Passwordreq *f, Ticket *t)
{
	uchar buf[MAXPASSREQLEN], *p;

	memset(f, 0, sizeof(*f));
	if(t->form == 1){
		memmove(buf, ap, MAXPASSREQLEN);
		if(form1M2B((char*)buf, MAXPASSREQLEN, t->key) < 0)
			return;
	}else{
		memmove(buf, ap, PASSREQLEN);
		decrypt(t->key, (char*)buf, PASSREQLEN);
	}

	p = buf;
	f->num = *p++;
	memmove(f->old, p, PASSWDLEN), p += PASSWDLEN;
	f->old[PASSWDLEN-1] = 0;
	memmove(f->new, p, PASSWDLEN), p += PASSWDLEN;
	f->new[PASSWDLEN-1] = 0;
	f->changesecret = *p++;
	memmove(f->secret, p, SECRETLEN);
	f->secret[SECRETLEN-1] = 0;
}
