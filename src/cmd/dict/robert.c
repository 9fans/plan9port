#include <u.h>
#include <libc.h>
#include <bio.h>
#include "dict.h"

/*
 * Robert Électronique.
 */

enum
{
	CIT = MULTIE+1,	/* citation ptr followed by long int and ascii label */
	BROM,		/* bold roman */
	ITON,		/* start italic */
	ROM,		/* roman */
	SYM,		/* symbol font? */
	HEL,		/* helvetica */
	BHEL,		/* helvetica bold */
	SMALL,		/* smaller? */
	ITOFF,		/* end italic */
	SUP,		/* following character is superscript */
	SUB		/* following character is subscript */
};

static Rune intab[256] = {
	/*0*/	/*1*/	/*2*/	/*3*/	/*4*/	/*5*/	/*6*/	/*7*/
/*00*/	NONE,	0x263a,	0x263b,	0x2665,	0x2666,	0x2663,	0x2660,	0x2022,
	0x25d8,	0x298,'\n',	0x2642,	0x2640,	0x266a,	0x266b,	0x203b,
/*10*/	0x21e8,	0x21e6,	0x2195,	0x203c,	0xb6,	0xa7,	0x2043,	0x21a8,
	0x2191,	0x2193,	0x2192,	0x2190,	0x2319,	0x2194,	0x25b4,	0x25be,
/*20*/	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,'\'',
	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
/*30*/	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
/*40*/	0x40,	0x41,	0x42,	0x43,	0x44,	0x45,	0x46,	0x47,
	0x48,	0x49,	0x4a,	0x4b,'L',	0x4d,	0x4e,	0x4f,
/*50*/	0x50,	0x51,	0x52,	0x53,	0x54,	0x55,	0x56,	0x57,
	0x58,	0x59,	0x5a,	0x5b,'\\',	0x5d,	0x5e,	0x5f,
/*60*/	0x60,	0x61,	0x62,	0x63,	0x64,	0x65,	0x66,	0x67,
	0x68,	0x69,	0x6a,	0x6b,	0x6c,	0x6d,	0x6e,	0x6f,
/*70*/	0x70,	0x71,	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,
	0x78,	0x79,	0x7a,	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
/*80*/	0xc7,	0xfc,	0xe9,	0xe2,	0xe4,	0xe0,	0xe5,	0xe7,
	0xea,	0xeb,	0xe8,	0xef,	0xee,	0xec,	0xc4,	0xc5,
/*90*/	0xc9,	0xe6,	0xc6,	0xf4,	0xf6,	0xf2,	0xfb,	0xf9,
	0xff,	0xd6,	0xdc,	0xa2,	0xa3,	0xa5,	0x20a7,	0x283,
/*a0*/	0xe1,	0xed,	0xf3,	0xfa,	0xf1,	0xd1,	0xaa,	0xba,
	0xbf,	0x2310,	0xac,	0xbd,	0xbc,	0xa1,	0xab,	0xbb,
/*b0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*c0*/	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
	CIT,	BROM,	NONE,	ITON,	ROM,	SYM,	HEL,	BHEL,
/*d0*/	NONE,	SMALL,	ITOFF,	SUP,	SUB,	NONE,	NONE,	NONE,
	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,	NONE,
/*e0*/	0x3b1,	0xdf,	0x3b3,	0x3c0,	0x3a3,	0x3c3,	0xb5,	0x3c4,
	0x3a6,	0x398,	0x3a9,	0x3b4,	0x221e,	0xd8,	0x3b5,	0x2229,
/*f0*/	0x2261,	0xb1,	0x2265,	0x2264,	0x2320,	0x2321,	0xf7,	0x2248,
	0xb0,	0x2219,	0xb7,	0x221a,	0x207f,	0xb2,	0x220e,	0xa0
};

static Rune suptab[256];

static void
initsuptab(void)
{
	suptab['0']= 0x2070;	suptab['1']= 0x2071;	suptab['2']= 0x2072;	suptab['3']= 0x2073;
	suptab['4']= 0x2074;	suptab['5']= 0x2075;	suptab['6']= 0x2076;	suptab['7']= 0x2077;
	suptab['8']= 0x2078;	suptab['9']= 0x2079;	suptab['+']= 0x207a;	suptab['-']= 0x207b;
	suptab['=']= 0x207c;	suptab['(']= 0x207d;	suptab[')']= 0x207e;	suptab['a']= 0xaa;
	suptab['n']= 0x207f;	suptab['o']= 0xba;
}

static Rune subtab[256];

static void
initsubtab(void)
{
	subtab['0']= 0x2080;	subtab['1']= 0x2081;	subtab['2']= 0x2082;	subtab['3']= 0x2083;
	subtab['4']= 0x2084;	subtab['5']= 0x2085;	subtab['6']= 0x2086;	subtab['7']= 0x2087;
	subtab['8']= 0x2088;	subtab['9']= 0x2089;	subtab['+']= 0x208a;	subtab['-']= 0x208b;
	subtab['=']= 0x208c;	subtab['(']= 0x208d;	subtab[')']= 0x208e;
}

#define	GSHORT(p)	(((p)[0]<<8) | (p)[1])
#define	GLONG(p)	(((p)[0]<<24) | ((p)[1]<<16) | ((p)[2]<<8) | (p)[3])

static char	cfile[] = "robert/cits.rob";
static char	dfile[] = "robert/defs.rob";
static char	efile[] = "robert/etym.rob";
static char	kfile[] = "robert/_phon";

static Biobuf *	cb;
static Biobuf *	db;
static Biobuf *	eb;

static Biobuf *	Bouvrir(char*);
static void	citation(int, int);
static void	robertprintentry(Entry*, Entry*, int);

void
robertindexentry(Entry e, int cmd)
{
	uchar *p = (uchar *)e.start;
	long ea, el, da, dl, fa;
	Entry def, etym;

	ea = GLONG(&p[0]);
	el = GSHORT(&p[4]);
	da = GLONG(&p[6]);
	dl = GSHORT(&p[10]);
	fa = GLONG(&p[12]);
	USED(fa);

	if(db == 0)
		db = Bouvrir(dfile);
	def.start = malloc(dl+1);
	def.end = def.start + dl;
	def.doff = da;
	Bseek(db, da, 0);
	Bread(db, def.start, dl);
	*def.end = 0;
	if(cmd == 'h'){
		robertprintentry(&def, 0, cmd);
	}else{
		if(eb == 0)
			eb = Bouvrir(efile);
		etym.start = malloc(el+1);
		etym.end = etym.start + el;
		etym.doff = ea;
		Bseek(eb, ea, 0);
		Bread(eb, etym.start, el);
		*etym.end = 0;
		robertprintentry(&def, &etym, cmd);
		free(etym.start);
	}
	free(def.start);
}

static void
robertprintentry(Entry *def, Entry *etym, int cmd)
{
	uchar *p, *pe;
	Rune r; int c, n;
	int baseline = 0;
	int lineno = 0;
	int cit = 0;

	if(suptab['0'] == 0)
		initsuptab();
	if(subtab['0'] == 0)
		initsubtab();

	p = (uchar *)def->start;
	pe = (uchar *)def->end;
	while(p < pe){
		if(cmd == 'r'){
			outchar(*p++);
			continue;
		}
		c = *p++;
		switch(r = intab[c]){	/* assign = */
		case BROM:
		case ITON:
		case ROM:
		case SYM:
		case HEL:
		case BHEL:
		case SMALL:
		case ITOFF:
		case NONE:
			if(debug)
				outprint("\\%.2ux", c);
			baseline = 0;
			break;

		case SUP:
			baseline = 1;
			break;

		case SUB:
			baseline = -1;
			break;

		case CIT:
			n = p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
			p += 4;
			if(debug)
				outprint("[%d]", n);
			while(*p == ' ' || ('0'<=*p && *p<='9') || *p == '.'){
				if(debug)
					outchar(*p);
				++p;
			}
			++cit;
			outnl(2);
			citation(n, cmd);
			baseline = 0;
			break;

		case '\n':
			outnl(0);
			baseline = 0;
			++lineno;
			break;

		default:
			if(baseline > 0 && r < nelem(suptab))
				r = suptab[r];
			else if(baseline < 0 && r < nelem(subtab))
				r = subtab[r];
			if(cit){
				outchar('\n');
				cit = 0;
			}
			outrune(r);
			baseline = 0;
			break;
		}
		if(r == '\n'){
			if(cmd == 'h')
				break;
			if(lineno == 1 && etym)
				robertprintentry(etym, 0, cmd);
		}
	}
	outnl(0);
}

static void
citation(int addr, int cmd)
{
	Entry cit;

	if(cb == 0)
		cb = Bouvrir(cfile);
	Bseek(cb, addr, 0);
	cit.start = Brdline(cb, 0xc8);
	cit.end = cit.start + Blinelen(cb) - 1;
	cit.doff = addr;
	*cit.end = 0;
	robertprintentry(&cit, 0, cmd);
}

long
robertnextoff(long fromoff)
{
	return (fromoff & ~15) + 16;
}

void
robertprintkey(void)
{
	Biobuf *db;
	char *l;

	db = Bouvrir(kfile);
	while(l = Brdline(db, '\n'))	/* assign = */
		Bwrite(bout, l, Blinelen(db));
	Bterm(db);
}

void
robertflexentry(Entry e, int cmd)
{
	uchar *p, *pe;
	Rune r; int c;
	int lineno = 1;

	p = (uchar *)e.start;
	pe = (uchar *)e.end;
	while(p < pe){
		if(cmd == 'r'){
			Bputc(bout, *p++);
			continue;
		}
		c = *p++;
		r = intab[c];
		if(r == '$')
			r = '\n';
		if(r == '\n'){
			++lineno;
			if(cmd == 'h' && lineno > 2)
				break;
		}
		if(cmd == 'h' && lineno < 2)
			continue;
		if(r > MULTIE){
			if(debug)
				Bprint(bout, "\\%.2ux", c);
			continue;
		}
		if(r < Runeself)
			Bputc(bout, r);
		else
			Bputrune(bout, r);
	}
	outnl(0);
}

long
robertnextflex(long fromoff)
{
	int c;

	if(Bseek(bdict, fromoff, 0) < 0)
		return -1;
	while((c = Bgetc(bdict)) >= 0){
		if(c == '$')
			return Boffset(bdict);
	}
	return -1;
}

static Biobuf *
Bouvrir(char *fichier)
{
	Biobuf *db;

	fichier = dictfile(fichier);
	db = Bopen(fichier, OREAD);
	if(db == 0){
		fprint(2, "%s: impossible d'ouvrir %s: %r\n", argv0, fichier);
		exits("ouvrir");
	}
	return db;
}
