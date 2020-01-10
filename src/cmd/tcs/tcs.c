#ifndef PLAN9
#include	<sys/types.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	"plan9.h"
#else /* PLAN9 */
#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#endif /* PLAN9 */
#include	"cyrillic.h"
#include	"misc.h"
#include	"ms.h"
#include	"8859.h"
#include	"big5.h"
#include	"gb.h"
#include	"hdr.h"
#include	"conv.h"

void usage(void);
void list(void);
int squawk = 1;
int clean = 0;
int verbose = 0;
long ninput, noutput, nrunes, nerrors;
char *file = "stdin";
char *argv0;
Rune runes[N];
char obuf[UTFmax*N];	/* maximum bloat from N runes */
long tab[NRUNE];
#ifndef	PLAN9
extern char version[];
#endif

void intable(int, long *, struct convert *);
void unicode_in(int, long *, struct convert *);
void unicode_out(Rune *, int, long *);

int
main(int argc, char **argv)
{
	char *from = "utf";
	char *to = "utf";
	int fd;
	int listem = 0;
	struct convert *t, *f;

	ARGBEGIN {
	case 'c':
		clean = 1;
		break;
	case 'f':
		from = EARGF(usage());
		break;
	case 'l':
		listem = 1;
		break;
	case 's':
		squawk = 0;
		break;
	case 't':
		to = EARGF(usage());
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage();
		break;
	} ARGEND

	USED(argc);
	if(verbose)
		squawk = 1;
	if(listem){
		list();
		EXIT(0, 0);
	}
	if(!from || !to)
		usage();
	f = conv(from, 1);
	t = conv(to, 0);
#define	PROC	{if(f->flags&Table)\
			intable(fd, (long *)f->data, t);\
		else\
			((Infn)(f->fn))(fd, (long *)0, t);}
	if(*argv){
		while(*argv){
			file = *argv;
#ifndef PLAN9
			if((fd = open(*argv, 0)) < 0){
				EPR "%s: %s: %s\n", argv0, *argv, strerror(errno));
#else /* PLAN9 */
			if((fd = open(*argv, OREAD)) < 0){
				EPR "%s: %s: %r\n", argv0, *argv);
#endif /* PLAN9 */
				EXIT(1, "open failure");
			}
			PROC
			close(fd);
			argv++;
		}
	} else {
		fd = 0;
		PROC
	}
	if(verbose)
		EPR "%s: %ld input bytes, %ld runes, %ld output bytes (%ld errors)\n", argv0,
			ninput, nrunes, noutput, nerrors);
	EXIT(((nerrors && squawk)? 1:0), ((nerrors && squawk)? "conversion error":0));
	return(0);	/* shut up compiler */
}

void
usage(void)
{
	EPR "Usage: %s [-slv] [-f cs] [-t cs] [file ...]\n", argv0);
	verbose = 1;
	list();
	EXIT(1, "usage");
}

void
list(void)
{
	struct convert *c;
	char ch = verbose?'\t':' ';

#ifndef	PLAN9
	EPR "%s version = '%s'\n", argv0, version);
#endif
	if(verbose)
		EPR "character sets:\n");
	else
		EPR "cs:");
	for(c = convert; c->name; c++){
		if((c->flags&From) && c[1].name && (strcmp(c[1].name, c->name) == 0)){
			EPR "%c%s", ch, c->name);
			c++;
		} else if(c->flags&Table)
			EPR "%c%s", ch, c->name);
		else if(c->flags&From)
			EPR "%c%s(from)", ch, c->name);
		else
			EPR "%c%s(to)", ch, c->name);
		if(verbose)
			EPR "\t%s\n", c->chatter);
	}
	if(!verbose)
		EPR "\n");
}

struct convert *
conv(char *name, int from)
{
	struct convert *c;

	for(c = convert; c->name; c++){
		if(cistrcmp(c->name, name) != 0)
			continue;
		if(c->flags&Table)
			return(c);
		if(((c->flags&From) == 0) == (from == 0))
			return(c);
	}
	EPR "%s: charset `%s' unknown\n", argv0, name);
	EXIT(1, "unknown character set");
	return(0);	/* just shut the compiler up */
}

void
swab2(char *b, int n)
{
	char *e, p;

	for(e = b+n; b < e; b++){
		p = *b;
		*b = b[1];
		*++b = p;
	}
}

void
unicode_in(int fd, long *notused, struct convert *out)
{
	u16int ubuf[N];
	Rune buf[N];
	int i, n;
	int swabme;

	USED(notused);
	if(read(fd, (char *)ubuf, 2) != 2)
		return;
	ninput += 2;
	switch(ubuf[0])
	{
	default:
		buf[0] = ubuf[0];
		OUT(out, buf, 1);
	case 0xFEFF:
		swabme = 0;
		break;
	case 0xFFFE:
		swabme = 1;
		break;
	}
	while((n = read(fd, (char *)ubuf, 2*N)) > 0){
		ninput += n;
		if(swabme)
			swab2((char *)ubuf, n);
		for(i=0; i<n/2; i++)
			buf[i] = ubuf[i];
		if(n&1){
			if(squawk)
				EPR "%s: odd byte count in %s\n", argv0, file);
			nerrors++;
			if(clean)
				n--;
			else
				buf[n++/2] = Runeerror;
		}
		OUT(out, buf, n/2);
	}
}

void
unicode_in_be(int fd, long *notused, struct convert *out)
{
	int i, n;
	u16int ubuf[N];
	Rune buf[N], r;
	uchar *p;

	USED(notused);
	while((n = read(fd, (char *)ubuf, 2*N)) > 0){
		ninput += n;
		p = (uchar*)ubuf;
		for(i=0; i<n/2; i++){
			r = *p++<<8;
			r |= *p++;
			buf[i] = r;
		}
		if(n&1){
			if(squawk)
				EPR "%s: odd byte count in %s\n", argv0, file);
			nerrors++;
			if(clean)
				n--;
			else
				buf[n++/2] = Runeerror;
		}
		OUT(out, buf, n/2);
	}
	OUT(out, buf, 0);
}

void
unicode_in_le(int fd, long *notused, struct convert *out)
{
	int i, n;
	u16int ubuf[N];
	Rune buf[N], r;
	uchar *p;

	USED(notused);
	while((n = read(fd, (char *)ubuf, 2*N)) > 0){
		ninput += n;
		p = (uchar*)ubuf;
		for(i=0; i<n/2; i++){
			r = *p++;
			r |= *p++<<8;
			buf[i] = r;
		}
		if(n&1){
			if(squawk)
				EPR "%s: odd byte count in %s\n", argv0, file);
			nerrors++;
			if(clean)
				n--;
			else
				buf[n++/2] = Runeerror;
		}
		OUT(out, buf, n/2);
	}
	OUT(out, buf, 0);
}

void
unicode_out(Rune *base, int n, long *notused)
{
	static int first = 1;
	u16int buf[N];
	int i;

	USED(notused);
	nrunes += n;
	if(first){
		u16int x = 0xFEFF;
		noutput += 2;
		write(1, (char *)&x, 2);
		first = 0;
	}
	noutput += 2*n;
	for(i=0; i<n; i++)
		buf[i] = base[i];
	write(1, (char *)buf, 2*n);
}

void
unicode_out_be(Rune *base, int n, long *notused)
{
	int i;
	uchar *p;
	Rune r;

	USED(notused);
	p = (uchar*)base;
	for(i=0; i<n; i++){
		r = base[i];
		*p++ = r>>8;
		*p++ = r;
	}
	nrunes += n;
	noutput += 2*n;
	write(1, (char *)base, 2*n);
}

void
unicode_out_le(Rune *base, int n, long *notused)
{
	int i;
	uchar *p;
	Rune r;

	USED(notused);
	p = (uchar*)base;
	for(i=0; i<n; i++){
		r = base[i];
		*p++ = r;
		*p++ = r>>8;
	}
	nrunes += n;
	noutput += 2*n;
	write(1, (char *)base, 2*n);
}

void
intable(int fd, long *table, struct convert *out)
{
	uchar buf[N];
	uchar *p, *e;
	Rune *r;
	int n;
	long c;

	while((n = read(fd, (char *)buf, N)) > 0){
		ninput += n;
		r = runes;
		for(p = buf, e = buf+n; p < e; p++){
			c = table[*p];
			if(c < 0){
				if(squawk)
					EPR "%s: bad char 0x%x near byte %ld in %s\n", argv0, *p, ninput+(p-e), file);
				nerrors++;
				if(clean)
					continue;
				c = BADMAP;
			}
			*r++ = c;
		}
		OUT(out, runes, r-runes);
	}
	OUT(out, runes, 0);
	if(n < 0){
#ifdef	PLAN9
		EPR "%s: input read: %r\n", argv0);
#else
		EPR "%s: input read: %s\n", argv0, strerror(errno));
#endif
		EXIT(1, "input read error");
	}
}

void
outtable(Rune *base, int n, long *map)
{
	long c;
	char *p;
	int i;

	nrunes += n;
	for(i = 0; i < NRUNE; i++)
		tab[i] = -1;
	for(i = 0; i < 256; i++)
		if(map[i] >= 0)
			tab[map[i]] = i;
	for(i = 0, p = obuf; i < n; i++){
		c = tab[base[i]];
		if(c < 0){
			if(squawk)
				EPR "%s: rune 0x%x not in output cs\n", argv0, base[i]);
			nerrors++;
			if(clean)
				continue;
			c = BADMAP;
		}
		*p++ = c;
	}
	noutput += p-obuf;
	write(1, obuf, p-obuf);
}

long tabascii[256] =
{
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1
};

long tabmsdos[256] =	/* from jhelling@cs.ruu.nl (Jeroen Hellingman) */
{
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, /* latin */
0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, /* forms */
0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, /* greek */
0x03a6, 0x0398, 0x2126, 0x03b4, 0x221e, 0x2205, 0x2208, 0x2229,
0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, /* math */
0x00b0, 0x2022, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x220e, 0x00a0
};
long tabmsdos2[256] =	/* from jhelling@cs.ruu.nl (Jeroen Hellingman) */
{
0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
0x25b6, 0x25c0, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x2043, 0x21a8,
0x2191, 0x2193, 0x2192, 0x2190, 0x2319, 0x2194, 0x25b2, 0x25bc,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, /* latin */
0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, /* forms */
0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, /* greek */
0x03a6, 0x0398, 0x2126, 0x03b4, 0x221e, 0x2205, 0x2208, 0x2229,
0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, /* math */
0x00b0, 0x2022, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x220e, 0x00a0
};
struct convert convert[] =
{	/* if two entries have the same name, put the from one first */
	{ "8859-1", "Latin-1 (Western and Northern Europe including Italian)", Table, (void *)tab8859_1 },
	{ "8859-2", "Latin-2 (Eastern Europe except Turkey and the Baltic countries)", Table, (void *)tab8859_2 },
	{ "8859-3", "Latin-3 (Mediterranean, South Africa, Esperanto)", Table, (void *)tab8859_3 },
	{ "8859-4", "Latin-4 (Scandinavia and the Baltic countries; obsolete)", Table, (void *)tab8859_4 },
	{ "8859-5", "Part 5 (Cyrillic)", Table, (void *)tab8859_5 },
	{ "8859-6", "Part 6 (Arabic)", Table, (void *)tab8859_6 },
	{ "8859-7", "Part 7 (Greek)", Table, (void *)tab8859_7 },
	{ "8859-8", "Part 8 (Hebrew)", Table, (void *)tab8859_8 },
	{ "8859-9", "Latin-5 (Turkey, Western Europe except Icelandic and Faroese)", Table, (void *)tab8859_9 },
	{ "8859-10", "Latin-6 (Northern Europe)", Table, (void *)tab8859_10 },
	{ "8859-15", "Latin-9 (Western Europe)", Table, (void *)tab8859_15 },
	{ "ascii", "7-bit ASCII", Table, (void *)tabascii },
	{ "atari", "ATARI-ST character set", Table, (void *)tabatari },
	{ "av", "Alternativnyj Variant", Table, (void *)tabav },
	{ "big5", "Big 5 (HKU)", From|Func, 0, (Fnptr)big5_in },
	{ "big5", "Big 5 (HKU)", Func, 0, (Fnptr)big5_out },
	{ "ebcdic", "EBCDIC", Table, (void *)tabebcdic },	/* 6f is recommended bad map */
	{ "euc-k", "Korean EUC: ASCII+KS C 5601 1987", From|Func, 0, (Fnptr)uksc_in },
	{ "euc-k", "Korean EUC: ASCII+KS C 5601 1987", Func, 0, (Fnptr)uksc_out },
	{ "gb2312", "GB2312-80 (Chinese)", From|Func, 0, (Fnptr)gb_in },
	{ "gb2312", "GB2312-80 (Chinese)", Func, 0, (Fnptr)gb_out },
	{ "html", "HTML", From|Func, 0, (Fnptr)html_in },
	{ "html", "HTML", Func, 0, (Fnptr)html_out },
	{ "ibm437", "IBM Code Page 437 (US)", Table, (void*)tabcp437 },
	{ "ibm720", "IBM Code Page 720 (Arabic)", Table, (void*)tabcp720 },
	{ "ibm737", "IBM Code Page 737 (Greek)", Table, (void*)tabcp737 },
	{ "ibm775", "IBM Code Page 775 (Baltic)", Table, (void*)tabcp775 },
	{ "ibm850", "IBM Code Page 850 (Multilingual Latin I)", Table, (void*)tabcp850 },
	{ "ibm852", "IBM Code Page 852 (Latin II)", Table, (void*)tabcp852 },
	{ "ibm855", "IBM Code Page 855 (Cyrillic)", Table, (void*)tabcp855 },
	{ "ibm857", "IBM Code Page 857 (Turkish)", Table, (void*)tabcp857 },
	{ "ibm858", "IBM Code Page 858 (Multilingual Latin I+Euro)", Table, (void*)tabcp858 },
	{ "ibm862", "IBM Code Page 862 (Hebrew)", Table, (void*)tabcp862 },
	{ "ibm866", "IBM Code Page 866 (Russian)", Table, (void*)tabcp866 },
	{ "ibm874", "IBM Code Page 874 (Thai)", Table, (void*)tabcp874 },
	{ "iso-2022-jp", "alias for jis-kanji (MIME)", From|Func, 0, (Fnptr)jisjis_in },
	{ "iso-2022-jp", "alias for jis-kanji (MIME)", Func, 0, (Fnptr)jisjis_out },
	{ "iso-8859-1", "alias for 8859-1 (MIME)", Table, (void *)tab8859_1 },
	{ "iso-8859-2", "alias for 8859-2 (MIME)", Table, (void *)tab8859_2 },
	{ "iso-8859-3", "alias for 8859-3 (MIME)", Table, (void *)tab8859_3 },
	{ "iso-8859-4", "alias for 8859-4 (MIME)", Table, (void *)tab8859_4 },
	{ "iso-8859-5", "alias for 8859-5 (MIME)", Table, (void *)tab8859_5 },
	{ "iso-8859-6", "alias for 8859-6 (MIME)", Table, (void *)tab8859_6 },
	{ "iso-8859-7", "alias for 8859-7 (MIME)", Table, (void *)tab8859_7 },
	{ "iso-8859-8", "alias for 8859-8 (MIME)", Table, (void *)tab8859_8 },
	{ "iso-8859-9", "alias for 8859-9 (MIME)", Table, (void *)tab8859_9 },
	{ "iso-8859-10", "alias for 8859-10 (MIME)", Table, (void *)tab8859_10 },
	{ "iso-8859-15", "alias for 8859-15 (MIME)", Table, (void *)tab8859_15 },
	{ "jis", "guesses at the JIS encoding", From|Func, 0, (Fnptr)jis_in },
	{ "jis-kanji", "ISO 2022-JP (Japanese)", From|Func, 0, (Fnptr)jisjis_in },
	{ "jis-kanji", "ISO 2022-JP (Japanese)", Func, 0, (Fnptr)jisjis_out },
	{ "koi8", "KOI-8 (GOST 19769-74)", Table, (void *)tabkoi8 },
	{ "koi8-r", "alias for koi8 (MIME)", Table, (void *)tabkoi8 },
	{ "latin1", "alias for 8859-1", Table, (void *)tab8859_1 },
	{ "macrom", "Macintosh Standard Roman character set", Table, (void *)tabmacroman },
	{ "microsoft", "alias for windows1252", Table, (void *)tabcp1252 },
	{ "ms-kanji", "Microsoft, or Shift-JIS", From|Func, 0, (Fnptr)msjis_in },
	{ "ms-kanji", "Microsoft, or Shift-JIS", Func, 0, (Fnptr)msjis_out },
	{ "msdos", "IBM PC (alias for ibm437)", Table, (void *)tabcp437 },
	{ "msdos2", "IBM PC (ibm437 with graphics in C0)", Table, (void *)tabmsdos2 },
	{ "next", "NEXTSTEP character set", Table, (void *)tabnextstep },
	{ "ov", "Osnovnoj Variant", Table, (void *)tabov },
	{ "ps2", "IBM PS/2: (alias for ibm850)", Table, (void *)tabcp850 },
	{ "sf1", "ISO-646: Finnish/Swedish SF-1 variant", Table, (void *)tabsf1 },
	{ "sf2", "ISO-646: Finnish/Swedish SF-2 variant (recommended)", Table, (void *)tabsf2 },
	{ "tis-620", "Thai+ASCII (TIS 620-1986)", Table, (void *)tabtis620 },
	{ "tune", "TUNE (Tamil)", From|Func, 0, (Fnptr)tune_in },
	{ "tune", "TUNE (Tamil)", Func, 0, (Fnptr)tune_out },
	{ "ucode", "Russian U-code", Table, (void *)tabucode },
	{ "ujis", "EUC-JX: JIS 0208", From|Func, 0, (Fnptr)ujis_in },
	{ "ujis", "EUC-JX: JIS 0208", Func, 0, (Fnptr)ujis_out },
	{ "unicode", "Unicode 1.1", From|Func, 0, (Fnptr)unicode_in },
	{ "unicode", "Unicode 1.1", Func, 0, (Fnptr)unicode_out },
	{ "unicode-be", "Unicode 1.1 big-endian", From|Func, 0, (Fnptr)unicode_in_be },
	{ "unicode-be", "Unicode 1.1 big-endian", Func, 0, (Fnptr)unicode_out_be },
	{ "unicode-le", "Unicode 1.1 little-endian", From|Func, 0, (Fnptr)unicode_in_le },
	{ "unicode-le", "Unicode 1.1 little-endian", Func, 0, (Fnptr)unicode_out_le },
	{ "us-ascii", "alias for ascii (MIME)", Table, (void *)tabascii },
	{ "utf", "FSS-UTF a.k.a. UTF-8", From|Func, 0, (Fnptr)utf_in },
	{ "utf", "FSS-UTF a.k.a. UTF-8", Func, 0, (Fnptr)utf_out },
	{ "utf1", "UTF-1 (ISO 10646 Annex A)", From|Func, 0, (Fnptr)isoutf_in },
	{ "utf1", "UTF-1 (ISO 10646 Annex A)", Func, 0, (Fnptr)isoutf_out },
	{ "utf-8", "alias for utf (MIME)", From|Func, 0, (Fnptr)utf_in },
	{ "utf-8", "alias for utf (MIME)", Func, 0, (Fnptr)utf_out },
	{ "utf-16", "alias for unicode (MIME)", From|Func, 0, (Fnptr)unicode_in },
	{ "utf-16", "alias for unicode (MIME)", Func, 0, (Fnptr)unicode_out },
	{ "utf-16be", "alias for unicode-be (MIME)", From|Func, 0, (Fnptr)unicode_in_be },
	{ "utf-16be", "alias for unicode-be (MIME)", Func, 0, (Fnptr)unicode_out_be },
	{ "utf-16le", "alias for unicode-le (MIME)", From|Func, 0, (Fnptr)unicode_in_le },
	{ "utf-16le", "alias for unicode-le (MIME)", Func, 0, (Fnptr)unicode_out_le },
	{ "viet1", "Vietnamese VSCII-1 (1993)", Table, (void *)tabviet1 },
	{ "viet2", "Vietnamese VSCII-2 (1993)", Table, (void *)tabviet2 },
	{ "vscii", "Vietnamese VISCII 1.1 (1992)", Table, (void *)tabviscii },
	{ "windows-1250", "Windows Code Page 1250 (Central Europe)", Table, (void *)tabcp1250 },
	{ "windows-1251", "Windows Code Page 1251 (Cyrillic)", Table, (void *)tabcp1251 },
	{ "windows-1252", "Windows Code Page 1252 (Latin I)", Table, (void *)tabcp1252 },
	{ "windows-1253", "Windows Code Page 1253 (Greek)", Table, (void *)tabcp1253 },
	{ "windows-1254", "Windows Code Page 1254 (Turkish)", Table, (void *)tabcp1254 },
	{ "windows-1255", "Windows Code Page 1255 (Hebrew)", Table, (void *)tabcp1255 },
	{ "windows-1256", "Windows Code Page 1256 (Arabic)", Table, (void *)tabcp1256 },
	{ "windows-1257", "Windows Code Page 1257 (Baltic)", Table, (void *)tabcp1257 },
	{ "windows-1258", "Windows Code Page 1258 (Vietnam)", Table, (void *)tabcp1258 },
	{ 0 }
};
