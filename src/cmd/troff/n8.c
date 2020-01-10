#include <u.h>
#include "tdef.h"
#include "fns.h"
#include "ext.h"

#define	HY_BIT	0200	/* stuff in here only works for 7-bit ascii */
			/* this value is used (as a literal) in suftab.c */
			/* to encode possible hyphenation points in suffixes. */
			/* it could be changed, by widening the tables */
			/* to be shorts instead of chars. */

/*
 * troff8.c
 *
 * hyphenation
 */

int	hexsize = 0;		/* hyphenation exception list size */
char	*hbufp = NULL;		/* base of list */
char	*nexth = NULL;		/* first free slot in list */
Tchar	*hyend;

#define THRESH 160 		/* digram goodness threshold */
int	thresh = THRESH;

int	texhyphen(void);
static	int	alpha(Tchar);

void hyphen(Tchar *wp)
{
	int j;
	Tchar *i;

	i = wp;
	while (punct((*i++)))
		;
	if (!alpha(*--i))
		return;
	wdstart = i++;
	while (alpha(*i++))
		;
	hyend = wdend = --i - 1;
	while (punct((*i++)))
		;
	if (*--i)
		return;
	if (wdend - wdstart < 4)	/* 4 chars is too short to hyphenate */
		return;
	hyp = hyptr;
	*hyp = 0;
	hyoff = 2;

	/* for now, try exceptions first, then tex (if hyphalg is non-zero),
	   then suffix and digram if tex didn't hyphenate it at all.
	*/

	if (!exword() && !texhyphen() && !suffix())
		digram();

	/* this appears to sort hyphenation points into increasing order */
	*hyp++ = 0;
	if (*hyptr)
		for (j = 1; j; ) {
			j = 0;
			for (hyp = hyptr + 1; *hyp != 0; hyp++) {
				if (*(hyp - 1) > *hyp) {
					j++;
					i = *hyp;
					*hyp = *(hyp - 1);
					*(hyp - 1) = i;
				}
			}
		}
}

static int alpha(Tchar i)	/* non-zero if really alphabetic */
{
	if (ismot(i))
		return 0;
	else if (cbits(i) >= ALPHABET)	/* this isn't very elegant, but there's */
		return 0;		/* no good way to make sure i is in range for */
	else				/* the call of isalpha */
		return isalpha(cbits(i));
}

int
punct(Tchar i)
{
	if (!i || alpha(i))
		return(0);
	else
		return(1);
}


void caseha(void)	/* set hyphenation algorithm */
{
	hyphalg = HYPHALG;
	if (skip())
		return;
	noscale++;
	hyphalg = atoi0();
	noscale = 0;
}


void caseht(void)	/* set hyphenation threshold;  not in manual! */
{
	thresh = THRESH;
	if (skip())
		return;
	noscale++;
	thresh = atoi0();
	noscale = 0;
}


char *growh(char *where)
{
	char *new;

	hexsize += NHEX;
	if ((new = grow(hbufp, hexsize, sizeof(char))) == NULL)
		return NULL;
	if (new == hbufp) {
		return where;
	} else {
		int diff;
		diff = where - hbufp;
		hbufp = new;
		return new + diff;
	}
}


void casehw(void)
{
	int i, k;
	char *j;
	Tchar t;

	if (nexth == NULL) {
		if ((nexth = hbufp = grow(hbufp, NHEX, sizeof(char))) == NULL) {
			ERROR "No space for exception word list." WARN;
			return;
		}
		hexsize = NHEX;
	}
	k = 0;
	while (!skip()) {
		if ((j = nexth) >= hbufp + hexsize - 2)
			if ((j = nexth = growh(j)) == NULL)
				goto full;
		for (;;) {
			if (ismot(t = getch()))
				continue;
			i = cbits(t);
			if (i == ' ' || i == '\n') {
				*j++ = 0;
				nexth = j;
				*j = 0;
				if (i == ' ')
					break;
				else
					return;
			}
			if (i == '-') {
				k = HY_BIT;
				continue;
			}
			*j++ = maplow(i) | k;
			k = 0;
			if (j >= hbufp + hexsize - 2)
				if ((j = growh(j)) == NULL)
					goto full;
		}
	}
	return;
full:
	ERROR "Cannot grow exception word list." WARN;
	*nexth = 0;
}


int exword(void)
{
	Tchar *w;
	char *e, *save;

	e = hbufp;
	while (1) {
		save = e;
		if (e == NULL || *e == 0)
			return(0);
		w = wdstart;
		while (*e && w <= hyend && (*e & 0177) == maplow(cbits(*w))) {
			e++;
			w++;
		}
		if (!*e) {
			if (w-1 == hyend || (w == wdend && maplow(cbits(*w)) == 's')) {
				w = wdstart;
				for (e = save; *e; e++) {
					if (*e & HY_BIT)
						*hyp++ = w;
					if (hyp > hyptr + NHYP - 1)
						hyp = hyptr + NHYP - 1;
					w++;
				}
				return(1);
			} else {
				e++;
				continue;
			}
		} else
			while (*e++)
				;
	}
}

int
suffix(void)
{
	Tchar *w;
	char *s, *s0;
	Tchar i;
	extern char *suftab[];

again:
	i = cbits(*hyend);
	if (!alpha(i))
		return(0);
	if (i < 'a')
		i -= 'A' - 'a';
	if ((s0 = suftab[i-'a']) == 0)
		return(0);
	for (;;) {
		if ((i = *s0 & 017) == 0)
			return(0);
		s = s0 + i - 1;
		w = hyend - 1;
		while (s > s0 && w >= wdstart && (*s & 0177) == maplow(cbits(*w))) {
			s--;
			w--;
		}
		if (s == s0)
			break;
		s0 += i;
	}
	s = s0 + i - 1;
	w = hyend;
	if (*s0 & HY_BIT)
		goto mark;
	while (s > s0) {
		w--;
		if (*s-- & HY_BIT) {
mark:
			hyend = w - 1;
			if (*s0 & 0100)	/* 0100 used in suftab to encode something too */
				continue;
			if (!chkvow(w))
				return(0);
			*hyp++ = w;
		}
	}
	if (*s0 & 040)
		return(0);
	if (exword())
		return(1);
	goto again;
}

int
maplow(int i)
{
	if (isupper(i))
		i = tolower(i);
	return(i);
}

int
vowel(int i)
{
	switch (i) {
	case 'a': case 'A':
	case 'e': case 'E':
	case 'i': case 'I':
	case 'o': case 'O':
	case 'u': case 'U':
	case 'y': case 'Y':
		return(1);
	default:
		return(0);
	}
}


Tchar *chkvow(Tchar *w)
{
	while (--w >= wdstart)
		if (vowel(cbits(*w)))
			return(w);
	return(0);
}


void digram(void)
{
	Tchar *w;
	int val;
	Tchar *nhyend, *maxw;
	int maxval;
	extern char bxh[26][13], bxxh[26][13], xxh[26][13], xhx[26][13], hxx[26][13];
        maxw = 0;
again:
	if (!(w = chkvow(hyend + 1)))
		return;
	hyend = w;
	if (!(w = chkvow(hyend)))
		return;
	nhyend = w;
	maxval = 0;
	w--;
	while (++w < hyend && w < wdend - 1) {
		val = 1;
		if (w == wdstart)
			val *= dilook('a', cbits(*w), bxh);
		else if (w == wdstart + 1)
			val *= dilook(cbits(*(w-1)), cbits(*w), bxxh);
		else
			val *= dilook(cbits(*(w-1)), cbits(*w), xxh);
		val *= dilook(cbits(*w), cbits(*(w+1)), xhx);
		val *= dilook(cbits(*(w+1)), cbits(*(w+2)), hxx);
		if (val > maxval) {
			maxval = val;
			maxw = w + 1;
		}
	}
	hyend = nhyend;
	if (maxval > thresh)
		*hyp++ = maxw;
	goto again;
}

int
dilook(int a, int b, char t[26][13])
{
	int i, j;

	i = t[maplow(a)-'a'][(j = maplow(b)-'a')/2];
	if (!(j & 01))
		i >>= 4;
	return(i & 017);
}


/* here beginneth the tex hyphenation code, as interpreted freely */
/* the main difference is that there is no attempt to squeeze space */
/* as tightly at tex does. */

static int	texit(Tchar *, Tchar *);
static int	readpats(void);
static void	install(char *);
static void	fixup(void);
static int	trieindex(int, int);

static char	pats[50000];	/* size ought to be computed dynamically */
static char	*nextpat = pats;
static char	*trie[27*27];	/* english-specific sizes */

int texhyphen(void)
{
	static int loaded = 0;		/* -1: couldn't find tex file */

	if (hyphalg == 0 || loaded == -1)	/* non-zero => tex for now */
		return 0;
	if (loaded == 0) {
		if (readpats())
			loaded = 1;
		else
			loaded = -1;
	}
	return texit(wdstart, wdend);
}

static int texit(Tchar *start, Tchar *end)	/* hyphenate as in tex, return # found */
{
	int nw, i, k, equal, cnt[500];
	char w[500+1], *np, *pp, *wp, *xpp, *xwp;

	w[0] = '.';
	for (nw = 1; start <= end && nw < 500-1; nw++, start++)
		w[nw] = maplow(tolower(cbits(*start)));
	start -= (nw - 1);
	w[nw++] = '.';
	w[nw] = 0;
/*
 * printf("try %s\n", w);
*/
	for (i = 0; i <= nw; i++)
		cnt[i] = '0';

	for (wp = w; wp+1 < w+nw; wp++) {
		for (pp = trie[trieindex(*wp, *(wp+1))]; pp < nextpat; ) {
			if (pp == 0		/* no trie entry */
			 || *pp != *wp		/* no match on 1st letter */
			 || *(pp+1) != *(wp+1))	/* no match on 2nd letter */
				break;		/*   so move to next letter of word */
			equal = 1;
			for (xpp = pp+2, xwp = wp+2; *xpp; )
				if (*xpp++ != *xwp++) {
					equal = 0;
					break;
				}
			if (equal) {
				np = xpp+1;	/* numpat */
				for (k = wp-w; *np; k++, np++)
					if (*np > cnt[k])
						cnt[k] = *np;
/*
 * printf("match: %s  %s\n", pp, xpp+1);
*/
			}
			pp += *(pp-1);	/* skip over pattern and numbers to next */
		}
	}
/*
 * for (i = 0; i < nw; i++) printf("%c", w[i]);
 * printf("  ");
 * for (i = 0; i <= nw; i++) printf("%c", cnt[i]);
 * printf("\n");
*/
/*
 * 	for (i = 1; i < nw - 1; i++) {
 * 		if (i > 2 && i < nw - 3 && cnt[i] % 2)
 * 			printf("-");
 * 		if (cbits(start[i-1]) != '.')
 * 			printf("%c", cbits(start[i-1]));
 * 	}
 * 	printf("\n");
*/
	for (i = 1; i < nw -1; i++)
		if (i > 2 && i < nw - 3 && cnt[i] % 2)
			*hyp++ = start + i - 1;
	return hyp - hyptr;	/* non-zero if a hyphen was found */
}

/*
	This code assumes that hyphen.tex looks like
		% some comments
		\patterns{ % more comments
		pat5ter4ns, 1 per line, SORTED, nothing else
		}
		more goo
		\hyphenation{ % more comments
		ex-cep-tions, one per line; i ignore this part for now
		}

	this code is NOT robust against variations.  unfortunately,
	it looks like every local language version of this file has
	a different format.  i have also made no provision for weird
	characters.  sigh.
*/

static int readpats(void)
{
	FILE *fp;
	char buf[200], buf1[200];

	if ((fp = fopen(unsharp(TEXHYPHENS), "r")) == NULL
	 && (fp = fopen(unsharp(DWBalthyphens), "r")) == NULL) {
		ERROR "warning: can't find hyphen.tex" WARN;
		return 0;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		sscanf(buf, "%s", buf1);
		if (strcmp(buf1, "\\patterns{") == 0)
			break;
	}
	while (fgets(buf, sizeof buf, fp) != NULL) {
		if (buf[0] == '}')
			break;
		install(buf);
	}
	fclose(fp);
	fixup();
	return 1;
}

static void install(char *s)	/* map ab4c5de to: 12 abcde \0 00405 \0 */
{
	int npat, lastpat;
	char num[500], *onextpat = nextpat;

	num[0] = '0';
	*nextpat++ = ' ';	/* fill in with count later */
	for (npat = lastpat = 0; *s != '\n' && *s != '\0'; s++) {
		if (isdigit((uchar)*s)) {
			num[npat] = *s;
			lastpat = npat;
		} else {
			*nextpat++ = *s;
			npat++;
			num[npat] = '0';
		}
	}
	*nextpat++ = 0;
	if (nextpat > pats + sizeof(pats)-20) {
		ERROR "tex hyphenation table overflow, tail end ignored" WARN;
		nextpat = onextpat;
	}
	num[lastpat+1] = 0;
	strcat(nextpat, num);
	nextpat += strlen(nextpat) + 1;
}

static void fixup(void)	/* build indexes of where . a b c ... start */
{
	char *p, *lastc;
	int n;

	for (lastc = pats, p = pats+1; p < nextpat; p++)
		if (*p == ' ') {
			*lastc = p - lastc;
			lastc = p;
		}
	*lastc = p - lastc;
	for (p = pats+1; p < nextpat; ) {
		n = trieindex(p[0], p[1]);
		if (trie[n] == 0)
			trie[n] = p;
		p += p[-1];
	}
	/* printf("pats = %d\n", nextpat - pats); */
}

static int trieindex(int d1, int d2)
{
	int z;

	z = 27 * (d1 == '.' ? 0 : d1 - 'a' + 1) + (d2 == '.' ? 0 : d2 - 'a' + 1);
	assert(z >= 0 && z < 27*27);
	return z;
}
