#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#undef MB_CUR_MAX
#define MB_CUR_MAX 3

#define	NROFF	(!TROFF)

/* Site dependent definitions */

#ifndef TMACDIR
#define TMACDIR		"lib/tmac/tmac."
#endif
#ifndef FONTDIR
#define FONTDIR		"lib/font"
#endif
#ifndef NTERMDIR
#define NTERMDIR	"lib/term/tab."
#endif
#ifndef TDEVNAME
#define TDEVNAME	"post"
#endif
#ifndef NDEVNAME
#define NDEVNAME	"37"
#endif
#ifndef TEXHYPHENS
#define	TEXHYPHENS	"/usr/lib/tex/macros/hyphen.tex"
#endif
#ifndef ALTHYPHENS
#define	ALTHYPHENS	"lib/tmac/hyphen.tex"	/* another place to look */
#endif

typedef	unsigned char	Uchar;
typedef unsigned short	Ushort;

typedef	/*unsigned*/ long	Tchar;

typedef	struct	Blockp	Blockp;
typedef	struct	Diver	Diver;
typedef	struct	Stack	Stack;
typedef	struct	Divsiz	Divsiz;
typedef	struct	Contab	Contab;
typedef	struct	Numtab	Numtab;
typedef	struct	Numerr	Numerr;
typedef	struct	Env	Env;
typedef	struct	Term	Term;
typedef struct	Chwid	Chwid;
typedef struct	Font	Font;
typedef	struct	Spnames	Spnames;
typedef	struct	Wcache	Wcache;
typedef	struct	Tbuf	Tbuf;

/* this simulates printf into a buffer that gets flushed sporadically */
/* the BSD goo is because SunOS sprintf doesn't return anything useful */

#ifdef BSD4_2
#define	OUT		(obufp += strlen(sprintf(obufp,
#define	PUT		))) > obuf+BUFSIZ ? flusho() : 1
#else
#define	OUT		(obufp += sprintf(obufp,
#define	PUT		)) > obuf+BUFSIZ ? flusho() : 1
#endif

#define	oputs(a)	OUT "%s", a PUT
#define	oput(c)		( *obufp++ = (c), obufp > obuf+BUFSIZ ? flusho() : 1 )

#undef	sprintf	/* Snow Leopard */

extern	char	errbuf[];
#define	ERROR	sprintf(errbuf,
#define	WARN	), errprint()
#define	FATAL	), errprint(), exit(1)

/* starting values for typesetting parameters: */

#define	PS	10	/* default point size */
#define	FT	1	/* default font position */
#define ULFONT	2	/* default underline font */
#define	BDFONT	3	/* default emboldening font */
#define	BIFONT	4	/* default bold italic font */
#define	LL	(unsigned) 65*INCH/10	/* line length; 39picas=6.5in */
#define	VS	((12*INCH)/72)	/* initial vert space */


#define	EMPTS(pts)	(((long)Inch*(pts) + 36) / 72)
#define	EM	(TROFF? EMPTS(pts): t.Em)
#define	INCH	(TROFF? Inch: 240)
#define	HOR	(TROFF? Hor: t.Adj)
#define	VERT	(TROFF? Vert: t.Vert)
#define	PO	(TROFF? Inch: 0)
#define	SPS	(TROFF? EMPTS(pts)/3: INCH/10)
#define	SS	(TROFF? 12: INCH/10)
#define	ICS	(TROFF? EMPTS(pts): 2*INCH/10)
#define	DTAB	(TROFF? (INCH/2): 0)

/* These "characters" are used to encode various internal functions
/* Some make use of the fact that most ascii characters between
/* 0 and 040 don't have any graphic or other function.
/* The few that do have a purpose (e.g., \n, \b, \t, ...
/* are avoided by the ad hoc choices here.
/* See ifilt[] in n1.c for others -- 1, 2, 3, 5, 6, 7, 010, 011, 012
*/

#define	LEADER	001
#define	IMP	004	/* impossible char; glues things together */
#define	TAB	011
#define	RPT	014	/* next character is to be repeated many times */
#define	CHARHT	015	/* size field sets character height */
#define	SLANT	016	/* size field sets amount of slant */
#define	DRAWFCN	017	/* next several chars describe arb drawing fcns */
#	define	DRAWLINE	'l'	/* line: 'l' dx dy char */
#	define	DRAWCIRCLE	'c'	/* circle: 'c' r */
#	define	DRAWELLIPSE	'e'	/* ellipse: 'e' rx ry */
#	define	DRAWARC		'a'	/* arc: 'a' dx dy dx dy */
#	define	DRAWSPLINE	'~'	/* quadratic B spline: '~' dx dy dx dy ... */
					/* other splines go thru too */
/* NOTE:  the use of ~ is a botch since it's often used in .tr commands */
/* better to use a letter like s, but change it in the postprocessors too */
/* for now, this is taken care of in n9.c and t10.c */
#	define	DRAWBUILD	'b'	/* built-up character (e.g., { */

#define	LEFT	020	/* \{ */
#define	RIGHT	021	/* \} */
#define	FILLER	022	/* \& and similar purposes */
#define	XON	023	/* \X'...' starts here */
#define	OHC	024	/* optional hyphenation character \% */
#define	CONT	025	/* \c character */
#define	PRESC	026	/* printable escape */
#define	UNPAD	027	/* unpaddable blank */
#define	XPAR	030	/* transparent mode indicator */
#define	FLSS	031	/* next Tchar contains vertical space */
			/* used when recalling diverted text */
#define	WORDSP	032	/* paddable word space */
#define	ESC	033	/* current escape character */
#define	XOFF	034	/* \X'...' ends here */
			/* matches XON, but they will probably never nest */
			/* so could drop this when another control is needed */
#define	HX	035	/* next character is value of \x'...' */
#define MOTCH	036	/* this "character" is really motion; used by cbits() */

#define	HYPHEN	c_hyphen
#define	EMDASH	c_emdash	/* \(em */
#define	RULE	c_rule		/* \(ru */
#define	MINUS	c_minus		/* minus sign on current font */
#define	LIG_FI	c_fi		/* \(ff */
#define	LIG_FL	c_fl		/* \(fl */
#define	LIG_FF	c_ff		/* \(ff */
#define	LIG_FFI	c_ffi		/* \(Fi */
#define	LIG_FFL	c_ffl		/* \(Fl */
#define	ACUTE	c_acute		/* acute accent \(aa */
#define	GRAVE	c_grave		/* grave accent \(ga */
#define	UNDERLINE c_under	/* \(ul */
#define	ROOTEN	c_rooten	/* root en \(rn */
#define	BOXRULE	c_boxrule	/* box rule \(br */
#define	LEFTHAND c_lefthand	/* left hand for word overflow */
#define	DAGGER	 c_dagger	/* dagger for end of sentence/footnote */

#define	HYPHALG	1	/* hyphenation algorithm: 0=>good old troff, 1=>tex */


/* array sizes, and similar limits: */

#define MAXFONTS 99	/* Maximum number of fonts in fontab */
#define	NM	91	/* requests + macros */
#define	NN	NNAMES	/* number registers */
#define	NNAMES	15	/* predefined reg names */
#define	NIF	15	/* if-else nesting */
#define	NS	128	/* name buffer */
#define	NTM	1024	/* tm buffer */
#define	NEV	3	/* environments */
#define	EVLSZ	10	/* size of ev stack */

#define	STACKSIZE (12*1024)	/* stack for macros and strings in progress */
#define	NHYP	10	/* max hyphens per word */
#define	NHEX	512	/* byte size of exception word list */
#define	NTAB	100	/* tab stops */
#define	NSO	5	/* "so" depth */
#define	NMF	5	/* number of -m flags */
#define	WDSIZE	500	/* word buffer click size */
#define	LNSIZE	4000	/* line buffer click size */
#define	OLNSIZE	5000	/* output line buffer click; bigger for 'w', etc. */
#define	NDI	5	/* number of diversions */

#define	ALPHABET alphabet	/* number of characters in basic alphabet. */
			/* 128 for parochial USA 7-bit ascii, */
			/* 256 for "European" mode with e.g., Latin-1 */

	/* NCHARS must be greater than
		ALPHABET (ascii stuff) + total number of distinct char names
		from all fonts that will be run in this job (including
		unnamed ones and \N's)
	*/

#define	NCHARS	(8*1024)	/* maximum size of troff character set*/


	/* However for nroff you want only :
	1. number of special codes in charset of DESC, which ends up being the
		value of nchtab and which must be less than 512.
	2. ALPHABET, which apparently is the size of the portion of the tables reserved
		for special control symbols
	Apparently the max N of \N is irrelevant; */
	/* to allow \N of up to 254 with up to 338 special characters
		you need NCHARS of 338 + ALPHABET = 466 */

#define	NROFFCHARS	1024	/* maximum size of nroff character set */

#define	NTRTAB		NCHARS	/* number of items in trtab[] */
#define NWIDCACHE	NCHARS	/* number of items in widcache[] */

#define	NTRAP	20	/* number of traps */
#define	NPN	20	/* numbers in "-o" */
#define	FBUFSZ	512	/* field buf size words */
#define	IBUFSZ	4096	/* bytes */
#define	NC	1024	/* cbuf size words */
#define	NOV	10	/* number of overstrike chars */
#define	NPP	10	/* pads per field */

/*
	Internal character representation:
	Internally, every character is carried around as
	a 32 bit cookie, called a "Tchar" (typedef long).
	Bits are numbered 31..0 from left to right.
	If bit 15 is 1, the character is motion, with
		if bit 16 it's vertical motion
		if bit 17 it's negative motion
	If bit 15 is 0, the character is a real character.
		if bit 31	zero motion
		bits 30..24	size
		bits 23..16	font
*/

/* in the following, "L" should really be a Tchar, but ... */
/* numerology leaves room for 16 bit chars */

#define	MOT	(01uL << 16)	/* motion character indicator */
#define	VMOT	(01uL << 30)	/* vertical motion bit */
#define	NMOT	(01uL << 29)	/* negative motion indicator */
/* #define	MOTV	(MOT|VMOT|NMOT)	/* motion flags */
/* #define	MAXMOT	(~MOTV)		/* maximum motion permitted */
#define	MAXMOT	0xFFFF

#define	ismot(n)	((n) & MOT)
#define	isvmot(n)	(((n) & (MOT|VMOT)) == (MOT|VMOT))	/* must have tested MOT previously */
#define	isnmot(n)	(((n) & (MOT|NMOT)) == (MOT|NMOT))	/* ditto */
#define	absmot(n)	((n) & 0xFFFF)

#define	ZBIT	(01uL << 31)	/* zero width char */
#define	iszbit(n)	((n) &  ZBIT)

#define	FSHIFT	17
#define	SSHIFT	(FSHIFT+7)
#define	SMASK		(0177uL << SSHIFT)	/* 128 distinct sizes */
#define	FMASK		(0177uL << FSHIFT)	/* 128 distinct fonts */
#define	SFMASK		(SMASK|FMASK)	/* size and font in a Tchar */
#define	sbits(n)	(((n) >> SSHIFT) & 0177)
#define	fbits(n)	(((n) >> FSHIFT) & 0177)
#define	sfbits(n)	(((n) & SFMASK) >> FSHIFT)
#define	cbits(n)	((n) & 0x1FFFF)		/* isolate character bits,  */
						/* but don't include motions */
extern	int	realcbits(Tchar);

#define	setsbits(n,s)	n = (n & ~SMASK) | (Tchar)(s) << SSHIFT
#define	setfbits(n,f)	n = (n & ~FMASK) | (Tchar)(f) << FSHIFT
#define	setsfbits(n,sf)	n = (n & ~SFMASK) | (Tchar)(sf) << FSHIFT
#define	setcbits(n,c)	n = (n & ~0xFFFFuL | (c))	/* set character bits */

#define	BYTEMASK 0377
#define	BYTE	 8

#define	SHORTMASK 0XFFFF
#define	SHORT	 16

#define	TABMASK	 ((unsigned) INT_MAX >> 1)
#define	RTAB	((TABMASK << 1) & ~TABMASK)
#define	CTAB	(RTAB << 1)

#define	TABBIT	02		/* bits in gchtab */
#define	LDRBIT	04
#define	FCBIT	010

#define	PAIR(A,B)	(A|(B<<SHORT))


extern	int	Inch, Hor, Vert, Unitwidth;

struct	Spnames
{
	int	*n;
	char	*v;
};

extern	Spnames	spnames[];

/*
	String and macro definitions are stored conceptually in a giant array
	indexed by type Offset.  In olden times, this array was real, and thus
	both huge and limited in size, leading to the "Out of temp file space"
	error.  In this version, the array is represented by a list of blocks,
	pointed to by blist[].bp.  Each block is of size BLK Tchars, and BLK
	MUST be a power of 2 for the macros below to work.

	The blocks associated with a particular string or macro are chained
	together in the array blist[].  Each blist[i].nextoff contains the
	Offset associated with the next block in the giant array, or -1 if
	this is the last block in the chain.  If .nextoff is 0, the block is
	free.

	To find the right index in blist for an Offset, divide by BLK.
*/

#define	NBLIST	2048	/* starting number of blocks in all definitions */

#define	BLK	128	/* number of Tchars in a block; must be 2^N with defns below */

#define	rbf0(o)		(blist[bindex(o)].bp[boffset(o)])
#define	bindex(o)	((o) / BLK)
#define	boffset(o)	((o) & (BLK-1))
#define	pastend(o)	(((o) & (BLK-1)) == 0)
/* #define	incoff(o)	( (++o & (BLK-1)) ? o : blist[bindex(o-1)].nextoff ) */
#define	incoff(o)	( (((o)+1) & (BLK-1)) ? o+1 : blist[bindex(o)].nextoff )

#define	skipline(f)	while (getc(f) != '\n')
#define is(s)		(strcmp(cmd, s) == 0)
#define	eq(s1, s2)	(strcmp(s1, s2) == 0)


typedef	unsigned long	Offset;		/* an offset in macro/string storage */

struct Blockp {		/* info about a block: */
	Tchar	*bp;		/* the data */
	Offset	nextoff;	/* offset of next block in a chain */
};

extern	Blockp	*blist;

#define	RD_OFFSET	(1 * BLK)	/* .rd command uses block 1 */

struct Diver {		/* diversion */
	Offset	op;
	int	dnl;
	int	dimac;
	int	ditrap;
	int	ditf;
	int	alss;
	int	blss;
	int	nls;
	int	mkline;
	int	maxl;
	int	hnl;
	int	curd;
};

struct Stack {		/* stack frame */
	int	nargs;
	Stack	*pframe;
	Offset	pip;
	int	pnchar;
	Tchar	prchar;
	int	ppendt;
	Tchar	pch;
	Tchar	*lastpbp;
	int	mname;
};

extern	Stack	s;

struct Divsiz {
	int dix;
	int diy;
};

struct Contab {		/* command or macro */
	unsigned int	rq;
	Contab	*link;
	void	(*f)(void);
	Offset	mx;
	Offset	emx;
	Divsiz	*divsiz;
};

#define	C(a,b)	{a, 0, b, 0, 0}		/* how to initialize a contab entry */

extern	Contab	contab[NM];

struct Numtab {	/* number registers */
	unsigned int	r;		/* name */
	int	val;
	short	fmt;
	short	inc;
	Numtab	*link;
};

extern	Numtab	numtab[NN];

#define	PN	0
#define	NL	1
#define	YR	2
#define	HP	3
#define	CT	4
#define	DN	5
#define	MO	6
#define	DY	7
#define	DW	8
#define	LN	9
#define	DL	10
#define	ST	11
#define	SB	12
#define	CD	13
#define	PID	14

struct	Wcache {	/* width cache, indexed by character */
	short	fontpts;
	short	width;
};

struct	Tbuf {		/* growable Tchar buffer */
	Tchar *_bufp;
	unsigned int _size;
};

/* the infamous environment block */

#define	ics	envp->_ics
#define	sps	envp->_sps
#define	spacesz	envp->_spacesz
#define	lss	envp->_lss
#define	lss1	envp->_lss1
#define	ll	envp->_ll
#define	ll1	envp->_ll1
#define	lt	envp->_lt
#define	lt1	envp->_lt1
#define	ic	envp->_ic
#define	icf	envp->_icf
#define	chbits	envp->_chbits
#define	spbits	envp->_spbits
#define	nmbits	envp->_nmbits
#define	apts	envp->_apts
#define	apts1	envp->_apts1
#define	pts	envp->_pts
#define	pts1	envp->_pts1
#define	font	envp->_font
#define	font1	envp->_font1
#define	ls	envp->_ls
#define	ls1	envp->_ls1
#define	ad	envp->_ad
#define	nms	envp->_nms
#define	ndf	envp->_ndf
#define	nmwid	envp->_nmwid
#define	fi	envp->_fi
#define	cc	envp->_cc
#define	c2	envp->_c2
#define	ohc	envp->_ohc
#define	tdelim	envp->_tdelim
#define	hyf	envp->_hyf
#define	hyoff	envp->_hyoff
#define	hyphalg	envp->_hyphalg
#define	un1	envp->_un1
#define	tabc	envp->_tabc
#define	dotc	envp->_dotc
#define	adsp	envp->_adsp
#define	adrem	envp->_adrem
#define	lastl	envp->_lastl
#define	nel	envp->_nel
#define	admod	envp->_admod
#define	wordp	envp->_wordp
#define	spflg	envp->_spflg
#define	linep	envp->_linep
#define	wdend	envp->_wdend
#define	wdstart	envp->_wdstart
#define	wne	envp->_wne
#define	ne	envp->_ne
#define	nc	envp->_nc
#define	nb	envp->_nb
#define	lnmod	envp->_lnmod
#define	nwd	envp->_nwd
#define	nn	envp->_nn
#define	ni	envp->_ni
#define	ul	envp->_ul
#define	cu	envp->_cu
#define	ce	envp->_ce
#define	in	envp->_in
#define	in1	envp->_in1
#define	un	envp->_un
#define	wch	envp->_wch
#define	pendt	envp->_pendt
#define	pendw	envp->_pendw
#define	pendnf	envp->_pendnf
#define	spread	envp->_spread
#define	it	envp->_it
#define	itmac	envp->_itmac
#define	hyptr	envp->_hyptr
#define	tabtab	envp->_tabtab
#define	line	envp->_line._bufp
#define	lnsize	envp->_line._size
#define	word	envp->_word._bufp
#define wdsize	envp->_word._size

#define oline	_oline._bufp
#define olnsize	_oline._size

/*
 * Note:
 * If this structure changes in ni.c, you must change
 * this as well, and vice versa.
 */

struct Env {
	int	_ics;
	int	_sps;
	int	_spacesz;
	int	_lss;
	int	_lss1;
	int	_ll;
	int	_ll1;
	int	_lt;
	int	_lt1;
	Tchar	_ic;
	int	_icf;
	Tchar	_chbits;
	Tchar	_spbits;
	Tchar	_nmbits;
	int	_apts;
	int	_apts1;
	int	_pts;
	int	_pts1;
	int	_font;
	int	_font1;
	int	_ls;
	int	_ls1;
	int	_ad;
	int	_nms;
	int	_ndf;
	int	_nmwid;
	int	_fi;
	int	_cc;
	int	_c2;
	int	_ohc;
	int	_tdelim;
	int	_hyf;
	int	_hyoff;
	int	_hyphalg;
	int	_un1;
	int	_tabc;
	int	_dotc;
	int	_adsp;
	int	_adrem;
	int	_lastl;
	int	_nel;
	int	_admod;
	Tchar	*_wordp;
	int	_spflg;
	Tchar	*_linep;
	Tchar	*_wdend;
	Tchar	*_wdstart;
	int	_wne;
	int	_ne;
	int	_nc;
	int	_nb;
	int	_lnmod;
	int	_nwd;
	int	_nn;
	int	_ni;
	int	_ul;
	int	_cu;
	int	_ce;
	int	_in;
	int	_in1;
	int	_un;
	int	_wch;
	int	_pendt;
	Tchar	*_pendw;
	int	_pendnf;
	int	_spread;
	int	_it;
	int	_itmac;
	Tchar	*_hyptr[NHYP];
	long	_tabtab[NTAB];
	Tbuf	_line;
	Tbuf	_word;
};

extern	Env	env[];
extern	Env	*envp;

enum {	MBchar = 'U', Troffchar = 'C', Number = 'N', Install = 'i', Lookup = 'l' };
	/* U => utf, for instance;  C => \(xx, N => \N'...' */



struct Chwid {	/* data on one character */
	Ushort	num;		/* character number:
					0 -> not on this font
					>= ALPHABET -> its number among all Cxy's */
	Ushort	code;		/* char code for actual device.  used for \N */
	char	*str;		/* code string for nroff */
	Uchar	wid;		/* width */
	Uchar	kern;		/* ascender/descender */
};

struct Font {	/* characteristics of a font */
	int	name;		/* int name, e.g., BI (2 chars) */
	char	longname[64];	/* long name of this font (e.g., "Bembo" */
	char	*truename;	/* path name of table if not in standard place */
	int	nchars;		/* number of width entries for this font */
	char	specfont;	/* 1 == special font */
	int	spacewidth;	/* width of space on this font */
	int	defaultwidth;	/* default width of characters on this font */
	Chwid	*wp;		/* widths, etc., of the real characters */
	char	ligfont;	/* 1 == ligatures exist on this font */
};

/* ligatures, ORed into ligfont */

#define	LFF	01
#define	LFI	02
#define	LFL	04
#define	LFFI	010
#define	LFFL	020

/* tracing modes */
#define TRNARGS	01		/* trace legality of numeric arguments */
#define TRREQ	02		/* trace requests */
#define TRMAC	04		/* trace macros */
#define RQERR	01		/* processing request/macro */

/* typewriter driving table structure */


extern	Term	t;
struct Term {
	int	bset;		/* these bits have to be on */
	int	breset;		/* these bits have to be off */
	int	Hor;		/* #units in minimum horiz motion */
	int	Vert;		/* #units in minimum vert motion */
	int	Newline;	/* #units in single line space */
	int	Char;		/* #units in character width */
	int	Em;		/* ditto */
	int	Halfline;	/* half line units */
	int	Adj;		/* minimum units for horizontal adjustment */
	char	*twinit;	/* initialize terminal */
	char	*twrest;	/* reinitialize terminal */
	char	*twnl;		/* terminal sequence for newline */
	char	*hlr;		/* half-line reverse */
	char	*hlf;		/* half-line forward */
	char	*flr;		/* full-line reverse */
	char	*bdon;		/* turn bold mode on */
	char	*bdoff;		/* turn bold mode off */
	char	*iton;		/* turn italic mode on */
	char	*itoff;		/* turn italic mode off */
	char	*ploton;	/* turn plot mode on */
	char	*plotoff;	/* turn plot mode off */
	char	*up;		/* sequence to move up in plot mode */
	char	*down;		/* ditto */
	char	*right;		/* ditto */
	char	*left;		/* ditto */

	Font	tfont;		/* widths and other info, as in a troff font */
};

extern	Term	t;

/*
 * for error reporting; keep track of escapes/requests with numeric arguments
 */
struct Numerr {
	char	type;	/* request or escape? */
	char	esc;	/* was escape sequence named esc */
	char	escarg;	/* argument of esc's like \D'l' */
	unsigned int	req;	/* was request or macro named req */
};
