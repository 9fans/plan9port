#include "os.h"
#include <mp.h>
#include <libsec.h>
#include <bio.h>

void
main(void)
{
	RSApriv *rsa;
	Biobuf b;
	char *p;
	int n;
	mpint *clr, *enc, *clr2;
	uchar buf[4096];
	uchar *e;
	vlong start;

	fmtinstall('B', mpconv);

	rsa = rsagen(1024, 16, 0);
	if(rsa == nil)
		sysfatal("rsagen");
	Binit(&b, 0, OREAD);
	clr = mpnew(0);
	clr2 = mpnew(0);
	enc = mpnew(0);

	strtomp("123456789abcdef123456789abcdef123456789abcdef123456789abcdef", nil, 16, clr);
	rsaencrypt(&rsa->pub, clr, enc);

	start = nsec();
	for(n = 0; n < 10; n++)
		rsadecrypt(rsa, enc, clr);
	print("%lld\n", nsec()-start);

	start = nsec();
	for(n = 0; n < 10; n++)
		mpexp(enc, rsa->dk, rsa->pub.n, clr2);
	print("%lld\n", nsec()-start);

	if(mpcmp(clr, clr2) != 0)
		print("%B != %B\n", clr, clr2);

	print("> ");
	while(p = Brdline(&b, '\n')){
		n = Blinelen(&b);
		letomp((uchar*)p, n, clr);
		print("clr %B\n", clr);
		rsaencrypt(&rsa->pub, clr, enc);
		print("enc %B\n", enc);
		rsadecrypt(rsa, enc, clr);
		print("clr %B\n", clr);
		n = mptole(clr, buf, sizeof(buf), nil);
		write(1, buf, n);
		print("> ");
	}
}
