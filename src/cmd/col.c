/* col - eliminate reverse line feeds */
#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>

enum {
	ESC	= '\033',
	RLF	= '\013',

	PL	= 256,
	LINELN	= 800,

	Tabstop	= 8,		/* must be power of 2 */
};

static int bflag, xflag, fflag;
static int cp, lp;
static int half;
static int ll, llh, mustwr;
static int pcp = 0;

static char *page[PL];
static char *line;
static char lbuff[LINELN];
static Biobuf bin, bout;

void	emit(char *s, int lineno);
void	incr(void), decr(void);
void	outc(Rune);

static void
usage(void)
{
	fprint(2, "usage: %s [-bfx]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, lno;
	long ch;
	Rune c;

	ARGBEGIN{
	case 'b':
		bflag++;
		break;
	case 'f':
		fflag++;
		break;
	case 'x':
		xflag++;
		break;
	default:
		usage();
	}ARGEND;

	for (ll=0; ll < PL; ll++)
		page[ll] = nil;

	cp = 0;
	ll = 0;
	mustwr = PL;
	line = lbuff;

	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);
	while ((ch = Bgetrune(&bin)) != Beof) {
		c = ch;
		switch (c) {
		case '\n':
			incr();
			incr();
			cp = 0;
			break;

		case '\0':
			break;

		case ESC:
			c = Bgetrune(&bin);
			switch (c) {
			case '7':	/* reverse full line feed */
				decr();
				decr();
				break;

			case '8':	/* reverse half line feed */
				if (fflag)
					decr();
				else
					if (--half < -1) {
						decr();
						decr();
						half += 2;
					}
				break;

			case '9':	/* forward half line feed */
				if (fflag)
					incr();
				else
					if (++half > 0) {
						incr();
						incr();
						half -= 2;
					}
				break;
			}
			break;

		case RLF:
			decr();
			decr();
			break;

		case '\r':
			cp = 0;
			break;

		case '\t':
			cp = (cp + Tabstop) & -Tabstop;
			break;

		case '\b':
			if (cp > 0)
				cp--;
			break;

		case ' ':
			cp++;
			break;

		default:
			if (!isascii(c) || isprint(c)) {
				outc(c);
				cp++;
			}
			break;
		}
	}

	for (i=0; i < PL; i++) {
		lno = (mustwr+i) % PL;
		if (page[lno] != 0)
			emit(page[lno], mustwr+i-PL);
	}
	emit(" ", (llh + 1) & -2);
	exits(0);
}

void
outc(Rune c)
{
	if (lp > cp) {
		line = lbuff;
		lp = 0;
	}

	while (lp < cp) {
		switch (*line) {
		case '\0':
			*line = ' ';
			lp++;
			break;
		case '\b':
			lp--;
			break;
		default:
			lp++;
			break;
		}
		line++;
	}
	while (*line == '\b')
		line += 2;
	if (bflag || *line == '\0' || *line == ' ')
		cp += runetochar(line, &c) - 1;
	else {
		char c1, c2, c3;

		c1 = *++line;
		*line++ = '\b';
		c2 = *line;
		*line++ = c;
		while (c1) {
			c3 = *line;
			*line++ = c1;
			c1 = c2;
			c2 = c3;
		}
		lp = 0;
		line = lbuff;
	}
}

void
store(int lno)
{
	lno %= PL;
	if (page[lno] != nil)
		free(page[lno]);
	page[lno] = malloc((unsigned)strlen(lbuff) + 2);
	if (page[lno] == nil)
		sysfatal("out of memory");
	strcpy(page[lno], lbuff);
}

void
fetch(int lno)
{
	char *p;

	lno %= PL;
	p = lbuff;
	while (*p)
		*p++ = '\0';
	line = lbuff;
	lp = 0;
	if (page[lno])
		strcpy(line, page[lno]);
}

void
emit(char *s, int lineno)
{
	int ncp;
	char *p;
	static int cline = 0;

	if (*s) {
		while (cline < lineno - 1) {
			Bputc(&bout, '\n');
			pcp = 0;
			cline += 2;
		}
		if (cline != lineno) {
			Bputc(&bout, ESC);
			Bputc(&bout, '9');
			cline++;
		}
		if (pcp)
			Bputc(&bout, '\r');
		pcp = 0;
		p = s;
		while (*p) {
			ncp = pcp;
			while (*p++ == ' ')
				if ((++ncp & 7) == 0 && !xflag) {
					pcp = ncp;
					Bputc(&bout, '\t');
				}
			if (!*--p)
				break;
			while (pcp < ncp) {
				Bputc(&bout, ' ');
				pcp++;
			}
			Bputc(&bout, *p);
			if (*p++ == '\b')
				pcp--;
			else
				pcp++;
		}
	}
}

void
incr(void)
{
	int lno;

	store(ll++);
	if (ll > llh)
		llh = ll;
	lno = ll % PL;
	if (ll >= mustwr && page[lno]) {
		emit(page[lno], ll - PL);
		mustwr++;
		free(page[lno]);
		page[lno] = nil;
	}
	fetch(ll);
}

void
decr(void)
{
	if (ll > mustwr - PL) {
		store(ll--);
		fetch(ll);
	}
}
