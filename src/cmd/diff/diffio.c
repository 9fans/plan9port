#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "diff.h"

struct line {
	int	serial;
	int	value;
};
extern struct line *file[2];
extern int len[2];
extern long *ixold, *ixnew;
extern int *J;

static Biobuf *input[2];
static char *file1, *file2;
static int firstchange;

#define MAXLINELEN	4096
#define MIN(x, y)	((x) < (y) ? (x): (y))

static int
readline(Biobuf *bp, char *buf)
{
	int c;
	char *p, *e;

	p = buf;
	e = p + MAXLINELEN-1;
	do {
		c = Bgetc(bp);
		if (c < 0) {
			if (p == buf)
				return -1;
			break;
		}
		if (c == '\n')
			break;
		*p++ = c;
	} while (p < e);
	*p = 0;
	if (c != '\n' && c >= 0) {
		do c = Bgetc(bp);
		while (c >= 0 && c != '\n');
	}
	return p - buf;
}

#define HALFLONG 16
#define low(x)	(x&((1L<<HALFLONG)-1))
#define high(x)	(x>>HALFLONG)

/*
 * hashing has the effect of
 * arranging line in 7-bit bytes and then
 * summing 1-s complement in 16-bit hunks
 */
static int
readhash(Biobuf *bp, char *buf)
{
	long sum;
	unsigned shift;
	char *p;
	int len, space;

	sum = 1;
	shift = 0;
	if ((len = readline(bp, buf)) == -1)
		return 0;
	p = buf;
	switch(bflag)	/* various types of white space handling */
	{
	case 0:
		while (len--) {
			sum += (long)*p++ << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	case 1:
		/*
		 * coalesce multiple white-space
		 */
		for (space = 0; len--; p++) {
			if (isspace((uchar)*p)) {
				space++;
				continue;
			}
			if (space) {
				shift += 7;
				space = 0;
			}
			sum += (long)*p << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	default:
		/*
		 * strip all white-space
		 */
		while (len--) {
			if (isspace((uchar)*p)) {
				p++;
				continue;
			}
			sum += (long)*p++ << (shift &= (HALFLONG-1));
			shift += 7;
		}
		break;
	}
	sum = low(sum) + high(sum);
	return ((short)low(sum) + (short)high(sum));
}

Biobuf *
prepare(int i, char *arg)
{
	struct line *p;
	int j, h;
	Biobuf *bp;
	char *cp, buf[MAXLINELEN];
	int nbytes;
	Rune r;

	bp = Bopen(arg, OREAD);
	if (!bp) {
		panic(mflag ? 0: 2, "cannot open %s: %r\n", arg);
		return 0;
	}
	if (binary)
		return bp;
	nbytes = Bread(bp, buf, MIN(1024, MAXLINELEN));
	if (nbytes > 0) {
		cp = buf;
		while (cp < buf+nbytes-UTFmax) {
			/*
			 * heuristic for a binary file in the
			 * brave new UNICODE world
			 */
			cp += chartorune(&r, cp);
			if (r == 0 || (r > 0x7f && r <= 0xa0)) {
				binary++;
				return bp;
			}
		}
		Bseek(bp, 0, 0);
	}
	p = MALLOC(struct line, 3);
	for (j = 0; h = readhash(bp, buf); p[j].value = h)
		p = REALLOC(p, struct line, (++j+3));
	len[i] = j;
	file[i] = p;
	input[i] = bp;			/*fix*/
	if (i == 0) {			/*fix*/
		file1 = arg;
		firstchange = 0;
	}
	else
		file2 = arg;
	return bp;
}

static int
squishspace(char *buf)
{
	char *p, *q;
	int space;

	for (space = 0, q = p = buf; *q; q++) {
		if (isspace((uchar)*q)) {
			space++;
			continue;
		}
		if (space && bflag == 1) {
			*p++ = ' ';
			space = 0;
		}
		*p++ = *q;
	}
	*p = 0;
	return p - buf;
}

/*
 * need to fix up for unexpected EOF's
 */
void
check(Biobuf *bf, Biobuf *bt)
{
	int f, t, flen, tlen;
	char fbuf[MAXLINELEN], tbuf[MAXLINELEN];

	ixold[0] = ixnew[0] = 0;
	for (f = t = 1; f < len[0]; f++) {
		flen = readline(bf, fbuf);
		ixold[f] = ixold[f-1] + flen + 1;		/* ftell(bf) */
		if (J[f] == 0)
			continue;
		do {
			tlen = readline(bt, tbuf);
			ixnew[t] = ixnew[t-1] + tlen + 1;	/* ftell(bt) */
		} while (t++ < J[f]);
		if (bflag) {
			flen = squishspace(fbuf);
			tlen = squishspace(tbuf);
		}
		if (flen != tlen || strcmp(fbuf, tbuf))
			J[f] = 0;
	}
	while (t < len[1]) {
		tlen = readline(bt, tbuf);
		ixnew[t] = ixnew[t-1] + tlen + 1;	/* fseek(bt) */
		t++;
	}
}

static void
range(int a, int b, char *separator)
{
	Bprint(&stdout, "%d", a > b ? b: a);
	if (a < b)
		Bprint(&stdout, "%s%d", separator, b);
}

static void
fetch(long *f, int a, int b, Biobuf *bp, char *s)
{
	char buf[MAXLINELEN];
	int maxb;

	if(a <= 1)
		a = 1;
	if(bp == input[0])
		maxb = len[0];
	else
		maxb = len[1];
	if(b > maxb)
		b = maxb;
	if(a > maxb)
		return;
	Bseek(bp, f[a-1], 0);
	while (a++ <= b) {
		readline(bp, buf);
		Bprint(&stdout, "%s%s\n", s, buf);
	}
}

typedef struct Change Change;
struct Change
{
	int a;
	int b;
	int c;
	int d;
};

Change *changes;
int nchanges;

void
change(int a, int b, int c, int d)
{
	char verb;
	char buf[4];
	Change *ch;

	if (a > b && c > d)
		return;
	anychange = 1;
	if (mflag && firstchange == 0) {
		if(mode) {
			buf[0] = '-';
			buf[1] = mode;
			buf[2] = ' ';
			buf[3] = '\0';
		} else {
			buf[0] = '\0';
		}
		Bprint(&stdout, "diff %s%s %s\n", buf, file1, file2);
		firstchange = 1;
	}
	verb = a > b ? 'a': c > d ? 'd': 'c';
	switch(mode) {
	case 'e':
		range(a, b, ",");
		Bputc(&stdout, verb);
		break;
	case 0:
		range(a, b, ",");
		Bputc(&stdout, verb);
		range(c, d, ",");
		break;
	case 'n':
		Bprint(&stdout, "%s:", file1);
		range(a, b, ",");
		Bprint(&stdout, " %c ", verb);
		Bprint(&stdout, "%s:", file2);
		range(c, d, ",");
		break;
	case 'f':
		Bputc(&stdout, verb);
		range(a, b, " ");
		break;
	case 'c':
	case 'a':
		if(nchanges%1024 == 0)
			changes = erealloc(changes, (nchanges+1024)*sizeof(changes[0]));
		ch = &changes[nchanges++];
		ch->a = a;
		ch->b = b;
		ch->c = c;
		ch->d = d;
		return;
	}
	Bputc(&stdout, '\n');
	if (mode == 0 || mode == 'n') {
		fetch(ixold, a, b, input[0], "< ");
		if (a <= b && c <= d)
			Bprint(&stdout, "---\n");
	}
	fetch(ixnew, c, d, input[1], mode == 0 || mode == 'n' ? "> ": "");
	if (mode != 0 && mode != 'n' && c <= d)
		Bprint(&stdout, ".\n");
}

enum
{
	Lines = 3		/* number of lines of context shown */
};

int
changeset(int i)
{
	while(i<nchanges && changes[i].b+1+2*Lines > changes[i+1].a)
		i++;
	if(i<nchanges)
		return i+1;
	return nchanges;
}

void
flushchanges(void)
{
	int a, b, c, d, at;
	int i, j;

	if(nchanges == 0)
		return;

	for(i=0; i<nchanges; ){
		j = changeset(i);
		a = changes[i].a-Lines;
		b = changes[j-1].b+Lines;
		c = changes[i].c-Lines;
		d = changes[j-1].d+Lines;
		if(a < 1)
			a = 1;
		if(c < 1)
			c = 1;
		if(b > len[0])
			b = len[0];
		if(d > len[1])
			d = len[1];
		if(mode == 'a'){
			a = 1;
			b = len[0];
			c = 1;
			d = len[1];
			j = nchanges;
		}
		Bprint(&stdout, "%s:", file1);
		range(a, b, ",");
		Bprint(&stdout, " - ");
		Bprint(&stdout, "%s:", file2);
		range(c, d, ",");
		Bputc(&stdout, '\n');
		at = a;
		for(; i<j; i++){
			fetch(ixold, at, changes[i].a-1, input[0], "  ");
			fetch(ixold, changes[i].a, changes[i].b, input[0], "- ");
			fetch(ixnew, changes[i].c, changes[i].d, input[1], "+ ");
			at = changes[i].b+1;
		}
		fetch(ixold, at, b, input[0], "  ");
	}
	nchanges = 0;
}
