#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include <libsec.h>

void
usage(void)
{
	fprint(2, "auth/asn12rsa [-t tag] [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *s;
	uchar *buf;
	int fd;
	long n, tot;
	char *tag, *file;
	RSApriv *key;

	fmtinstall('B', mpfmt);

	tag = nil;
	ARGBEGIN{
	case 't':
		tag = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0 && argc != 1)
		usage();

	if(argc == 1)
		file = argv[0];
	else
		file = "/dev/stdin";

	if((fd = open(file, OREAD)) < 0)
		sysfatal("open %s: %r", file);
	buf = nil;
	tot = 0;
	for(;;){
		buf = realloc(buf, tot+8192);
		if(buf == nil)
			sysfatal("realloc: %r");
		if((n = read(fd, buf+tot, 8192)) < 0)
			sysfatal("read: %r");
		if(n == 0)
			break;
		tot += n;
	}

	key = asn1toRSApriv(buf, tot);
	if(key == nil)
		sysfatal("couldn't parse asn1 key");

	s = smprint("key proto=rsa %s%ssize=%d ek=%lB !dk=%lB n=%lB !p=%lB !q=%lB !kp=%lB !kq=%lB !c2=%lB\n",
		tag ? tag : "", tag ? " " : "",
		mpsignif(key->pub.n), key->pub.ek,
		key->dk, key->pub.n, key->p, key->q,
		key->kp, key->kq, key->c2);
	if(s == nil)
		sysfatal("smprint: %r");
	write(1, s, strlen(s));
	exits(0);
}
