#include <u.h>
#include <libc.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>
#include "rsa2any.h"

void
usage(void)
{
	fprint(2, "usage: auth/rsa2pub [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	RSApriv *key;
	Attr *a;
	char *s;

	fmtinstall('A', _attrfmt);
	fmtinstall('B', mpfmt);
	quotefmtinstall();

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if((key = getkey(argc, argv, 0, &a)) == nil)
		sysfatal("%r");

	s = smprint("key %A size=%d ek=%lB n=%lB\n",
		a,
		mpsignif(key->pub.n), key->pub.ek, key->pub.n);
	if(s == nil)
		sysfatal("smprint: %r");
	write(1, s, strlen(s));
	exits(nil);
}
