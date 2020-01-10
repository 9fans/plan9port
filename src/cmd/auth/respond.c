#include <u.h>
#include <libc.h>
#include <auth.h>

void
usage(void)
{
	fprint(2, "usage: auth/respond 'params' chal\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char buf[128];
	int n;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 2)
		usage();

	memset(buf, 0, sizeof buf);
	n = auth_respond(argv[1], strlen(argv[1]), buf, sizeof buf-1, auth_getkey, "%s", argv[0]);
	if(n < 0)
		sysfatal("auth_respond: %r");
	write(1, buf, n);
	print("\n");
}
