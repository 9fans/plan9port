#include "e.h"

/*

This file contains parameter values for many of the
tuning parameters in eqn.  Names are defined words.

Strings are plugged in verbatim.
Floats are usually in ems.

*/

/* In main.c: */

double	BeforeSub = 1.2;	/* line space before a subscript */
double	AfterSub  = 0.2;	/* line space after a subscript */

/* diacrit.c: */

double	Dvshift	= 0.25;		/* vertical shift for diacriticals on tall letters */
double	Dhshift = 0.025;	/* horizontal shift for tall letters */
double	Dh2shift = 0.05;	/* horizontal shift for small letters */
double	Dheight	= 0.25;		/* increment to height for diacriticals */
double	Barv	= 0.68;		/* vertical shift for bar */
double	Barh	= 0.05;		/* 1/2 horizontal shrink for bar */
double	Ubarv	= 0.1;		/* shift underbar up this much ems */
double	Ubarh	= 0.05;		/* 1/2 horizontal shrink for underbar */

/* Also:
	Vec, Dyad, Hat, Tilde, Dot, Dotdot, Utilde */

/* eqnbox.c: */

char	*IRspace = "\\^";	/* space between italic & roman boxes */

/* fat.c: */

double	Fatshift = 0.05;	/* fattening shifts by Fatshift ems */

/* funny.c: */

int	Funnyps	= 5;		/* point size change (== 5 above) */
double	Funnyht = 0.2;		/* height correction */
double	Funnybase = 0.3;	/* base correction */

/* integral.c: */

int	Intps	= 4;		/* point size change for integral (== 4 above) */
double	Intht	= 1.15;		/* ht of integral in ems */
double	Intbase	= 0.3;		/* base in ems */
double	Int1h	= 0.4;		/* lower limit left */
double	Int1v	= 0.2;		/* lower limit down */
double	Int2h	= 0.05;		/* upper limit right was 8 */
double	Int2v	= 0.1;		/* upper limit up */

/* matrix.c: */

char	*Matspace = "\\ \\ ";	/* space between matrix columns */

/* over.c: */

double	Overgap	= 0.3;		/* gap between num and denom */
double	Overwid	= 0.5;		/* extra width of box */
double	Overline = 0.1;		/* extra length of fraction bar */

/* paren.c* */

double	Parenbase = 0.4;	/* shift of base for even count */
double	Parenshift = 0.13;	/* how much to shift parens down in left ... */
				/* ignored unless postscript */
double	Parenheight = 0.3;	/* extra height above builtups */

/* pile.c: */

double	Pilegap	= 0.4;		/* gap between pile elems */
double	Pilebase = 0.5;		/* shift base of even # of piled elems */

/* shift.c: */

double	Subbase	= 0.2;		/* subscript base belowe main base */
double	Supshift = 0.4;		/* superscript .4 up main box */
char	*Sub1space = "\\|";	/* italic sub roman space */
char	*Sup1space = "\\|";	/* italic sup roman space */
char	*Sub2space = "\\^";	/* space after subscripted thing */
char	*SS1space = "\\^";	/* space before sub in x sub i sup j */
char	*SS2space = "\\^";	/* space before sup */

/* sqrt.c: */
	/* sqrt is hard!  punt for now. */
	/* part of the problem is that every typesetter does it differently */
	/* and we have several typesetters to run. */

/* text.c: */
	/* ought to be done by a table */

struct tune {
	char	*name;
	char	*cval;
} tune[]	={
  /* diacrit.c */
	"vec_def",	"\\f1\\v'-.5m'\\s-3\\(->\\s0\\v'.5m'\\fP",	/* was \s-2 & .45m */
	"dyad_def",	"\\f1\\v'-.5m'\\s-3\\z\\(<-\\|\\(->\\s0\\v'.5m'\\fP",
	"hat_def",	"\\f1\\v'-.05m'\\s+1^\\s0\\v'.05m'\\fP",	/* was .1 */
	"tilde_def",	"\\f1\\v'-.05m'\\s+1~\\s0\\v'.05m'\\fP",
	"dot_def",	"\\f1\\v'-.67m'.\\v'.67m'\\fP",
	"dotdot_def",	"\\f1\\v'-.67m'..\\v'.67m'\\fP",
	"utilde_def",	"\\f1\\v'1.0m'\\s+2~\\s-2\\v'-1.0m'\\fP",
  /* funny.c */
	"sum_def",	"\\|\\v'.3m'\\s+5\\(*S\\s-5\\v'-.3m'\\|",
	"union_def",	"\\|\\v'.3m'\\s+5\\(cu\\s-5\\v'-.3m'\\|",
	"inter_def",	"\\|\\v'.3m'\\s+5\\(ca\\s-5\\v'-.3m'\\|",
	"prod_def",	"\\|\\v'.3m'\\s+5\\(*P\\s-5\\v'-.3m'\\|",
  /* integral.c */
	"int_def",	"\\v'.1m'\\s+4\\(is\\s-4\\v'-.1m'",
	0, 0
};

tbl	*ftunetbl[TBLSIZE];	/* user-defined names */

char *ftunes[] ={	/* this table intentionally left small */
	"Subbase",
	"Supshift",
	0
};

void init_tune(void)
{
	int i;

	for (i = 0; tune[i].name != NULL; i++)
		install(deftbl, tune[i].name, tune[i].cval, 0);
	for (i = 0; ftunes[i] != NULL; i++)
		install(ftunetbl, ftunes[i], (char *) 0, 0);
}

#define eq(s, t) (strcmp(s,t) == 0)

void ftune(char *s, char *t)	/* brute force for now */
{
	double f = atof(t);
	double *target;

	while (*t == ' ' || *t == '\t')
		t++;
	if (eq(s, "Subbase"))
		target = &Subbase;
	else if (eq(s, "Supshift"))
		target = &Supshift;
	if (t[0] == '+' || t[0] == '-')
		*target += f;
	else
		*target = f;
}
