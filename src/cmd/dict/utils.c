#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

Dict dicts[] = {
	{"oed",		"Oxford English Dictionary, 2nd Ed.",
	 "oed2",	"oed2index",
	 oednextoff,	oedprintentry,		oedprintkey},
	{"ahd",		"American Heritage Dictionary, 2nd College Ed.",
	 "ahd/DICT.DB",	"ahd/index",
	 ahdnextoff,	ahdprintentry,		ahdprintkey},
	{"pgw",		"Project Gutenberg Webster Dictionary",
	 "pgw",	"pgwindex",
	 pgwnextoff,	pgwprintentry,		pgwprintkey},
	{"thesaurus",	"Collins Thesaurus",
	 "thesaurus",	"thesindex",
	 thesnextoff,	thesprintentry,	thesprintkey},
	{"roget",		"Project Gutenberg Roget's Thesaurus",
	 "roget", "rogetindex",
	 rogetnextoff,	rogetprintentry,	rogetprintkey},

	{"ce",		"Gendai Chinese->English",
	 "world/sansdata/sandic24.dat",
	 "world/sansdata/ceindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"ceh",		"Gendai Chinese->English (Hanzi index)",
	 "world/sansdata/sandic24.dat",
	 "world/sansdata/cehindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"ec",		"Gendai English->Chinese",
	 "world/sansdata/sandic24.dat",
	 "world/sansdata/ecindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"dae",		"Gyldendal Danish->English",
	 "world/gylddata/sandic30.dat",
	 "world/gylddata/daeindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"eda",		"Gyldendal English->Danish",
	 "world/gylddata/sandic29.dat",
	 "world/gylddata/edaindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"due",		"Wolters-Noordhoff Dutch->English",
	 "world/woltdata/sandic07.dat",
	 "world/woltdata/deindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"edu",		"Wolters-Noordhoff English->Dutch",
	 "world/woltdata/sandic06.dat",
	 "world/woltdata/edindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"fie",		"WSOY Finnish->English",
	 "world/werndata/sandic32.dat",
	 "world/werndata/fieindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"efi",		"WSOY English->Finnish",
	 "world/werndata/sandic31.dat",
	 "world/werndata/efiindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"fe",		"Collins French->English",
	 "fe",	"feindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},
	{"ef",		"Collins English->French",
	 "ef",	"efindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},

	{"ge",		"Collins German->English",
	 "ge",	"geindex",
	 pcollgnextoff,	pcollgprintentry,	pcollgprintkey},
	{"eg",		"Collins English->German",
	 "eg",	"egindex",
	 pcollgnextoff,	pcollgprintentry,	pcollgprintkey},

	{"ie",		"Collins Italian->English",
	 "ie",	"ieindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},
	{"ei",		"Collins English->Italian",
	 "ei",	"eiindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},

	{"je",		"Sanshusha Japanese->English",
	 "world/sansdata/sandic18.dat",
	 "world/sansdata/jeindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"jek",		"Sanshusha Japanese->English (Kanji index)",
	 "world/sansdata/sandic18.dat",
	 "world/sansdata/jekindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"ej",		"Sanshusha English->Japanese",
	 "world/sansdata/sandic18.dat",
	 "world/sansdata/ejindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"tjeg",	"Sanshusha technical Japanese->English,German",
	 "world/sansdata/sandic16.dat",
	 "world/sansdata/tjegindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"tjegk",	"Sanshusha technical Japanese->English,German (Kanji index)",
	 "world/sansdata/sandic16.dat",
	 "world/sansdata/tjegkindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"tegj",	"Sanshusha technical English->German,Japanese",
	 "world/sansdata/sandic16.dat",
	 "world/sansdata/tegjindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"tgje",	"Sanshusha technical German->Japanese,English",
	 "world/sansdata/sandic16.dat",
	 "world/sansdata/tgjeindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"ne",		"Kunnskapforlaget Norwegian->English",
	 "world/kunndata/sandic28.dat",
	 "world/kunndata/neindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"en",		"Kunnskapforlaget English->Norwegian",
	 "world/kunndata/sandic27.dat",
	 "world/kunndata/enindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"re",		"Leon Ungier Russian->English",
	 "re",	"reindex",
	 simplenextoff,	simpleprintentry,	simpleprintkey},
	{"er",		"Leon Ungier English->Russian",
	 "re",	"erindex",
	 simplenextoff,	simpleprintentry,	simpleprintkey},

	{"se",		"Collins Spanish->English",
	 "se",	"seindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},
	{"es",		"Collins English->Spanish",
	 "es",	"esindex",
	 pcollnextoff,	pcollprintentry,	pcollprintkey},

	{"swe",		"Esselte Studium Swedish->English",
	 "world/essedata/sandic34.dat",
	 "world/essedata/sweindex",
	 worldnextoff,	worldprintentry,	worldprintkey},
	{"esw",		"Esselte Studium English->Swedish",
	 "world/essedata/sandic33.dat",
	 "world/essedata/eswindex",
	 worldnextoff,	worldprintentry,	worldprintkey},

	{"movie",	"Movies -- by title",
	 "movie/data",	"movtindex",
	 movienextoff,	movieprintentry,	movieprintkey},
	{"moviea",	"Movies -- by actor",
	 "movie/data",	"movaindex",
	 movienextoff,	movieprintentry,	movieprintkey},
	{"movied",	"Movies -- by director",
	 "movie/data",	"movdindex",
	 movienextoff,	movieprintentry,	movieprintkey},

	{"slang",	"English Slang",
	 "slang",	"slangindex",
	 slangnextoff,	slangprintentry,	slangprintkey},

	{"robert",	"Robert Électronique",
	 "robert/_pointers",	"robert/_index",
	 robertnextoff,	robertindexentry,	robertprintkey},
	{"robertv",	"Robert Électronique - formes des verbes",
	 "robert/flex.rob",	"robert/_flexindex",
	 robertnextflex,	robertflexentry,	robertprintkey},

	{0, 0, 0, 0, 0}
};

typedef struct Lig Lig;
struct Lig {
	Rune	start;		/* accent rune */
	Rune	pairs[100];		/* <char,accented version> pairs */
};

/* keep in sync with dict.h */
static Lig ligtab[Nligs] = {
	{0xb4,	{0x41, 0xc1, 0x61, 0xe1, 0x43, 0x106, 0x63, 0x107, 0x45, 0xc9, 0x65, 0xe9, 0x67, 0x123, 0x49, 0xcd, 0x69, 0xed, 0x131, 0xed, 0x4c, 0x139, 0x6c, 0x13a, 0x4e, 0x143, 0x6e, 0x144, 0x4f, 0xd3, 0x6f, 0xf3, 0x52, 0x154, 0x72, 0x155, 0x53, 0x15a, 0x73, 0x15b, 0x55, 0xda, 0x75, 0xfa, 0x59, 0xdd, 0x79, 0xfd, 0x5a, 0x179, 0x7a, 0x17a, 0}},
	{0x2cb,	{0x41, 0xc0, 0x61, 0xe0, 0x45, 0xc8, 0x65, 0xe8, 0x49, 0xcc, 0x69, 0xec, 0x131, 0xec, 0x4f, 0xd2, 0x6f, 0xf2, 0x55, 0xd9, 0x75, 0xf9, 0}},
	{0xa8,	{0x41, 0xc4, 0x61, 0xe4, 0x45, 0xcb, 0x65, 0xeb, 0x49, 0xcf, 0x69, 0xef, 0x4f, 0xd6, 0x6f, 0xf6, 0x55, 0xdc, 0x75, 0xfc, 0x59, 0x178, 0x79, 0xff, 0}},
	{0xb8,	{0x43, 0xc7, 0x63, 0xe7, 0x47, 0x122, 0x4b, 0x136, 0x6b, 0x137, 0x4c, 0x13b, 0x6c, 0x13c, 0x4e, 0x145, 0x6e, 0x146, 0x52, 0x156, 0x72, 0x157, 0x53, 0x15e, 0x73, 0x15f, 0x54, 0x162, 0x74, 0x163, 0}},
	{0x2dc,	{0x41, 0xc3, 0x61, 0xe3, 0x49, 0x128, 0x69, 0x129, 0x131, 0x129, 0x4e, 0xd1, 0x6e, 0xf1, 0x4f, 0xd5, 0x6f, 0xf5, 0x55, 0x168, 0x75, 0x169, 0}},
	{0x2d8,	{0x41, 0x102, 0x61, 0x103, 0x45, 0x114, 0x65, 0x115, 0x47, 0x11e, 0x67, 0x11f, 0x49, 0x12c, 0x69, 0x12d, 0x131, 0x12d, 0x4f, 0x14e, 0x6f, 0x14f, 0x55, 0x16c, 0x75, 0x16d, 0}},
	{0x2da,	{0x41, 0xc5, 0x61, 0xe5, 0x55, 0x16e, 0x75, 0x16f, 0}},
	{0x2d9,	{0x43, 0x10a, 0x63, 0x10b, 0x45, 0x116, 0x65, 0x117, 0x47, 0x120, 0x67, 0x121, 0x49, 0x130, 0x4c, 0x13f, 0x6c, 0x140, 0x5a, 0x17b, 0x7a, 0x17c, 0}},
	{0x2e,	{0}},
	{0x2322,	{0x41, 0xc2, 0x61, 0xe2, 0x43, 0x108, 0x63, 0x109, 0x45, 0xca, 0x65, 0xea, 0x47, 0x11c, 0x67, 0x11d, 0x48, 0x124, 0x68, 0x125, 0x49, 0xce, 0x69, 0xee, 0x131, 0xee, 0x4a, 0x134, 0x6a, 0x135, 0x4f, 0xd4, 0x6f, 0xf4, 0x53, 0x15c, 0x73, 0x15d, 0x55, 0xdb, 0x75, 0xfb, 0x57, 0x174, 0x77, 0x175, 0x59, 0x176, 0x79, 0x177, 0}},
	{0x32f,	{0}},
	{0x2db,	{0x41, 0x104, 0x61, 0x105, 0x45, 0x118, 0x65, 0x119, 0x49, 0x12e, 0x69, 0x12f, 0x131, 0x12f, 0x55, 0x172, 0x75, 0x173, 0}},
	{0xaf,	{0x41, 0x100, 0x61, 0x101, 0x45, 0x112, 0x65, 0x113, 0x49, 0x12a, 0x69, 0x12b, 0x131, 0x12b, 0x4f, 0x14c, 0x6f, 0x14d, 0x55, 0x16a, 0x75, 0x16b, 0}},
	{0x2c7,	{0x43, 0x10c, 0x63, 0x10d, 0x44, 0x10e, 0x64, 0x10f, 0x45, 0x11a, 0x65, 0x11b, 0x4c, 0x13d, 0x6c, 0x13e, 0x4e, 0x147, 0x6e, 0x148, 0x52, 0x158, 0x72, 0x159, 0x53, 0x160, 0x73, 0x161, 0x54, 0x164, 0x74, 0x165, 0x5a, 0x17d, 0x7a, 0x17e, 0}},
	{0x2bd,	{0}},
	{0x2bc,	{0}},
	{0x32e,	{0}}
};

Rune multitab[Nmulti][5] = {
	{0x2bd, 0x3b1, 0},
	{0x2bc, 0x3b1, 0},
	{0x61, 0x6e, 0x64, 0},
	{0x61, 0x2f, 0x71, 0},
	{0x3c, 0x7c, 0},
	{0x2e, 0x2e, 0},
	{0x2e, 0x2e, 0x2e, 0},
	{0x2bd, 0x3b5, 0},
	{0x2bc, 0x3b5, 0},
	{0x2014, 0x2014, 0},
	{0x2bd, 0x3b7, 0},
	{0x2bc, 0x3b7, 0},
	{0x2bd, 0x3b9, 0},
	{0x2bc, 0x3b9, 0},
	{0x63, 0x74, 0},
	{0x66, 0x66, 0},
	{0x66, 0x66, 0x69, 0},
	{0x66, 0x66, 0x6c, 0},
	{0x66, 0x6c, 0},
	{0x66, 0x69, 0},
	{0x26b, 0x26b, 0},
	{0x73, 0x74, 0},
	{0x2bd, 0x3bf, 0},
	{0x2bc, 0x3bf, 0},
	{0x6f, 0x72, 0},
	{0x2bd, 0x3c1, 0},
	{0x2bc, 0x3c1, 0},
	{0x7e, 0x7e, 0},
	{0x2bd, 0x3c5, 0},
	{0x2bc, 0x3c5, 0},
	{0x2bd, 0x3c9, 0},
	{0x2bc, 0x3c9, 0},
	{0x6f, 0x65, 0},
	{0x20, 0x20, 0}
};

#define	risupper(r)	(0x41 <= (r) && (r) <= 0x5a)
#define	rislatin1(r)	(0xC0 <= (r) && (r) <= 0xFF)
#define	rtolower(r)	((r)-'A'+'a')

static Rune latin_fold_tab[] =
{
/*	Table to fold latin 1 characters to ASCII equivalents
			based at Rune value 0xc0

	 À    Á    Â    Ã    Ä    Å    Æ    Ç
	 È    É    Ê    Ë    Ì    Í    Î    Ï
	 Ð    Ñ    Ò    Ó    Ô    Õ    Ö    ×
	 Ø    Ù    Ú    Û    Ü    Ý    Þ    ß
	 à    á    â    ã    ä    å    æ    ç
	 è    é    ê    ë    ì    í    î    ï
	 ð    ñ    ò    ó    ô    õ    ö    ÷
	 ø    ù    ú    û    ü    ý    þ    ÿ
*/
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c',
	'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 ,
	'o', 'u', 'u', 'u', 'u', 'y',  0 ,  0 ,
	'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c',
	'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
	'd', 'n', 'o', 'o', 'o', 'o', 'o',  0 ,
	'o', 'u', 'u', 'u', 'u', 'y',  0 , 'y'
};

static Rune 	*ttabstack[20];
static int	ntt;

/*
 * tab is an array of n Assoc's, sorted by key.
 * Look for key in tab, and return corresponding val
 * or -1 if not there
 */
long
lookassoc(Assoc *tab, int n, char *key)
{
	Assoc *q;
	long i, low, high;
	int r;

	for(low = -1, high = n; high > low+1; ){
		i = (high+low)/2;
		q = &tab[i];
		if((r=strcmp(key, q->key))<0)
			high = i;
		else if(r == 0)
			return q->val;
		else
			low=i;
	}
	return -1;
}

long
looknassoc(Nassoc *tab, int n, long key)
{
	Nassoc *q;
	long i, low, high;

	for(low = -1, high = n; high > low+1; ){
		i = (high+low)/2;
		q = &tab[i];
		if(key < q->key)
			high = i;
		else if(key == q->key)
			return q->val;
		else
			low=i;
	}
	return -1;
}

void
err(char *fmt, ...)
{
	char buf[1000];
	va_list v;

	va_start(v, fmt);
	vsnprint(buf, sizeof(buf), fmt, v);
	va_end(v);
	fprint(2, "%s: %s\n", argv0, buf);
}

/*
 * Write the rune r to bout, keeping track of line length
 * and breaking the lines (at blanks) when they get too long
 */
void
outrune(long r)
{
	if(outinhibit)
		return;
	if(++linelen > breaklen && r == 0x20) {
		Bputc(bout, '\n');
		linelen = 0;
	} else
		Bputrune(bout, r);
}

void
outrunes(Rune *rp)
{
	Rune r;

	while((r = *rp++) != 0)
		outrune(r);
}

/* like outrune, but when arg is know to be a char */
void
outchar(int c)
{
	if(outinhibit)
		return;
	if(++linelen > breaklen && c == ' ') {
		c ='\n';
		linelen = 0;
	}
	Bputc(bout, c);
}

void
outchars(char *s)
{
	char c;

	while((c = *s++) != 0)
		outchar(c);
}

void
outprint(char *fmt, ...)
{
	char buf[1000];
	va_list v;

	va_start(v, fmt);
	vsnprint(buf, sizeof(buf), fmt, v);
	va_end(v);
	outchars(buf);
}

void
outpiece(char *b, char *e)
{
	int c, lastc;

	lastc = 0;
	while(b < e) {
		c = *b++;
		if(c == '\n')
			c = ' ';
		if(!(c == ' ' && lastc == ' '))
			outchar(c);
		lastc = c;
	}
}

/*
 * Go to new line if not already there; indent if ind != 0.
 * If ind > 1, leave a blank line too.
 * Slight hack: assume if current line is only one or two
 * characters long, then they were spaces.
 */
void
outnl(int ind)
{
	if(outinhibit)
		return;
	if(ind) {
		if(ind > 1) {
			if(linelen > 2)
				Bputc(bout, '\n');
			Bprint(bout, "\n  ");
		} else if(linelen == 0)
			Bprint(bout, "  ");
		else if(linelen == 1)
			Bputc(bout, ' ');
		else if(linelen != 2)
			Bprint(bout, "\n  ");
		linelen = 2;
	} else {
		if(linelen) {
			Bputc(bout, '\n');
			linelen = 0;
		}
	}
}

/*
 * Fold the runes in null-terminated rp.
 * Use the sort(1) definition of folding (uppercase to lowercase,
 * latin1-accented characters to corresponding unaccented chars)
 */
void
fold(Rune *rp)
{
	Rune r;

	while((r = *rp) != 0) {
		if (rislatin1(r) && latin_fold_tab[r-0xc0])
				r = latin_fold_tab[r-0xc0];
		if(risupper(r))
			r = rtolower(r);
		*rp++ = r;
	}
}

/*
 * Like fold, but put folded result into new
 * (assumed to have enough space).
 * old is a regular expression, but we know that
 * metacharacters aren't affected
 */
void
foldre(char *new, char *old)
{
	Rune r;

	while(*old) {
		old += chartorune(&r, old);
		if (rislatin1(r) && latin_fold_tab[r-0xc0])
				r = latin_fold_tab[r-0xc0];
		if(risupper(r))
			r = rtolower(r);
		new += runetochar(new, &r);
	}
	*new = 0;
}

/*
 *	acomp(s, t) returns:
 *		-2 if s strictly precedes t
 *		-1 if s is a prefix of t
 *		0 if s is the same as t
 *		1 if t is a prefix of s
 *		2 if t strictly precedes s
 */

int
acomp(Rune *s, Rune *t)
{
	int cs, ct;

	for(;;) {
		cs = *s;
		ct = *t;
		if(cs != ct)
			break;
		if(cs == 0)
			return 0;
		s++;
		t++;
	}
	if(cs == 0)
		return -1;
	if(ct == 0)
		return 1;
	if(cs < ct)
		return -2;
	return 2;
}

/*
 * Copy null terminated Runes from 'from' to 'to'.
 */
void
runescpy(Rune *to, Rune *from)
{
	while((*to++ = *from++) != 0)
		continue;
}

/*
 * Conversion of unsigned number to long, no overflow detection
 */
long
runetol(Rune *r)
{
	int c;
	long n;

	n = 0;
	for(;; r++){
		c = *r;
		if(0x30<=c && c<=0x39)
			c -= '0';
		else
			break;
		n = n*10 + c;
	}
	return n;
}

/*
 * See if there is a rune corresponding to the accented
 * version of r with accent acc (acc in [LIGS..LIGE-1]),
 * and return it if so, else return NONE.
 */
Rune
liglookup(Rune acc, Rune r)
{
	Rune *p;

	if(acc < LIGS || acc >= LIGE)
		return NONE;
	for(p = ligtab[acc-LIGS].pairs; *p; p += 2)
		if(*p == r)
			return *(p+1);
	return NONE;
}

/*
 * Maintain a translation table stack (a translation table
 * is an array of Runes indexed by bytes or 7-bit bytes).
 * If starting is true, push the curtab onto the stack
 * and return newtab; else pop the top of the stack and
 * return it.
 * If curtab is 0, initialize the stack and return.
 */
Rune *
changett(Rune *curtab, Rune *newtab, int starting)
{
	if(curtab == 0) {
		ntt = 0;
		return 0;
	}
	if(starting) {
		if(ntt >= asize(ttabstack)) {
			if(debug)
				err("translation stack overflow");
			return curtab;
		}
		ttabstack[ntt++] = curtab;
		return newtab;
	} else {
		if(ntt == 0) {
			if(debug)
				err("translation stack underflow");
			return curtab;
		}
		return ttabstack[--ntt];
	}
}
