#define	devname	p9_devname

extern	int	TROFF;

extern	int	alphabet;
extern	char	**argp;
extern	char	*eibuf;
extern	char	*ibufp;
extern	char	*obufp;
extern	char	*unlkp;
extern	char	*xbufp;
extern	char	*xeibuf;
extern	char	cfname[NSO+1][NS];
extern  int	trace;
extern	char	devname[];
extern	char	ibuf[IBUFSZ];
extern	char	mfiles[NMF][NS];
extern	char	nextf[];
extern	char	obuf[];
extern	char	termtab[];
extern	char	fontdir[];
extern	Font	fonts[MAXFONTS+1];
extern	char	xbuf[IBUFSZ];
extern	Offset	apptr;
extern	Offset	ip;
extern	Offset	nextb;
extern	Offset	offset;
extern	Offset	woff;
extern	Numerr	numerr;
extern	int	*pnp;
extern	int	pstab[];
extern	int	nsizes;
extern	int	app;
extern	int	ascii;
extern	int	bd;
extern	int	bdtab[];
extern	int	ccs;
extern	char	*chnames[];	/* chnames[n-ALPHABET] -> name of char n */
extern	int	copyf;
extern	int	cs;
extern	int	dfact;
extern	int	dfactd;
extern	int	diflg;
extern	int	dilev;
extern	int	donef;
extern	int	dotT;
extern	int	dpn;
extern	int	ds;
extern	int	ejf;
extern	int	em;
extern	int	eqflg;
extern	int	error;
extern	int	esc;
extern	int	eschar;
extern	int	ev;
extern	int	evi;
extern	int	evlist[EVLSZ];
extern	int	fc;
extern	int	flss;
extern	int	fontlab[];
extern	int	hflg;
extern	int	ibf;
extern	int	ifi;
extern	int	iflg;
extern	int	init;
extern	int	lead;
extern	int	lg;
extern	int	lgf;
extern	int	macerr;
extern	int	mflg;
extern	int	mfont;
extern	int	mlist[NTRAP];
extern	int	mpts;
extern	int	nchnames;
extern	int	ndone;
extern	int	newmn;
extern	int	nflush;
extern	int	nfo;
extern	int	nfonts;
extern	int	nform;
extern	int	nhyp;
extern	int	nlflg;
extern	int	nlist[NTRAP];
extern	int	nmfi;
extern	int	nonumb;
extern	int	noscale;
extern	int	npn;
extern	int	npnflg;
extern	int	nx;
extern	int	oldbits;
extern	int	oldmn;
extern	int	over;
extern	int	padc;
extern	int	pfont;
extern	int	pfrom;
extern	int	pipeflg;
extern	int	pl;
extern	int	pnlist[];
extern	int	po1;
extern	int	po;
extern	int	ppts;
#define	print	troffprint
extern	int	print;
extern	FILE	*ptid;
extern	int	pto;
extern	int	quiet;
extern	int	ralss;
extern	int	rargc;
extern	int	raw;
extern	int	res;
extern	int	sbold;
extern	int	setwdf;
extern	int	sfont;
extern	int	smnt;
extern	int	stdi;
extern	int	stop;
extern	int	sv;
extern	int	tabch,	ldrch;
extern	int	tflg;
extern	int	totout;
extern	int	trap;
extern	Ushort	trtab[];
extern	int	tty;
extern	int	ulfont;
extern	int	vflag;
extern	int	whichroff;
extern	int	widthp;
extern	int	xfont;
extern	int	xpts;
extern	Stack	*ejl;
extern	Stack	*frame;
extern	Stack	*stk;
extern	Stack	*nxf;
extern	Tchar	**hyp;
extern	Tchar	*olinep;
extern	Tchar	pbbuf[NC];
extern	Tchar	*pbp;
extern	Tchar	*lastpbp;
extern	Tchar	ch;
extern	Tchar	nrbits;
extern	Tbuf	_oline;
extern	Wcache	widcache[];
extern	char	gchtab[];
extern	Diver	d[NDI];
extern	Diver	*dip;


extern	char	xchname[];
extern	short	xchtab[];
extern	char	*codestr;
extern	char	*chnamep;
extern	short	*chtab;
extern	int	nchtab;

extern Numtab *numtabp;

/* these characters are used as various signals or values
/* in miscellaneous places.
/* values are set in specnames in t10.c
*/

extern int	c_hyphen;
extern int	c_emdash;
extern int	c_rule;
extern int	c_minus;
extern int	c_fi;
extern int	c_fl;
extern int	c_ff;
extern int	c_ffi;
extern int	c_ffl;
extern int	c_acute;
extern int	c_grave;
extern int	c_under;
extern int	c_rooten;
extern int	c_boxrule;
extern int	c_lefthand;
extern int	c_dagger;
extern int	c_isalnum;

/*
 * String pointers for DWB pathname management.
 */

extern char	*DWBfontdir;
extern char	*DWBntermdir;
extern char	*DWBalthyphens;
