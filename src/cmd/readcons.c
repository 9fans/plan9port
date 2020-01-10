#include <u.h>
#include <libc.h>

void
usage(void)
{
	fprint(2, "usage: readcons [-s] [-d default] prompt\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *def, *p;
	int secret;

	def = nil;
	secret = 0;
	ARGBEGIN{
	case 's':
		secret = 1;
		break;
	case 'd':
		def = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	p = readcons(argv[0], def, secret);
	if(p == nil)
		exits("readcons");
	print("%s\n", p);
	exits(0);
}
