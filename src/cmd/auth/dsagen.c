#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

void
usage(void)
{
	fprint(2, "usage: auth/dsagen [-t 'attr=value attr=value ...']\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *s;
	char *tag;
	DSApriv *key;

	tag = nil;
	key = nil;
	fmtinstall('B', mpfmt);

	ARGBEGIN{
	case 't':
		tag = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0)
		usage();

	key = dsagen(nil);

	s = smprint("key proto=dsa %s%sp=%lB q=%lB alpha=%lB key=%lB !secret=%lB\n",
		tag ? tag : "", tag ? " " : "",
		key->pub.p, key->pub.q, key->pub.alpha, key->pub.key,
		key->secret);
	if(s == nil)
		sysfatal("smprint: %r");

	if(write(1, s, strlen(s)) != strlen(s))
		sysfatal("write: %r");

	exits(nil);
}
