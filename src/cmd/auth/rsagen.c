#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

void
usage(void)
{
	fprint(2, "usage: auth/rsagen [-b bits] [-t 'attr=value attr=value ...']\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *s;
	int bits;
	char *tag;
	RSApriv *key;

	bits = 1024;
	tag = nil;
	key = nil;
	fmtinstall('B', mpfmt);

	ARGBEGIN{
	case 'b':
		bits = atoi(EARGF(usage()));
		if(bits == 0)
			usage();
		break;
	case 't':
		tag = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0)
		usage();

	do{
		if(key)
			rsaprivfree(key);
		key = rsagen(bits, 6, 0);
	}while(mpsignif(key->pub.n) != bits);

	s = smprint("key proto=rsa %s%ssize=%d ek=%lB !dk=%lB n=%lB !p=%lB !q=%lB !kp=%lB !kq=%lB !c2=%lB\n",
		tag ? tag : "", tag ? " " : "",
		mpsignif(key->pub.n), key->pub.ek,
		key->dk, key->pub.n, key->p, key->q,
		key->kp, key->kq, key->c2);
	if(s == nil)
		sysfatal("smprint: %r");

	if(write(1, s, strlen(s)) != strlen(s))
		sysfatal("write: %r");

	exits(nil);
}
