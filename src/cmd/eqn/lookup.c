#include "e.h"
#include "y.tab.h"

tbl	*keytbl[TBLSIZE];	/* key words */
tbl	*restbl[TBLSIZE];	/* reserved words */
tbl	*deftbl[TBLSIZE];	/* user-defined names */

struct keyword {
	char	*key;
	int	keyval;
} keyword[]	={
	"sub", 		SUB, 
	"sup", 		SUP, 
	".EN", 		DOTEN,
	".EQ",		DOTEQ, 
	"from", 	FROM, 
	"to", 		TO, 
	"sum", 		SUM, 
	"hat", 		HAT, 
	"vec", 		VEC, 
	"dyad", 	DYAD, 
	"dot", 		DOT, 
	"dotdot", 	DOTDOT, 
	"bar", 		BAR,
	"lowbar",	LOWBAR,
	"highbar",	HIGHBAR, 
	"tilde", 	TILDE, 
	"utilde", 	UTILDE, 
	"under", 	UNDER, 
	"prod", 	PROD, 
	"int", 		INT, 
	"integral", 	INT, 
	"union", 	UNION, 
	"inter", 	INTER, 
	"matrix", 	MATRIX, 
	"col", 		COL, 
	"lcol", 	LCOL, 
	"ccol", 	CCOL, 
	"rcol", 	RCOL, 
	"pile", 	COL,	/* synonyms ... */ 
	"lpile", 	LCOL, 
	"cpile", 	CCOL, 
	"rpile", 	RCOL, 
	"over", 	OVER, 
	"sqrt", 	SQRT, 
	"above", 	ABOVE, 
	"size", 	SIZE, 
	"font", 	FONT, 
	"fat", 		FAT, 
	"roman", 	ROMAN, 
	"italic", 	ITALIC, 
	"bold", 	BOLD, 
	"left", 	LEFT, 
	"right", 	RIGHT, 
	"delim", 	DELIM, 
	"define", 	DEFINE, 
	"tdefine", 	DEFINE, 
	"ndefine", 	NDEFINE, 
	"ifdef",	IFDEF,
	"gsize", 	GSIZE, 
	".gsize", 	GSIZE, 
	"gfont", 	GFONT, 
	"include", 	INCLUDE, 
	"copy", 	INCLUDE, 
	"space",	SPACE,
	"up", 		UP, 
	"down", 	DOWN, 
	"fwd", 		FWD, 
	"back", 	BACK, 
	"mark", 	MARK, 
	"lineup", 	LINEUP, 
	0, 	0
};

struct resword {
	char	*res;
	char	*resval;
} resword[]	={
	">=",		"\\(>=",
	"<=",		"\\(<=",
	"==",		"\\(==",
	"!=",		"\\(!=",
	"+-",		"\\(+-",
	"->",		"\\(->",
	"<-",		"\\(<-",
	"inf",		"\\(if",
	"infinity",	"\\(if",
	"partial",	"\\(pd",
	"half",		"\\f1\\(12\\fP",
	"prime",	"\\f1\\v'.5m'\\s+3\\(fm\\s-3\\v'-.5m'\\fP",
	"dollar",	"\\f1$\\fP",
	"nothing",	"",
	"times",	"\\(mu",
	"del",		"\\(gr",
	"grad",		"\\(gr",
	"approx",	"\\v'-.2m'\\z\\(ap\\v'.25m'\\(ap\\v'-.05m'",
	"cdot",		"\\v'-.3m'.\\v'.3m'",
	"...",		"\\v'-.25m'\\ .\\ .\\ .\\ \\v'.25m'",
	",...,",	"\\f1,\\fP\\ .\\ .\\ .\\ \\f1,\\fP\\|",
	"alpha",	"α",
	"ALPHA",	"Α",
	"beta",		"β",
	"BETA",		"Β",
	"gamma",	"γ",
	"GAMMA",	"Γ",
	"delta",	"δ",
	"DELTA",	"Δ",
	"epsilon",	"ε",
	"EPSILON",	"Ε",
	"omega",	"ω",
	"OMEGA",	"Ω",
	"lambda",	"λ",
	"LAMBDA",	"Λ",
	"mu",		"μ",
	"MU",		"Μ",
	"nu",		"ν",
	"NU",		"Ν",
	"theta",	"θ",
	"THETA",	"Θ",
	"phi",		"φ",
	"PHI",		"Φ",
	"pi",		"π",
	"PI",		"Π",
	"sigma",	"σ",
	"SIGMA",	"Σ",
	"xi",		"ξ",
	"XI",		"Ξ",
	"zeta",		"ζ",
	"ZETA",		"Ζ",
	"iota",		"ι",
	"IOTA",		"Ι",
	"eta",		"η",
	"ETA",		"Η",
	"kappa",	"κ",
	"KAPPA",	"Κ",
	"rho",		"ρ",
	"RHO",		"Ρ",
	"tau",		"τ",
	"TAU",		"Τ",
	"omicron",	"ο",
	"OMICRON",	"Ο",
	"upsilon",	"υ",
	"UPSILON",	"Υ",
	"psi",		"ψ",
	"PSI",		"Ψ",
	"chi",		"χ",
	"CHI",		"Χ",
	"and",		"\\f1and\\fP",
	"for",		"\\f1for\\fP",
	"if",		"\\f1if\\fP",
	"Re",		"\\f1Re\\fP",
	"Im",		"\\f1Im\\fP",
	"sin",		"\\f1sin\\fP",
	"cos",		"\\f1cos\\fP",
	"tan",		"\\f1tan\\fP",
	"arc",		"\\f1arc\\fP",
	"sinh",		"\\f1sinh\\fP",
	"coth",		"\\f1coth\\fP",
	"tanh",		"\\f1tanh\\fP",
	"cosh",		"\\f1cosh\\fP",
	"lim",		"\\f1lim\\fP",
	"log",		"\\f1log\\fP",
	"ln",		"\\f1ln\\fP",
	"max",		"\\f1max\\fP",
	"min",		"\\f1min\\fP",
	"exp",		"\\f1exp\\fP",
	"det",		"\\f1det\\fP",
	0,	0
};

int hash(char *s)
{
	register unsigned int h;

	for (h = 0; *s != '\0'; )
		h += *s++;
	h %= TBLSIZE;
	return h;
}

tbl *lookup(tbl **tblp, char *name)	/* find name in tbl */
{
	register tbl *p;

	for (p = tblp[hash(name)]; p != NULL; p = p->next)
		if (strcmp(name, p->name) == 0)
			return(p);
	return(NULL);
}

void install(tbl **tblp, char *name, char *cval, int ival)	/* install name, vals in tblp */
{
	register tbl *p;
	int h;

	if ((p = lookup(tblp, name)) == NULL) {
		p = (tbl *) malloc(sizeof(tbl));
		if (p == NULL)
			ERROR "out of space in install" FATAL;
		h = hash(name);	/* bad visibility here */
		p->name = name;
		p->next = tblp[h];
		tblp[h] = p;
	}
	p->cval = cval;
	p->ival = ival;
}

void init_tbl(void)	/* initialize tables */
{
	int i;
	extern int init_tune(void);

	for (i = 0; keyword[i].key != NULL; i++)
		install(keytbl, keyword[i].key, (char *) 0, keyword[i].keyval);
	for (i = 0; resword[i].res != NULL; i++)
		install(restbl, resword[i].res, resword[i].resval, 0);
	init_tune();	/* tuning table done in tuning.c */
}
