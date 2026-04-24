#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <libsec.h>

/*
 * New ticket format: the reply protector/type is replaced by an 8-byte
 * signature and a 4-byte counter forming the 12-byte nonce for
 * chacha20/poly1305 encryption. A 16-byte tag is appended.
 */

static struct {
	char	num;
	char	sig[8];
} form1sig[] = {
	AuthPass,	"form1 PR",
	AuthTs,		"form1 Ts",
	AuthTc,		"form1 Tc",
	AuthAs,		"form1 As",
	AuthAc,		"form1 Ac",
	AuthTp,		"form1 Tp",
	AuthHr,		"form1 Hr",
};

int
form1check(char *ap, int n)
{
	int i;

	if(n < 8)
		return -1;
	for(i = 0; i < nelem(form1sig); i++)
		if(memcmp(form1sig[i].sig, ap, 8) == 0)
			return form1sig[i].num;
	return -1;
}

int
form1B2M(char *ap, int n, uchar key[32])
{
	static u32int counter;
	Chachastate s;
	uchar *p;
	int i;

	for(i = nelem(form1sig)-1; i >= 0; i--)
		if(form1sig[i].num == *ap)
			break;
	if(i < 0)
		abort();

	p = (uchar*)ap + 12;
	memmove(p, ap+1, --n);

	memmove(ap, form1sig[i].sig, 8);
	i = counter++;
	ap[8] = i;
	ap[9] = i >> 8;
	ap[10] = i >> 16;
	ap[11] = i >> 24;

	setupChachastate(&s, key, 32, (uchar*)ap, 12, 20);
	ccpoly_encrypt(p, n, nil, 0, p+n, &s);
	return 12+16+n;
}

int
form1M2B(char *ap, int n, uchar key[32])
{
	Chachastate s;
	uchar *p;
	int num;

	num = form1check(ap, n);
	if(num < 0)
		return -1;
	n -= 12+16;
	if(n <= 0)
		return -1;

	p = (uchar*)ap + 12;
	setupChachastate(&s, key, 32, (uchar*)ap, 12, 20);
	if(ccpoly_decrypt(p, n, nil, 0, p+n, &s))
		return -1;

	memmove(ap+1, p, n);
	ap[0] = num;
	return n+1;
}
