#include "os.h"
#include <mp.h>
#include "dat.h"

void
mpmod(mpint *x, mpint *n, mpint *r)
{
	int sign;
	mpint *ns;

	sign = x->sign;
	ns = sign < 0 && n == r ? mpcopy(n) : n;
	if((n->flags & MPfield) == 0
	|| ((Mfield*)n)->reduce((Mfield*)n, x, r) != 0)
		mpdiv(x, n, nil, r);
	if(sign < 0){
		mpmagsub(ns, r, r);
		if(ns != n)
			mpfree(ns);
	}
}
