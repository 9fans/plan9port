#include <u.h>
#include <libc.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>
#include "rsa2any.h"

int ssh2;

void
usage(void)
{
	fprint(2, "usage: auth/rsa2ssh [-2] [-c comment] [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	RSApriv *k;
	char *comment;

	fmtinstall('B', mpfmt);
	fmtinstall('[', encodefmt);
	comment = "";
	ARGBEGIN{
	case '2':
		ssh2 = 1;
		break;
	case 'c':
		comment = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if((k = getkey(argc, argv, 0, nil)) == nil)
		sysfatal("%r");

	if(ssh2){
		uchar buf[8192], *p;

		p = buf;
		p = put4(p, 7);
		p = putn(p, "ssh-rsa", 7);
		p = putmp2(p, k->pub.ek);
		p = putmp2(p, k->pub.n);
		print("ssh-rsa %.*[ %s\n", p-buf, buf, comment);
	}else
		print("%d %.10B %.10B %s\n", mpsignif(k->pub.n), k->pub.ek,
			k->pub.n, comment);
	exits(nil);
}
