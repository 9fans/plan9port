#include "os.h"
#include <mp.h>
#include <libsec.h>

void
main(void)
{
	EGpriv *sk;
	mpint *m, *gamma, *delta, *in, *out;
	int plen, shift;

	fmtinstall('B', mpconv);

	sk = egprivalloc();
	sk->pub.p = uitomp(2357, nil);
	sk->pub.alpha = uitomp(2, nil);
	sk->pub.key = uitomp(1185, nil);
	sk->secret = uitomp(1751, nil);

	m = uitomp(2035, nil);

	plen = mpsignif(sk->pub.p)+1;
	shift = ((plen+Dbits-1)/Dbits)*Dbits;
	gamma = uitomp(1430, nil);
	delta = uitomp(697, nil);
	out = mpnew(0);
	in = mpnew(0);
	mpleft(gamma, shift, in);
	mpadd(delta, in, in);
	egdecrypt(sk, in, out);

	if(mpcmp(m, out) != 0)
		print("decrypt failed to recover message\n");
}
