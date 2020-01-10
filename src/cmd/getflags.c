#include <u.h>
#include <libc.h>

void
usage(void)
{
	print("status=usage\n");
	exits(0);
}

char*
findarg(char *flags, Rune r)
{
	char *p;
	Rune rr;

	for(p=flags; p!=(char*)1; p=strchr(p, ',')+1){
		chartorune(&rr, p);
		if(rr == r)
			return p;
	}
	return nil;
}

int
countargs(char *p)
{
	int n;

	n = 1;
	while(*p == ' ')
		p++;
	for(; *p && *p != ','; p++)
		if(*p == ' ' && *(p-1) != ' ')
			n++;
	return n;
}

void
main(int argc, char *argv[])
{
	char *flags, *p, buf[512];
	int i, n;
	Fmt fmt;

	doquote = needsrcquote;
	quotefmtinstall();
	argv0 = argv[0];	/* for sysfatal */

	flags = getenv("flagfmt");
	if(flags == nil){
		fprint(2, "$flagfmt not set\n");
		print("exit 'missing flagfmt'");
		exits(0);
	}

	fmtfdinit(&fmt, 1, buf, sizeof buf);
	for(p=flags; p!=(char*)1; p=strchr(p, ',')+1)
		fmtprint(&fmt, "flag%.1s=()\n", p);
	ARGBEGIN{
	default:
		if((p = findarg(flags, ARGC())) == nil)
			usage();
		p += runelen(ARGC());
		if(*p == ',' || *p == 0){
			fmtprint(&fmt, "flag%C=1\n", ARGC());
			break;
		}
		n = countargs(p);
		fmtprint(&fmt, "flag%C=(", ARGC());
		for(i=0; i<n; i++)
			fmtprint(&fmt, "%s%q", i ? " " : "", EARGF(usage()));
		fmtprint(&fmt, ")\n");
	}ARGEND

	fmtprint(&fmt, "*=(");
	for(i=0; i<argc; i++)
		fmtprint(&fmt, "%s%q", i ? " " : "", argv[i]);
	fmtprint(&fmt, ")\n");
	fmtprint(&fmt, "status=''\n");
	fmtfdflush(&fmt);
	exits(0);
}
