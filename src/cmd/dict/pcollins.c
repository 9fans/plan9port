#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * Routines for handling dictionaries in the "Paperback Collins"
 * format (with tags surrounded by >....<)
 */
enum {
	Buflen=1000
};

/* More special runes */
enum {
	B = MULTIE+1,	/* bold */
	H,		/* headword start */
	I,		/* italics */
	Ps,		/* pronunciation start */
	Pe,		/* pronunciation end */
	R,		/* roman */
	X		/* headword end */
};

/* Assoc tables must be sorted on first field */

static Assoc tagtab[] = {
	{"AA",		0xc5},
	{"AC",		LACU},
	{"B",		B},
	{"CE",		LCED},
	{"CI",		LFRN},
	{"Di",		0x131},
	{"EL",		0x2d},
	{"GR",		LGRV},
	{"H",		H},
	{"I",		I},
	{"OE",		0x152},
	{"R",		R},
	{"TI",		LTIL},
	{"UM",		LUML},
	{"X",		X},
	{"[",		Ps},
	{"]",		Pe},
	{"ac",		LACU},
	{"ce",		LCED},
	{"ci",		LFRN},
	{"gr",		LGRV},
	{"oe",		0x153},
	{"supe",	0x65},		/* should be raised */
	{"supo",	0x6f},		/* should be raised */
	{"ti",		LTIL},
	{"um",		LUML},
	{"{",		Ps},
	{"~",		0x7e},
	{"~~",		MTT}
};

static Rune normtab[128] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	0x20,	NONE,	NONE,	NONE,	NONE,	NONE,
/*10*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/  0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	TAGE,	0x3d,	TAGS,	0x3f,
/*40*/  0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,	'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,	'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	NONE
};

static char *gettag(char *, char *);

static Entry	curentry;
static char	tag[Buflen];
#define cursize (curentry.end-curentry.start)

void
pcollprintentry(Entry e, int cmd)
{
	char *p, *pe;
	long r, rprev, t, rlig;
	int saveoi;
	Rune *transtab;

	p = e.start;
	pe = e.end;
	transtab = normtab;
	rprev = NONE;
	changett(0, 0, 0);
	curentry = e;
	saveoi = 0;
	if(cmd == 'h')
		outinhibit = 1;
	while(p < pe) {
		if(cmd == 'r') {
			outchar(*p++);
			continue;
		}
		r = transtab[(*p++)&0x7F];
		if(r < NONE) {
			/* Emit the rune, but buffer in case of ligature */
			if(rprev != NONE)
				outrune(rprev);
			rprev = r;
		} else if(r == TAGS) {
			p = gettag(p, pe);
			t = lookassoc(tagtab, asize(tagtab), tag);
			if(t == -1) {
				if(debug && !outinhibit)
					err("tag %ld %d %s",
						e.doff, cursize, tag);
				continue;
			}
			if(t < NONE) {
				if(rprev != NONE)
					outrune(rprev);
				rprev = t;
			} else if(t >= LIGS && t < LIGE) {
				/* handle possible ligature */
				rlig = liglookup(t, rprev);
				if(rlig != NONE)
					rprev = rlig;	/* overwrite rprev */
				else {
					/* could print accent, but let's not */
					if(rprev != NONE) outrune(rprev);
					rprev = NONE;
				}
			} else if(t >= MULTI && t < MULTIE) {
				if(rprev != NONE) {
					outrune(rprev);
					rprev = NONE;
				}
				outrunes(multitab[t-MULTI]);
			} else {
				if(rprev != NONE) {
					outrune(rprev);
					rprev = NONE;
				}
				switch(t){
				case H:
					if(cmd == 'h')
						outinhibit = 0;
					else
						outnl(0);
					break;
				case X:
					if(cmd == 'h')
						outinhibit = 1;
					else
						outchars(".  ");
					break;
				case Ps:
					/* don't know enough of pron. key yet */
					saveoi = outinhibit;
					outinhibit = 1;
					break;
				case Pe:
					outinhibit = saveoi;
					break;
				}
			}
		}
	}
	if(cmd == 'h')
		outinhibit = 0;
	outnl(0);
}

long
pcollnextoff(long fromoff)
{
	long a;
	char *p;

	a = Bseek(bdict, fromoff, 0);
	if(a < 0)
		return -1;
	for(;;) {
		p = Brdline(bdict, '\n');
		if(!p)
			break;
		if(p[0] == '>' && p[1] == 'H' && p[2] == '<')
			return (Boffset(bdict)-Blinelen(bdict));
	}
	return -1;
}

void
pcollprintkey(void)
{
	Bprint(bout, "No pronunciation key yet\n");
}

/*
 * f points just after '>'; fe points at end of entry.
 * Expect next characters from bin to match:
 *  [^ <]+<
 *     tag
 * Accumulate the tag in tag[].
 * Return pointer to after final '<'.
 */
static char *
gettag(char *f, char *fe)
{
	char *t;
	int c, i;

	t = tag;
	i = Buflen;
	while(--i > 0) {
		c = *f++;
		if(c == '<' || f == fe)
			break;
		*t++ = c;
	}
	*t = 0;
	return f;
}
