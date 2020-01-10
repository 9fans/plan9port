/*
 * Deal with duplicated lines in a file
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

#define	SIZE	8000

int	fields	= 0;
int	letters	= 0;
int	linec	= 0;
char	mode;
int	uniq;
char	*b1, *b2;
long	bsize;
Biobuf	fin;
Biobuf	fout;

int	gline(char *buf);
void	pline(char *buf);
int	equal(char *b1, char *b2);
char*	skip(char *s);

void
main(int argc, char *argv[])
{
	int f;

	bsize = SIZE;
	b1 = malloc(bsize);
	b2 = malloc(bsize);
	f = 0;
	while(argc > 1) {
		if(*argv[1] == '-') {
			if(isdigit((uchar)argv[1][1]))
				fields = atoi(&argv[1][1]);
			else
				mode = argv[1][1];
			argc--;
			argv++;
			continue;
		}
		if(*argv[1] == '+') {
			letters = atoi(&argv[1][1]);
			argc--;
			argv++;
			continue;
		}
		f = open(argv[1], 0);
		if(f < 0) {
			fprint(2, "cannot open %s\n", argv[1]);
			exits("open");
		}
		break;
	}
	if(argc > 2) {
		fprint(2, "unexpected argument %s\n", argv[2]);
		exits("arg");
	}
	Binit(&fin, f, OREAD);
	Binit(&fout, 1, OWRITE);

	if(gline(b1))
		exits(0);
	for(;;) {
		linec++;
		if(gline(b2)) {
			pline(b1);
			exits(0);
		}
		if(!equal(b1, b2)) {
			pline(b1);
			linec = 0;
			do {
				linec++;
				if(gline(b1)) {
					pline(b2);
					exits(0);
				}
			} while(equal(b2, b1));
			pline(b2);
			linec = 0;
		}
	}
}

int
gline(char *buf)
{
	char *p;

	p = Brdline(&fin, '\n');
	if(p == 0)
		return 1;
	if(fin.rdline >= bsize-1) {
		fprint(2, "line too long\n");
		exits("too long");
	}
	memmove(buf, p, fin.rdline);
	buf[fin.rdline-1] = 0;
	return 0;
}

void
pline(char *buf)
{

	switch(mode) {

	case 'u':
		if(uniq) {
			uniq = 0;
			return;
		}
		break;

	case 'd':
		if(uniq)
			break;
		return;

	case 'c':
		Bprint(&fout, "%4d ", linec);
	}
	uniq = 0;
	Bprint(&fout, "%s\n", buf);
}

int
equal(char *b1, char *b2)
{
	char c;

	if(fields || letters) {
		b1 = skip(b1);
		b2 = skip(b2);
	}
	for(;;) {
		c = *b1++;
		if(c != *b2++) {
			if(c == 0 && mode == 's')
				return 1;
			return 0;
		}
		if(c == 0) {
			uniq++;
			return 1;
		}
	}
}

char*
skip(char *s)
{
	int nf, nl;

	nf = nl = 0;
	while(nf++ < fields) {
		while(*s == ' ' || *s == '\t')
			s++;
		while(!(*s == ' ' || *s == '\t' || *s == 0) )
			s++;
	}
	while(nl++ < letters && *s != 0)
			s++;
	return s;
}
