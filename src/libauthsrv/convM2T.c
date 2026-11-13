#include <u.h>
#include <libc.h>
#include <authsrv.h>

extern int form1check(char *ap, int n);
extern int form1M2B(char *ap, int n, uchar key[32]);

void
convM2T(char *ap, Ticket *f, char *key)
{
	Authkey *ak;
	uchar buf[MAXTICKETLEN], *p;
	int form, n;

	memset(f, 0, sizeof(*f));
	form = form1check(ap, 8);
	if(form >= 0){
		f->form = 1;
		n = MAXTICKETLEN;
		memmove(buf, ap, n);
		ak = (Authkey*)key;
		if(ak != nil)
			form1M2B((char*)buf, n, ak->pakkey);
	}else{
		f->form = 0;
		n = TICKETLEN;
		memmove(buf, ap, n);
		if(key)
			decrypt(key, (char*)buf, n);
	}

	p = buf;
	f->num = *p++;
	memmove(f->chal, p, CHALLEN), p += CHALLEN;
	memmove(f->cuid, p, ANAMELEN), p += ANAMELEN;
	f->cuid[ANAMELEN-1] = 0;
	memmove(f->suid, p, ANAMELEN), p += ANAMELEN;
	f->suid[ANAMELEN-1] = 0;
	if(f->form == 1)
		memmove(f->key, p, NONCELEN);
	else
		memmove(f->key, p, DESKEYLEN);
}
