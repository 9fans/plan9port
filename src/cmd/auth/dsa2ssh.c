#include <u.h>
#include <libc.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>
#include "rsa2any.h"

void
usage(void)
{
	fprint(2, "usage: auth/dsa2ssh [-c comment] [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	DSApriv *k;
	char *comment;
	uchar buf[8192], *p;

	fmtinstall('B', mpfmt);
	fmtinstall('[', encodefmt);
	comment = "";
	ARGBEGIN{
	case 'c':
		comment = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if((k = getdsakey(argc, argv, 0, nil)) == nil)
		sysfatal("%r");

	p = buf;
	p = put4(p, 7);
	p = putn(p, "ssh-dss", 7);
	p = putmp2(p, k->pub.p);
	p = putmp2(p, k->pub.q);
	p = putmp2(p, k->pub.alpha);
	p = putmp2(p, k->pub.key);
	print("ssh-dss %.*[ %s\n", p-buf, buf, comment);
	exits(nil);
}
