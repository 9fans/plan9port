#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mp.h>
#include <libsec.h>

void
usage(void)
{
	fprint(2, "auth/pemdecode section [file]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *buf;
	uchar *bin;
	int fd;
	long n, tot;
	int len;
	char *tag, *file;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	tag = argv[0];
	if(argc == 2)
		file = argv[1];
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
	buf[tot] = 0;
	bin = decodePEM(buf, tag, &len, nil);
	if(bin == nil)
		sysfatal("cannot extract section '%s' from pem", tag);
	if((n=write(1, bin, len)) != len)
		sysfatal("writing %d bytes got %ld: %r", len, n);
	exits(0);
}
