#include	<u.h>
#include 	<libc.h>
#include	<bio.h>

Biobuf	*fin;
Biobuf	fout;

#define	MINSPAN		6		/* Min characters in string */

#define BUFSIZE		70

void stringit(char *);
#undef isprint
#define isprint risprint
int isprint(Rune);

void
main(int argc, char **argv)
{
	int i;

	Binit(&fout, 1, OWRITE);
	if(argc < 2) {
		stringit("/dev/stdin");
		exits(0);
	}

	for(i = 1; i < argc; i++) {
		if(argc > 2)
			print("%s:\n", argv[i]);

		stringit(argv[i]);
	}

	exits(0);
}

void
stringit(char *str)
{
	long posn, start;
	int cnt = 0;
	long c;

	Rune buf[BUFSIZE];

	if ((fin = Bopen(str, OREAD)) == 0) {
		perror("open");
		return;
	}

	start = 0;
	posn = Boffset(fin);
	while((c = Bgetrune(fin)) >= 0) {
		if(isprint(c)) {
			if(start == 0)
				start = posn;
			buf[cnt++] = c;
			if(cnt == BUFSIZE-1) {
				buf[cnt] = 0;
				Bprint(&fout, "%8ld: %S ...\n", start, buf);
				start = 0;
				cnt = 0;
			}
		} else {
			 if(cnt >= MINSPAN) {
				buf[cnt] = 0;
				Bprint(&fout, "%8ld: %S\n", start, buf);
			}
			start = 0;
			cnt = 0;
		}
		posn = Boffset(fin);
	}

	if(cnt >= MINSPAN){
		buf[cnt] = 0;
		Bprint(&fout, "%8ld: %S\n", start, buf);
	}
	Bterm(fin);
}

int
isprint(Rune r)
{
	if ((r >= ' ' && r <0x7f) || r > 0xA0)
		return 1;
	else
		return 0;
}
