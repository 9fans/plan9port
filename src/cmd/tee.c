/*
 * tee-- pipe fitting
 */

#include <u.h>
#include <libc.h>

int	uflag;
int	aflag;
int	openf[100];

char in[8192];

int	intignore(void*, char*);

void
main(int argc, char **argv)
{
	int i;
	int r, n;

	ARGBEGIN {
	case 'a':
		aflag++;
		break;

	case 'i':
		atnotify(intignore, 1);
		break;

	case 'u':
		uflag++;
		/* uflag is ignored and undocumented; it's a relic from Unix */
		break;

	default:
		fprint(2, "usage: tee [-ai] [file ...]\n");
		exits("usage");
	} ARGEND

	USED(argc);
	n = 0;
	while(*argv) {
		if(aflag) {
			openf[n] = open(argv[0], OWRITE);
			if(openf[n] < 0)
				openf[n] = create(argv[0], OWRITE, 0666);
			seek(openf[n], 0L, 2);
		} else
			openf[n] = create(argv[0], OWRITE, 0666);
		if(openf[n] < 0) {
			fprint(2, "tee: cannot open %s: %r\n", argv[0]);
		} else
			n++;
		argv++;
	}
	openf[n++] = 1;

	for(;;) {
		r = read(0, in, sizeof in);
		if(r <= 0)
			exits(nil);
		for(i=0; i<n; i++)
			write(openf[i], in, r);
	}
}

int
intignore(void *a, char *msg)
{
	USED(a);
	if(strcmp(msg, "interrupt") == 0)
		return 1;
	return 0;
}
