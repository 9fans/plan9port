#include <u.h>
#include <libc.h>

void
usage(void)
{
	fprint(2, "usage: fsize file...\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i;
	Dir *d;

	ARGBEGIN{
	default:
		usage();
	}ARGEND
	if(argc == 0)
		usage();

	for(i=0; i<argc; i++){
		if((d = dirstat(argv[i])) == nil)
			fprint(2, "dirstat %s: %r", argv[i]);
		else{
			print("%s: %lld\n", argv[i], d->length);
			free(d);
		}
	}
}
