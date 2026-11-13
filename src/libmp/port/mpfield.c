#include "os.h"
#include <mp.h>
#include "dat.h"

mpint*
mpfield(mpint *N)
{
	Mfield *f;

	if(N == nil || N->flags & (MPfield|MPstatic))
		return N;
	if((f = cnfield(N)) != nil)
		goto Exchange;
	if((f = gmfield(N)) != nil)
		goto Exchange;
	return N;
Exchange:
	setmalloctag(f, getcallerpc(&N));
	mpfree(N);
	return (mpint*)f;
}
