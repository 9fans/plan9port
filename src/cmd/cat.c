#include <u.h>
#include <libc.h>

void
cat(int f, char *s)
{
	char buf[8192];
	long n;

	while((n=read(f, buf, (long)sizeof buf))>0)
		if(write(1, buf, n)!=n)
			sysfatal("write error copying %s: %r", s);
	if(n < 0)
		sysfatal("error reading %s: %r", s);
}

void
main(int argc, char *argv[])
{
	int f, i;

	argv0 = "cat";
	if(argc == 1)
		cat(0, "<stdin>");
	else for(i=1; i<argc; i++){
		f = open(argv[i], OREAD);
		if(f < 0)
			sysfatal("can't open %s: %r", argv[i]);
		else{
			cat(f, argv[i]);
			close(f);
		}
	}
	exits(0);
}
