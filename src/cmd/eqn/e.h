#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#undef sqrt
#define sqrt p9_sqrt
#define YYSTYPE uintptr_t  /* DEC v. GNU garbage */

enum charclass {
	OTHER, OLET, ILET, DIG, LPAR, RPAR, SLASH, PLUS, ILETF, ILETJ, VBAR,
	NONE, LAST
};
extern int class[LAST][LAST];

#define	dprintf		if (dbg) printf
#define	max(x,y)	(((x) >= (y)) ? (x) : (y))	/* beware of side effects */
#define	min(x,y)	(((x) <= (y)) ? (x) : (y))

#undef	sprintf	/* Snow Leopard */

extern	char	errbuf[2000];
extern	char	*cmdname;
#define	ERROR	sprintf(errbuf,
#define	FATAL	), error(1, errbuf)
#define	WARNING	), error(0, errbuf)
#define	SYNTAX	), yyerror(errbuf)

#define	ROM	'1'
#define	ITAL	'2'
#define	BLD	'3'
#define	BDIT	'4'

#define	DEFGAP	-999	/* default gap in piles */

extern int	dbg;
extern int	ct;
extern int	lp[];
extern int	used[];		/* available registers */
extern int	ps;		/* dflt init pt size */
extern int	deltaps;	/* default change in ps */
extern int	dps_set;	/* 1 => -p option used */
extern int	gsize;		/* global size */
extern int	ft;		/* default font */
extern int	display;	/* 1 => inline, 0 => .EQ/.EN */
extern int	synerr;		/* 1 if syntax error in this eqn */

extern char	*typesetter;	/* typesetter name for -T... */
extern int	minsize;	/* min size it can print */
extern int	ttype;		/* actual type of typesetter: */

#define	DEVCAT	1
#define	DEV202	2
#define	DEVAPS	3
#define	DEVPOST	4

extern double	eht[];
extern double	ebase[];
extern int	lfont[];
extern int	rfont[];
extern int	lclass[];
extern int	rclass[];
extern YYSTYPE	yyval;   /* DEC cc wants long */
extern YYSTYPE  yylval;   /* DEC cc wants long */
extern int	eqnreg;
extern double	eqnht;
extern int	lefteq, righteq;
extern int	markline;	/* 1 if this EQ/EN contains mark or lineup */

#define	TBLSIZE	100

typedef struct s_tbl {
	char	*name;		/* e.g., "max" or "sum" */
	char	*cval;		/* e.g., "\\f1max\\fP" */
	int	ival;		/*    or SUM */
	struct s_tbl *next;
} tbl;

extern	char	*spaceval;	/* use in place of normal \x (for pic) */

#define	String	01
#define	Macro	02
#define	File	04
#define	Char	010
#define	Free	040

typedef struct infile {
	FILE	*fin;
	char	*fname;
	int	lineno;
} Infile;

typedef struct {	/* input source */
	int	type;	/* Macro, String, File */
	char	*sp;	/* if String or Macro */
} Src;

extern	Src	src[], *srcp;	/* input source stack */

#define	MAXARGS	20
typedef struct {	/* argument stack */
	char	*argstk[MAXARGS];	/* pointers to args */
	char	*argval;	/* points to space containing args */
} Arg;

typedef struct {	/* font number and name */
	int	ft;
	char	name[10];
} Font;

extern	Font	ftstack[];
extern	Font	*ftp;

extern	int	szstack[];
extern	int	nszstack;

extern	Infile	infile[10];
extern	Infile	*curfile;

extern	tbl	*lookup(tbl **tblp, char *name);
extern	void	install(tbl **tblp, char *name, char *cval, int ival);
extern	tbl	*keytbl[], *deftbl[], *restbl[], *ftunetbl[];

extern	int	salloc(void);
extern	void	sfree(int);
extern	void	nrwid(int, int, int);
extern	char	*ABSPS(int);
extern	char	*DPS(int, int);
extern	int	EFFPS(int);
extern	double	EM(double, int);
extern	double	REL(double, int);
extern	char	*pad(int);
extern	void	getstr(char *, int);
extern	char	*strsave(char *);

extern	int	input(void);
extern	int	unput(int);
extern	void	pbstr(char *);
extern	void	error(int, char *);
extern	void	yyerror(char *);

extern	void	diacrit(int, int);
extern	void	eqnbox(int, int, int);
extern	void	setfont(char *);
extern	void	font(int, int);
extern	void	globfont(void);
extern	void	fatbox(int);
extern	void	fromto(int, int, int);
extern	void	funny(int);
extern	void	integral(int, int, int);
extern	void	setintegral(void);
extern	void	pushsrc(int, char *);
extern	void	popsrc(void);
extern	void	putout(int);
extern	void	text(int, char *);
extern void subsup(int, int, int);
extern void bshiftb(int, int, int);
extern void shift2(int, int, int);
extern void setsize(char *);
extern void size(int, int);
extern void globsize(void);
extern void sqrt(int);
extern void text(int, char *);
extern void boverb(int, int);
extern void lineup(int);
extern void mark(int);
extern void paren(int, int, int);
extern void move(int, int, int);
extern void pile(int);
extern int startcol(int);
extern void column(int, int);
extern void matrix(int);
