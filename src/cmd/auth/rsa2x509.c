#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <mp.h>
#include <libsec.h>
#include "rsa2any.h"

void
usage(void)
{
	fprint(2, "usage: aux/rsa2x509 [-e expireseconds] 'C=US ...CN=xxx' [key]");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int len;
	uchar *cert;
	ulong valid[2];
	RSApriv *key;

	fmtinstall('B', mpfmt);
	fmtinstall('H', encodefmt);

	valid[0] = time(0);
	valid[1] = valid[0] + 3*366*24*60*60;

	ARGBEGIN{
	default:
		usage();
	case 'e':
		valid[1] = valid[0] + strtoul(ARGF(), 0, 10);
		break;
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	if((key = getkey(argc-1, argv+1, 1, nil)) == nil)
		sysfatal("%r");

	cert = X509gen(key, argv[0], valid, &len);
	if(cert == nil)
		sysfatal("X509gen: %r");

	write(1, cert, len);
	exits(0);
}
