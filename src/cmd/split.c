#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <regexp.h>

char	digit[] = "0123456789";
char	*suffix = "";
char	*stem = "x";
char	suff[] = "aa";
char	name[200];
Biobuf	bout;
Biobuf	*output = &bout;

extern int nextfile(void);
extern int matchfile(Resub*);
extern void openf(void);
extern char *fold(char*,int);
extern void usage(void);
extern void badexp(void);

void
main(int argc, char *argv[])
{
	Reprog *exp;
	char *pattern = 0;
	int n = 1000;
	char *line;
	int xflag = 0;
	int iflag = 0;
	Biobuf bin;
	Biobuf *b = &bin;
	char buf[256];

	ARGBEGIN {
	case 'l':
	case 'n':
		n=atoi(EARGF(usage()));
		break;
	case 'e':
		pattern = strdup(EARGF(usage()));
		break;
	case 'f':
		stem = strdup(EARGF(usage()));
		break;
	case 's':
		suffix = strdup(EARGF(usage()));
		break;
	case 'x':
		xflag++;
		break;
	case 'i':
		iflag++;
		break;
	default:
		usage();
		break;

	} ARGEND;

	if(argc < 0 || argc > 1)
		usage();

	if(argc != 0) {
		b = Bopen(argv[0], OREAD);
		if(b == nil) {
			fprint(2, "split: can't open %s: %r\n", argv[0]);
			exits("open");
		}
	} else
		Binit(b, 0, OREAD);

	if(pattern) {
		if(!(exp = regcomp(iflag? fold(pattern,strlen(pattern)): pattern)))
			badexp();
		while((line=Brdline(b,'\n')) != 0) {
			Resub match[2];
			memset(match, 0, sizeof match);
			line[Blinelen(b)-1] = 0;
			if(regexec(exp,iflag?fold(line,Blinelen(b)-1):line,match,2)) {
				if(matchfile(match) && xflag)
					continue;
			} else if(output == 0)
				nextfile();	/* at most once */
			Bwrite(output, line, Blinelen(b)-1);
			Bputc(output, '\n');
		}
	} else {
		int linecnt = n;

		while((line=Brdline(b,'\n')) != 0) {
			if(++linecnt > n) {
				nextfile();
				linecnt = 1;
			}
			Bwrite(output, line, Blinelen(b));
		}

		/*
		 * in case we didn't end with a newline, tack whatever's
		 * left onto the last file
		 */
		while((n = Bread(b, buf, sizeof(buf))) > 0)
			Bwrite(output, buf, n);
	}
	if(b != nil)
		Bterm(b);
	exits(0);
}

int
nextfile(void)
{
	static int canopen = 1;
	if(suff[0] > 'z') {
		if(canopen)
			fprint(2, "split: file %szz not split\n",stem);
		canopen = 0;
	} else {
		strcpy(name, stem);
		strcat(name, suff);
		if(++suff[1] > 'z')
			suff[1] = 'a', ++suff[0];
		openf();
	}
	return canopen;
}

int
matchfile(Resub *match)
{
	if(match[1].s.sp) {
		int len = match[1].e.ep - match[1].s.sp;
		strncpy(name, match[1].s.sp, len);
		strcpy(name+len, suffix);
		openf();
		return 1;
	}
	return nextfile();
}

void
openf(void)
{
	static int fd = 0;
	Bflush(output);
	Bterm(output);
	if(fd > 0)
		close(fd);
	fd = create(name,OWRITE,0666);
	if(fd < 0) {
		fprint(2, "grep: can't create %s: %r\n", name);
		exits("create");
	}
	Binit(output, fd, OWRITE);
}

char *
fold(char *s, int n)
{
	static char *fline;
	static int linesize = 0;
	char *t;

	if(linesize < n+1){
		fline = realloc(fline,n+1);
		linesize = n+1;
	}
	for(t=fline; *t++ = tolower((uchar)*s++); )
		continue;
		/* we assume the 'A'-'Z' only appear as themselves
		 * in a utf encoding.
		 */
	return fline;
}

void
usage(void)
{
	fprint(2, "usage: split [-n num] [-e exp] [-f stem] [-s suff] [-x] [-i] [file]\n");
	exits("usage");
}

void
badexp(void)
{
	fprint(2, "split: bad regular expression\n");
	exits("bad regular expression");
}
