#include "e.h"

#define getline p9getline
#undef inline
#define inline _einline

#define	MAXLINE	3600	/* maximum input line */

char *version = "version Oct 24, 1991";

char	in[MAXLINE+1];	/* input buffer */
int	noeqn;
char	*cmdname;

int	yyparse(void);
void	settype(char *);
int	getdata(void);
int	getline(char *);
void	inline(void);
void	init(void);
void	init_tbl(void);

int
main(int argc, char *argv[])
{
	char *p, buf[20];

	cmdname = argv[0];
	if ((p = getenv("TYPESETTER")))
		typesetter = p;
	while (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {

		case 'd':
			if (argv[1][2] == '\0') {
				dbg++;
				printf("...\teqn %s\n", version);
			} else {
				lefteq = argv[1][2];
				righteq = argv[1][3];
			}
			break;
		case 's': szstack[0] = gsize = atoi(&argv[1][2]); break;
		case 'p': deltaps = atoi(&argv[1][2]); dps_set = 1; break;
		case 'm': minsize = atoi(&argv[1][2]); break;
		case 'f': strcpy(ftstack[0].name,&argv[1][2]); break;
		case 'e': noeqn++; break;
		case 'T': typesetter = &argv[1][2]; break;
		default:
			fprintf(stderr, "%s: unknown option %s\n", cmdname, argv[1]);
			break;
		}
		argc--;
		argv++;
	}
	settype(typesetter);
	sprintf(buf, "\"%s\"", typesetter);
	install(deftbl, strsave(typesetter), strsave(buf), 0);
	init_tbl();	/* install other keywords in tables */
	curfile = infile;
	pushsrc(File, curfile->fname);
	if (argc <= 1) {
		curfile->fin = stdin;
		curfile->fname = strsave("-");
		getdata();
	} else
		while (argc-- > 1) {
			if (strcmp(*++argv, "-") == 0)
				curfile->fin = stdin;
			else if ((curfile->fin = fopen(*argv, "r")) == NULL)
				ERROR "can't open file %s", *argv FATAL;
			curfile->fname = strsave(*argv);
			getdata();
			if (curfile->fin != stdin)
				fclose(curfile->fin);
		}
	return 0;
}

void settype(char *s)	/* initialize data for particular typesetter */
			/* the minsize could profitably come from the */
{			/* troff description file /usr/lib/font/dev.../DESC.out */
	if (strcmp(s, "202") == 0)
		{ minsize = 5; ttype = DEV202; }
	else if (strcmp(s, "aps") == 0)
		{ minsize = 5; ttype = DEVAPS; }
	else if (strcmp(s, "cat") == 0)
		{ minsize = 6; ttype = DEVCAT; }
	else if (strcmp(s, "post") == 0)
		{ minsize = 4; ttype = DEVPOST; }
	else
		{ minsize = 5; ttype = DEV202; }
}

int
getdata(void)
{
	int i, type, ln;
	char fname[100];

	errno = 0;
	curfile->lineno = 0;
	printf(".lf 1 %s\n", curfile->fname);
	while ((type = getline(in)) != EOF) {
		if (in[0] == '.' && in[1] == 'E' && in[2] == 'Q') {
			for (i = 11; i < 100; i++)
				used[i] = 0;
			printf("%s", in);
			if (markline) {	/* turn off from last time */
				printf(".nr MK 0\n");
				markline = 0;
			}
			display = 1;
			init();
			yyparse();
			if (eqnreg > 0) {
				if (markline)
					printf(".nr MK %d\n", markline); /* for -ms macros */
				printf(".if %gm>\\n(.v .ne %gm\n", eqnht, eqnht);
				printf(".rn %d 10\n", eqnreg);
				if (!noeqn)
					printf("\\&\\*(10\n");
			}
			printf(".EN");
			while (putchar(input()) != '\n')
				;
			printf(".lf %d\n", curfile->lineno+1);
		}
		else if (type == lefteq)
			inline();
		else if (in[0] == '.' && in[1] == 'l' && in[2] == 'f') {
			if (sscanf(in+3, "%d %s", &ln, fname) == 2) {
				free(curfile->fname);
				printf(".lf %d %s\n", curfile->lineno = ln, curfile->fname = strsave(fname));
			} else
				printf(".lf %d\n", curfile->lineno = ln);
		} else
			printf("%s", in);
	}
	return(0);
}

int
getline(char *s)
{
	register int c;

	while ((c=input()) != '\n' && c != EOF && c != lefteq) {
		if (s >= in+MAXLINE) {
			ERROR "input line too long: %.20s\n", in WARNING;
			in[MAXLINE] = '\0';
			break;
		}
		*s++ = c;
	}
	if (c != lefteq)
		*s++ = c;
	*s = '\0';
	return(c);
}

void inline(void)
{
	int ds, n, sz1 = 0;

	n = curfile->lineno;
	if (szstack[0] != 0)
		printf(".nr %d \\n(.s\n", sz1 = salloc());
	ds = salloc();
	printf(".rm %d \n", ds);
	display = 0;
	do {
		if (*in)
			printf(".as %d \"%s\n", ds, in);
		init();
		yyparse();
		if (eqnreg > 0) {
			printf(".as %d \\*(%d\n", ds, eqnreg);
			sfree(eqnreg);
			printf(".lf %d\n", curfile->lineno+1);
		}
	} while (getline(in) == lefteq);
	if (*in)
		printf(".as %d \"%s", ds, in);
	if (sz1)
		printf("\\s\\n(%d", sz1);
	printf("\\*(%d\n", ds);
	printf(".lf %d\n", curfile->lineno+1);
	if (curfile->lineno > n+3)
		fprintf(stderr, "eqn warning: multi-line %c...%c, file %s:%d,%d\n",
			lefteq, righteq, curfile->fname, n, curfile->lineno);
	sfree(ds);
	if (sz1) sfree(sz1);
}

void putout(int p1)
{
	double before, after;
	extern double BeforeSub, AfterSub;

	dprintf(".\tanswer <- S%d, h=%g,b=%g\n",p1, eht[p1], ebase[p1]);
	eqnht = eht[p1];
	before = eht[p1] - ebase[p1] - BeforeSub;	/* leave room for sub or superscript */
	after = ebase[p1] - AfterSub;
	if (spaceval || before > 0.01 || after > 0.01) {
		printf(".ds %d ", p1);	/* used to be \\x'0' here:  why? */
		if (spaceval != NULL)
			printf("\\x'0-%s'", spaceval);
		else if (before > 0.01)
			printf("\\x'0-%gm'", before);
		printf("\\*(%d", p1);
		if (spaceval == NULL && after > 0.01)
			printf("\\x'%gm'", after);
		putchar('\n');
	}
	if (szstack[0] != 0)
		printf(".ds %d %s\\*(%d\\s\\n(99\n", p1, DPS(gsize,gsize), p1);
	eqnreg = p1;
	if (spaceval != NULL) {
		free(spaceval);
		spaceval = NULL;
	}
}

void init(void)
{
	synerr = 0;
	ct = 0;
	ps = gsize;
	ftp = ftstack;
	ft = ftp->ft;
	nszstack = 0;
	if (szstack[0] != 0)	/* absolute gsize in effect */
		printf(".nr 99 \\n(.s\n");
}

int
salloc(void)
{
	int i;

	for (i = 11; i < 100; i++)
		if (used[i] == 0) {
			used[i]++;
			return(i);
		}
	ERROR "no eqn strings left (%d)", i FATAL;
	return(0);
}

void sfree(int n)
{
	used[n] = 0;
}

void nrwid(int n1, int p, int n2)
{
	printf(".nr %d 0\\w'%s\\*(%d'\n", n1, DPS(gsize,p), n2);	/* 0 defends against - width */
}

char *ABSPS(int dn)	/* absolute size dn in printable form \sd or \s(dd (dd >= 40) */
{
	static char buf[100], *lb = buf;
	char *p;

	if (lb > buf + sizeof(buf) - 10)
		lb = buf;
	p = lb;
	*lb++ = '\\';
	*lb++ = 's';
	if (dn >= 10) {		/* \s(dd only works in new troff */
		if (dn >= 40)
			*lb++ = '(';
		*lb++ = dn/10 + '0';
		*lb++ = dn%10 + '0';
	} else {
		*lb++ = dn + '0';
	}
	*lb++ = '\0';
	return p;
}

char *DPS(int f, int t)	/* delta ps (t-f) in printable form \s+d or \s-d or \s+-(dd */
{
	static char buf[100], *lb = buf;
	char *p;
	int dn;

	if (lb > buf + sizeof(buf) - 10)
		lb = buf;
	p = lb;
	*lb++ = '\\';
	*lb++ = 's';
	dn = EFFPS(t) - EFFPS(f);
	if (szstack[nszstack] != 0)	/* absolute */
		dn = EFFPS(t);		/* should do proper \s(dd */
	else if (dn >= 0)
		*lb++ = '+';
	else {
		*lb++ = '-';
		dn = -dn;
	}
	if (dn >= 10) {		/* \s+(dd only works in new troff */
		*lb++ = '(';
		*lb++ = dn/10 + '0';
		*lb++ = dn%10 + '0';
	} else {
		*lb++ = dn + '0';
	}
	*lb++ = '\0';
	return p;
}

int
EFFPS(int n)	/* effective value of n */
{
	if (n >= minsize)
		return n;
	else
		return minsize;
}

double EM(double m, int ps)	/* convert m to ems in gsize */
{
	m *= (double) EFFPS(ps) / gsize;
	if (m <= 0.001 && m >= -0.001)
		return 0;
	else
		return m;
}

double REL(double m, int ps)	/* convert m to ems in ps */
{
	m *= (double) gsize / EFFPS(ps);
	if (m <= 0.001 && m >= -0.001)
		return 0;
	else
		return m;
}
