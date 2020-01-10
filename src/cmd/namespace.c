#include <u.h>
#include <libc.h>

void
usage(void)
{
	fprint(2, "usage: namespace\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *ns;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc)
		usage();

	ns = getns();
	if(ns == nil){
		fprint(2, "namespace: %r\n");
		exits("namespace");
	}

	print("%s\n", ns);
	exits(0);
}
