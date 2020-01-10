#include <stdio.h>
#include "tdef.h"
#include "fns.h"
#include "ext.h"

char	termtab[NS];	/* term type added in ptinit() */
char	fontdir[NS];	/* added in casefp; not used by nroff */
char	devname[20];	/* default output device */

Numtab numtab[NN] = {
	{ PAIR('%', 0) },
	{ PAIR('n', 'l') },
	{ PAIR('y', 'r') },
	{ PAIR('h', 'p') },
	{ PAIR('c', 't') },
	{ PAIR('d', 'n') },
	{ PAIR('m', 'o') },
	{ PAIR('d', 'y') },
	{ PAIR('d', 'w') },
	{ PAIR('l', 'n') },
	{ PAIR('d', 'l') },
	{ PAIR('s', 't') },
	{ PAIR('s', 'b') },
	{ PAIR('c', '.') },
	{ PAIR('$', '$') }
};


int	alphabet	= 256;	/* latin-1 */
int	pto	= 10000;
int	pfrom	= 1;
int	print	= 1;
char	nextf[NS]	= TMACDIR;
char	mfiles[NMF][NS];
int	nmfi	= 0;
int	oldbits	= -1;
int	init	= 1;
int	fc	= IMP;	/* field character */
int	eschar	= '\\';
int	pl;
int	po;
FILE	*ptid;

int	dfact	= 1;
int	dfactd	= 1;
int	res	= 1;
int	smnt	= 0;	/* beginning of special fonts */
int	ascii	= 0;	/* ascii normally off for troff, on for nroff;  -a turns on */
int	lg;
int	pnlist[NPN] = { -1 };


int	*pnp	= pnlist;
int	npn	= 1;
int	npnflg	=  1;
int	dpn	=  -1;
int	totout	=  1;
int	ulfont	=  ULFONT;
int	tabch	=  TAB;
int	ldrch	=  LEADER;


Contab contab[NM] = {
	C(PAIR('d', 's'), caseds),
	C(PAIR('a', 's'), caseas),
	C(PAIR('s', 'p'), casesp),
	C(PAIR('f', 't'), caseft),
	C(PAIR('p', 's'), caseps),
	C(PAIR('v', 's'), casevs),
	C(PAIR('n', 'r'), casenr),
	C(PAIR('i', 'f'), caseif),
	C(PAIR('i', 'e'), caseie),
	C(PAIR('e', 'l'), caseel),
	C(PAIR('p', 'o'), casepo),
	C(PAIR('t', 'l'), casetl),
	C(PAIR('t', 'm'), casetm),
	C(PAIR('f', 'm'), casefm),
	C(PAIR('b', 'p'), casebp),
	C(PAIR('c', 'h'), casech),
	C(PAIR('p', 'n'), casepn),
	C(PAIR('b', 'r'), tbreak),
	C(PAIR('t', 'i'), caseti),
	C(PAIR('n', 'e'), casene),
	C(PAIR('n', 'f'), casenf),
	C(PAIR('c', 'e'), casece),
	C(PAIR('f', 'i'), casefi),
	C(PAIR('i', 'n'), casein),
	C(PAIR('l', 'l'), casell),
	C(PAIR('n', 's'), casens),
	C(PAIR('m', 'k'), casemk),
	C(PAIR('r', 't'), casert),
	C(PAIR('a', 'm'), caseam),
	C(PAIR('d', 'e'), casede),
	C(PAIR('d', 'i'), casedi),
	C(PAIR('d', 'a'), caseda),
	C(PAIR('w', 'h'), casewh),
	C(PAIR('d', 't'), casedt),
	C(PAIR('i', 't'), caseit),
	C(PAIR('r', 'm'), caserm),
	C(PAIR('r', 'r'), caserr),
	C(PAIR('r', 'n'), casern),
	C(PAIR('a', 'd'), casead),
	C(PAIR('r', 's'), casers),
	C(PAIR('n', 'a'), casena),
	C(PAIR('p', 'l'), casepl),
	C(PAIR('t', 'a'), caseta),
	C(PAIR('t', 'r'), casetr),
	C(PAIR('u', 'l'), caseul),
	C(PAIR('c', 'u'), casecu),
	C(PAIR('l', 't'), caselt),
	C(PAIR('n', 'x'), casenx),
	C(PAIR('s', 'o'), caseso),
	C(PAIR('i', 'g'), caseig),
	C(PAIR('t', 'c'), casetc),
	C(PAIR('f', 'c'), casefc),
	C(PAIR('e', 'c'), caseec),
	C(PAIR('e', 'o'), caseeo),
	C(PAIR('l', 'c'), caselc),
	C(PAIR('e', 'v'), caseev),
	C(PAIR('r', 'd'), caserd),
	C(PAIR('a', 'b'), caseab),
	C(PAIR('f', 'l'), casefl),
	C(PAIR('e', 'x'), caseex),
	C(PAIR('s', 's'), casess),
	C(PAIR('f', 'p'), casefp),
	C(PAIR('c', 's'), casecs),
	C(PAIR('b', 'd'), casebd),
	C(PAIR('l', 'g'), caselg),
	C(PAIR('h', 'c'), casehc),
	C(PAIR('h', 'y'), casehy),
	C(PAIR('n', 'h'), casenh),
	C(PAIR('n', 'm'), casenm),
	C(PAIR('n', 'n'), casenn),
	C(PAIR('s', 'v'), casesv),
	C(PAIR('o', 's'), caseos),
	C(PAIR('l', 's'), casels),
	C(PAIR('c', 'c'), casecc),
	C(PAIR('c', '2'), casec2),
	C(PAIR('e', 'm'), caseem),
	C(PAIR('a', 'f'), caseaf),
	C(PAIR('h', 'a'), caseha),
	C(PAIR('h', 'w'), casehw),
	C(PAIR('m', 'c'), casemc),
	C(PAIR('p', 'm'), casepm),
	C(PAIR('p', 'i'), casepi),
	C(PAIR('u', 'f'), caseuf),
	C(PAIR('p', 'c'), casepc),
	C(PAIR('h', 't'), caseht),
	C(PAIR('c', 'f'), casecf),
	C(PAIR('s', 'y'), casesy),
	C(PAIR('l', 'f'), caself),
	C(PAIR('p', 't'), casept),
	C(PAIR('g', 'd'), casegd)
};


Tbuf _oline;

/*
 * troff environment block
 */

Env env[NEV] = { {	/* this sets up env[0] */
/* int	ics	 */	0,	/* insertion character space, set by .mc */
/* int	sps	 */	0,
/* int	spacesz	 */	0,
/* int	lss	 */	0,
/* int	lss1	 */	0,
/* int	ll	 */	0,
/* int	ll1	 */	0,
/* int	lt	 */	0,
/* int	lt1	 */	0,
/* Tchar ic	 */	0,	/* insertion character (= margin character) */
/* int	icf	 */	0,	/* insertion character flag */
/* Tchar chbits	 */	0,	/* size+font bits for current character */
/* Tchar spbits	 */	0,
/* Tchar nmbits	 */	0,	/* size+font bits for number from .nm */
/* int	apts	 */	PS,	/* actual point size -- as requested by user */
/* int	apts1	 */	PS,	/* need not match an existent size */
/* int	pts	 */	PS,	/* hence, this is the size that really exists */
/* int	pts1	 */	PS,
/* int	font	 */	FT,
/* int	font1	 */	FT,
/* int	ls	 */	1,
/* int	ls1	 */	1,
/* int	ad	 */	1,
/* int	nms	 */	1,	/* .nm multiplier */
/* int	ndf	 */	1,	/* .nm separator */
/* int	nmwid	 */	3,	/* max width of .nm numbers */
/* int	fi	 */	1,
/* int	cc	 */	'.',
/* int	c2	 */	'\'',
/* int	ohc	 */	OHC,
/* int	tdelim	 */	IMP,
/* int	hyf	 */	1,
/* int	hyoff	 */	0,
/* int	hyphalg  */	HYPHALG,
/* int	un1	 */	-1,
/* int	tabc	 */	0,
/* int	dotc	 */	'.',
/* int	adsp	 */	0,	/* add this much space to each padding point */
/* int	adrem	 */	0,	/* excess space to add until it runs out */
/* int	lastl	 */	0,	/* last text on current output line */
/* int	nel	 */	0,	/* how much space left on current output line */
/* int	admod	 */	0,	/* adjust mode */
/* Tchar *wordp	 */	0,
/* int	spflg	 */	0,	/* probably to indicate space after punctuation needed */
/* Tchar *linep	 */	0,
/* Tchar *wdend	 */	0,
/* Tchar *wdstart */	0,
/* int	wne	 */	0,
/* int	ne	 */	0,	/* how much space taken on current output line */
/* int	nc	 */	0,	/* #characters (incl blank) on output line */
/* int	nb	 */	0,
/* int	lnmod	 */	0,	/* line number mode, set by .nm */
/* int	nwd	 */	0,	/* number of words on current output line */
/* int	nn	 */	0,	/* from .nn command */
/* int	ni	 */	0,	/* indent of .nm numbers, probably */
/* int	ul	 */	0,
/* int	cu	 */	0,
/* int	ce	 */	0,
/* int	in	 */	0,	/* indent and previous value */
/* int	in1	 */	0,
/* int	un	 */	0,	/* unindent of left margin in some way */
/* int	wch	 */	0,
/* int	pendt	 */	0,
/* Tchar *pendw	 */	(Tchar *)0,
/* int	pendnf	 */	0,
/* int	spread	 */	0,
/* int	it	 */	0,	/* input trap count */
/* int	itmac	 */	0
} };

Env	*envp	= env;	/* start off in env 0 */

Numerr	numerr;

Stack	*frame, *stk, *ejl;
Stack	*nxf;

int	pipeflg;
int	hflg;	/* used in nroff only */
int	eqflg;	/* used in nroff only */

int	xpts;
int	ppts;
int	pfont;
int	mpts;
int	mfont;
int	cs;
int	ccs;
int	bd;

int	stdi;
int	quiet;
int	stop;
char	ibuf[IBUFSZ];
char	xbuf[IBUFSZ];
char	*ibufp;
char	*xbufp;
char	*eibuf;
char	*xeibuf;
Tchar	pbbuf[NC];		/* pushback buffer for arguments, \n, etc. */
Tchar	*pbp = pbbuf;		/* next free slot in pbbuf */
Tchar	*lastpbp = pbbuf;	/* pbp in previous stack frame */
int	nx;
int	mflg;
Tchar	ch = 0;
int	ibf;
int	ifi;
int	iflg;
int	rargc;
char	**argp;
Ushort	trtab[NTRTAB];
int	lgf;
int	copyf;
Offset	ip;
int	nlflg;
int	donef;
int	nflush;
int	nfo;
int	padc;
int	raw;
int	flss;
int	nonumb;
int	trap;
int	tflg;
int	ejf;
int	dilev;
Offset	offset;
int	em;
int	ds;
Offset	woff;
int	app;
int	ndone;
int	lead;
int	ralss;
Offset	nextb;
Tchar	nrbits;
int	nform;
int	oldmn;
int	newmn;
int	macerr;
Offset	apptr;
int	diflg;
int	evi;
int	vflag;
int	noscale;
int	po1;
int	nlist[NTRAP];
int	mlist[NTRAP];
int	evlist[EVLSZ];
int	ev;
int	tty;
int	sfont	= FT;	/* appears to be "standard" font; used by .ul */
int	sv;
int	esc;
int	widthp;
int	xfont;
int	setwdf;
int	over;
int	nhyp;
Tchar	**hyp;
Tchar	*olinep;
int	dotT;
char	*unlkp;
Wcache	widcache[NWIDCACHE];
Diver	d[NDI];
Diver	*dip;

int	c_hyphen;
int	c_emdash;
int	c_rule;
int	c_minus;
int	c_fi;
int	c_fl;
int	c_ff;
int	c_ffi;
int	c_ffl;
int	c_acute;
int	c_grave;
int	c_under;
int	c_rooten;
int	c_boxrule;
int	c_lefthand;
int	c_dagger;
int	c_isalnum;

Spnames	spnames[] =
{
	&c_hyphen,	"hy",
	&c_emdash,	"em",
	&c_rule,	"ru",
	&c_minus,	"\\-",
	&c_fi,		"fi",
	&c_fl,		"fl",
	&c_ff,		"ff",
	&c_ffi,		"Fi",
	&c_ffl,		"Fl",
	&c_acute,	"aa",
	&c_grave,	"ga",
	&c_under,	"ul",
	&c_rooten,	"rn",
	&c_boxrule,	"br",
	&c_lefthand,	"lh",
	&c_dagger,	"dg",	/* not in nroff?? */
	&c_isalnum,	"__",
	0, 0
};


Tchar	(*hmot)(void);
Tchar	(*makem)(int i);
Tchar	(*setabs)(void);
Tchar	(*setch)(int c);
Tchar	(*sethl)(int k);
Tchar	(*setht)(void);
Tchar	(*setslant)(void);
Tchar	(*vmot)(void);
Tchar	(*xlss)(void);
int	(*findft)(int i);
int	(*width)(Tchar j);
void	(*mchbits)(void);
void	(*ptlead)(void);
void	(*ptout)(Tchar i);
void	(*ptpause)(void);
void	(*setfont)(int a);
void	(*setps)(void);
void	(*setwd)(void);
