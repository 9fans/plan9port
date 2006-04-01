#include <u.h>
#include <libc.h>
#include <bio.h>

long	count[1<<16];
Biobuf	bout;

void	freq(int, char*);
long	flag;
enum
{
	Fdec	= 1<<0,
	Fhex	= 1<<1,
	Foct	= 1<<2,
	Fchar	= 1<<3,
	Frune	= 1<<4
};

void
main(int argc, char *argv[])
{
	int f, i;

	flag = 0;
	Binit(&bout, 1, OWRITE);
	ARGBEGIN{
	default:
		fprint(2, "freq: unknown option %c\n", ARGC());
		exits("usage");
	case 'd':
		flag |= Fdec;
		break;
	case 'x':
		flag |= Fhex;
		break;
	case 'o':
		flag |= Foct;
		break;
	case 'c':
		flag |= Fchar;
		break;
	case 'r':
		flag |= Frune;
		break;
	}ARGEND
	if((flag&(Fdec|Fhex|Foct|Fchar)) == 0)
		flag |= Fdec | Fhex | Foct | Fchar;
	if(argc < 1) {
		freq(0, "-");
		exits(0);
	}
	for(i=0; i<argc; i++) {
		f = open(argv[i], 0);
		if(f < 0) {
			fprint(2, "cannot open %s\n", argv[i]);
			continue;
		}
		freq(f, argv[i]);
		close(f);
	}
	exits(0);
}

void
freq(int f, char *s)
{
	Biobuf bin;
	long c, i;

	memset(count, 0, sizeof(count));
	Binit(&bin, f, OREAD);
	if(flag & Frune) {
		for(;;) {
			c = Bgetrune(&bin);
			if(c < 0)
				break;
			count[c]++;
		}
	} else {
		for(;;) {
			c = Bgetc(&bin);
			if(c < 0)
				break;
			count[c]++;
		}
	}
	Bterm(&bin);
	if(c != Beof)
		fprint(2, "freq: read error on %s\n", s);

	for(i=0; i<nelem(count); i++) {
		if(count[i] == 0)
			continue;
		if(flag & Fdec)
			Bprint(&bout, "%3ld ", i);
		if(flag & Foct)
			Bprint(&bout, "%.3lo ", i);
		if(flag & Fhex)
			Bprint(&bout, "%.2lx ", i);
		if(flag & Fchar) {
			if(i <= 0x20 ||
			   i >= 0x7f && i < 0xa0 ||
			   i > 0xff && !(flag & Frune))
				Bprint(&bout, "- ");
			else
				Bprint(&bout, "%C ", (int)i);
		}
		Bprint(&bout, "%8ld\n", count[i]);
	}
	Bflush(&bout);
}
