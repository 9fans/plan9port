#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

static int
digestfmt(Fmt *fmt)
{
	char buf[SHA1dlen*2+1];
	uchar *p;
	int i;

	p = va_arg(fmt->args, uchar*);
	for(i=0; i<SHA1dlen; i++)
		sprint(buf+2*i, "%.2ux", p[i]);
	return fmtstrcpy(fmt, buf);
}

static void
sum(int fd, char *name)
{
	int n;
	uchar buf[8192], digest[SHA1dlen];
	DigestState *s;

	s = sha1(nil, 0, nil, nil);
	while((n = read(fd, buf, sizeof buf)) > 0)
		sha1(buf, n, nil, s);
	sha1(nil, 0, digest, s);
	if(name == nil)
		print("%M\n", digest);
	else
		print("%M\t%s\n", digest, name);
}

void
main(int argc, char *argv[])
{
	int i, fd;

	ARGBEGIN{
	default:
		fprint(2, "usage: sha1sum [file...]\n");
		exits("usage");
	}ARGEND

	fmtinstall('M', digestfmt);

	if(argc == 0)
		sum(0, nil);
	else for(i = 0; i < argc; i++){
		fd = open(argv[i], OREAD);
		if(fd < 0){
			fprint(2, "sha1sum: can't open %s: %r\n", argv[i]);
			continue;
		}
		sum(fd, argv[i]);
		close(fd);
	}
	exits(nil);
}
