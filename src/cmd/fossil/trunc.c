#include <u.h>
#include <libc.h>

void
main(int argc, char **argv)
{
	Dir d;

	if(argc != 3){
		fprint(2, "usage: trunc file size\n");
		exits("usage");
	}

	nulldir(&d);
	d.length = strtoull(argv[2], 0, 0);
	if(dirwstat(argv[1], &d) < 0)
		sysfatal("dirwstat: %r");
	exits(0);
}
