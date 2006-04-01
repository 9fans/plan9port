%{
#include <u.h>
#include <libc.h>
#include <bio.h>

enum
{
	Ndim	= 15,		/* number of dimensions */
	Nsym	= 40,		/* size of a name */
	Nvar	= 203,		/* hash table size */
	Maxe	= 695		/* log of largest number */
};

typedef	struct	Var	Var;
typedef	struct	Node	Node;
typedef	struct	Prefix	Prefix;

struct	Node
{
	double	val;
	schar	dim[Ndim];
};
struct	Var
{
	Rune	name[Nsym];
	Node	node;
	Var*	link;
};
struct	Prefix
{
	double	val;
	char*	name;
	Rune*	pname;
};

char	buf[100];
int	digval;
Biobuf*	fi;
Biobuf	linebuf;
Var*	fund[Ndim];
Rune	line[1000];
ulong	lineno;
int	linep;
int	nerrors;
Node	one;
int	peekrune;
Node	retnode1;
Node	retnode2;
Node	retnode;
Rune	sym[Nsym];
Var*	vars[Nvar];
int	vflag;

#define div unitsdiv

extern	void	add(Node*, Node*, Node*);
extern	void	div(Node*, Node*, Node*);
extern	int	specialcase(Node*, Node*, Node*);
extern	double	fadd(double, double);
extern	double	fdiv(double, double);
extern	double	fmul(double, double);
extern	int	gdigit(void*);
extern	Var*	lookup(int);
extern	void	main(int, char*[]);
extern	void	mul(Node*, Node*, Node*);
extern	void	ofile(void);
extern	double	pname(void);
extern	void	printdim(char*, int, int);
extern	int	ralpha(int);
extern	int	readline(void);
extern	void	sub(Node*, Node*, Node*);
extern	int	Ufmt(Fmt*);
extern	void	xpn(Node*, Node*, int);
extern	void	yyerror(char*, ...);
extern	int	yylex(void);
extern	int	yyparse(void);

typedef	Node*	indnode;
/* #pragma	varargck	type	"U"	indnode */

%}
%union
{
	Node	node;
	Var*	var;
	int	numb;
	double	val;
}

%type	<node>	prog expr expr0 expr1 expr2 expr3 expr4

%token	<val>	VAL
%token	<var>	VAR
%token	<numb>	SUP
%%
prog:
	':' VAR expr
	{
		int f;

		f = $2->node.dim[0];
		$2->node = $3;
		$2->node.dim[0] = 1;
		if(f)
			yyerror("redefinition of %S", $2->name);
		else
		if(vflag)
			print("%S\t%U\n", $2->name, &$2->node);
	}
|	':' VAR '#'
	{
		int f, i;

		for(i=1; i<Ndim; i++)
			if(fund[i] == 0)
				break;
		if(i >= Ndim) {
			yyerror("too many dimensions");
			i = Ndim-1;
		}
		fund[i] = $2;

		f = $2->node.dim[0];
		$2->node = one;
		$2->node.dim[0] = 1;
		$2->node.dim[i] = 1;
		if(f)
			yyerror("redefinition of %S", $2->name);
		else
		if(vflag)
			print("%S\t#\n", $2->name);
	}
|	'?' expr
	{
		retnode1 = $2;
	}
|	'?'
	{
		retnode1 = one;
	}

expr:
	expr4
|	expr '+' expr4
	{
		add(&$$, &$1, &$3);
	}
|	expr '-' expr4
	{
		sub(&$$, &$1, &$3);
	}

expr4:
	expr3
|	expr4 '*' expr3
	{
		mul(&$$, &$1, &$3);
	}
|	expr4 '/' expr3
	{
		div(&$$, &$1, &$3);
	}

expr3:
	expr2
|	expr3 expr2
	{
		mul(&$$, &$1, &$2);
	}

expr2:
	expr1
|	expr2 SUP
	{
		xpn(&$$, &$1, $2);
	}
|	expr2 '^' expr1
	{
		int i;

		for(i=1; i<Ndim; i++)
			if($3.dim[i]) {
				yyerror("exponent has units");
				$$ = $1;
				break;
			}
		if(i >= Ndim) {
			i = $3.val;
			if(i != $3.val)
				yyerror("exponent not integral");
			xpn(&$$, &$1, i);
		}
	}

expr1:
	expr0
|	expr1 '|' expr0
	{
		div(&$$, &$1, &$3);
	}

expr0:
	VAR
	{
		if($1->node.dim[0] == 0) {
			yyerror("undefined %S", $1->name);
			$$ = one;
		} else
			$$ = $1->node;
	}
|	VAL
	{
		$$ = one;
		$$.val = $1;
	}
|	'(' expr ')'
	{
		$$ = $2;
	}
%%

int
yylex(void)
{
	int c, i;

	c = peekrune;
	peekrune = ' ';

loop:
	if((c >= '0' && c <= '9') || c == '.')
		goto numb;
	if(ralpha(c))
		goto alpha;
	switch(c) {
	case ' ':
	case '\t':
		c = line[linep++];
		goto loop;
	case 0xd7:
		return 0x2a;
	case 0xf7:
		return 0x2f;
	case 0xb9:
	case 0x2071:
		yylval.numb = 1;
		return SUP;
	case 0xb2:
	case 0x2072:
		yylval.numb = 2;
		return SUP;
	case 0xb3:
	case 0x2073:
		yylval.numb = 3;
		return SUP;
	}
	return c;

alpha:
	memset(sym, 0, sizeof(sym));
	for(i=0;; i++) {
		if(i < nelem(sym))
			sym[i] = c;
		c = line[linep++];
		if(!ralpha(c))
			break;
	}
	sym[nelem(sym)-1] = 0;
	peekrune = c;
	yylval.var = lookup(0);
	return VAR;

numb:
	digval = c;
	yylval.val = fmtcharstod(gdigit, 0);
	return VAL;
}

void
main(int argc, char *argv[])
{
	char *file;

	ARGBEGIN {
	default:
		print("usage: units [-v] [file]\n");
		exits("usage");
	case 'v':
		vflag = 1;
		break;
	} ARGEND

	file = unsharp("#9/lib/units");
	if(argc > 0)
		file = argv[0];
	fi = Bopen(file, OREAD);
	if(fi == 0) {
		print("cant open: %s\n", file);
		exits("open");
	}
	fmtinstall('U', Ufmt);
	one.val = 1;

	/*
	 * read the 'units' file to
	 * develope a database
	 */
	lineno = 0;
	for(;;) {
		lineno++;
		if(readline())
			break;
		if(line[0] == 0 || line[0] == '/')
			continue;
		peekrune = ':';
		yyparse();
	}

	/*
	 * read the console to
	 * print ratio of pairs
	 */
	Bterm(fi);
	fi = &linebuf;
	Binit(fi, 0, OREAD);
	lineno = 0;
	for(;;) {
		if(lineno & 1)
			print("you want: ");
		else
			print("you have: ");
		if(readline())
			break;
		peekrune = '?';
		nerrors = 0;
		yyparse();
		if(nerrors)
			continue;
		if(lineno & 1) {
			if(specialcase(&retnode, &retnode2, &retnode1))
				print("\tis %U\n", &retnode);
			else {
				div(&retnode, &retnode2, &retnode1);
				print("\t* %U\n", &retnode);
				div(&retnode, &retnode1, &retnode2);
				print("\t/ %U\n", &retnode);
			}
		} else
			retnode2 = retnode1;
		lineno++;
	}
	print("\n");
	exits(0);
}

/*
 * all characters that have some
 * meaning. rest are usable as names
 */
int
ralpha(int c)
{
	switch(c) {
	case 0:
	case '+':
	case '-':
	case '*':
	case '/':
	case '[':
	case ']':
	case '(':
	case ')':
	case '^':
	case ':':
	case '?':
	case ' ':
	case '\t':
	case '.':
	case '|':
	case '#':
	case 0xb9:
	case 0x2071:
	case 0xb2:
	case 0x2072:
	case 0xb3:
	case 0x2073:
	case 0xd7:
	case 0xf7:
		return 0;
	}
	return 1;
}

int
gdigit(void *v)
{
	int c;

	USED(v);
	c = digval;
	if(c) {
		digval = 0;
		return c;
	}
	c = line[linep++];
	peekrune = c;
	return c;
}

void
yyerror(char *fmt, ...)
{
	va_list arg;

	/*
	 * hack to intercept message from yaccpar
	 */
	if(strcmp(fmt, "syntax error") == 0) {
		yyerror("syntax error, last name: %S", sym);
		return;
	}
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	print("%ld: %S\n\t%s\n", lineno, line, buf);
	nerrors++;
	if(nerrors > 5) {
		print("too many errors\n");
		exits("errors");
	}
}

void
add(Node *c, Node *a, Node *b)
{
	int i, d;

	for(i=0; i<Ndim; i++) {
		d = a->dim[i];
		c->dim[i] = d;
		if(d != b->dim[i])
			yyerror("add must be like units");
	}
	c->val = fadd(a->val, b->val);
}

void
sub(Node *c, Node *a, Node *b)
{
	int i, d;

	for(i=0; i<Ndim; i++) {
		d = a->dim[i];
		c->dim[i] = d;
		if(d != b->dim[i])
			yyerror("sub must be like units");
	}
	c->val = fadd(a->val, -b->val);
}

void
mul(Node *c, Node *a, Node *b)
{
	int i;

	for(i=0; i<Ndim; i++)
		c->dim[i] = a->dim[i] + b->dim[i];
	c->val = fmul(a->val, b->val);
}

void
div(Node *c, Node *a, Node *b)
{
	int i;

	for(i=0; i<Ndim; i++)
		c->dim[i] = a->dim[i] - b->dim[i];
	c->val = fdiv(a->val, b->val);
}

void
xpn(Node *c, Node *a, int b)
{
	int i;

	*c = one;
	if(b < 0) {
		b = -b;
		for(i=0; i<b; i++)
			div(c, c, a);
	} else
	for(i=0; i<b; i++)
		mul(c, c, a);
}

int
specialcase(Node *c, Node *a, Node *b)
{
	int i, d, d1, d2;

	d1 = 0;
	d2 = 0;
	for(i=1; i<Ndim; i++) {
		d = a->dim[i];
		if(d) {
			if(d != 1 || d1)
				return 0;
			d1 = i;
		}
		d = b->dim[i];
		if(d) {
			if(d != 1 || d2)
				return 0;
			d2 = i;
		}
	}
	if(d1 == 0 || d2 == 0)
		return 0;

	if(memcmp(fund[d1]->name, L"°C", 3*sizeof(Rune)) == 0 &&
	   memcmp(fund[d2]->name, L"°F", 3*sizeof(Rune)) == 0 &&
	   b->val == 1) {
		memcpy(c->dim, b->dim, sizeof(c->dim));
		c->val = a->val * 9. / 5. + 32.;
		return 1;
	}

	if(memcmp(fund[d1]->name, L"°F", 3*sizeof(Rune)) == 0 &&
	   memcmp(fund[d2]->name, L"°C", 3*sizeof(Rune)) == 0 &&
	   b->val == 1) {
		memcpy(c->dim, b->dim, sizeof(c->dim));
		c->val = (a->val - 32.) * 5. / 9.;
		return 1;
	}
	return 0;
}

void
printdim(char *str, int d, int n)
{
	Var *v;

	if(n) {
		v = fund[d];
		if(v)
			sprint(strchr(str, 0), " %S", v->name);
		else
			sprint(strchr(str, 0), " [%d]", d);
		switch(n) {
		case 1:
			break;
		case 2:
			strcat(str, "²");
			break;
		case 3:
			strcat(str, "³");
			break;
		default:
			sprint(strchr(str, 0), "^%d", n);
		}
	}
}

int
Ufmt(Fmt *fp)
{
	char str[200];
	Node *n;
	int f, i, d;

	n = va_arg(fp->args, Node*);
	sprint(str, "%g", n->val);

	f = 0;
	for(i=1; i<Ndim; i++) {
		d = n->dim[i];
		if(d > 0)
			printdim(str, i, d);
		else
		if(d < 0)
			f = 1;
	}

	if(f) {
		strcat(str, " /");
		for(i=1; i<Ndim; i++) {
			d = n->dim[i];
			if(d < 0)
				printdim(str, i, -d);
		}
	}

	return fmtstrcpy(fp, str);
}

int
readline(void)
{
	int i, c;

	linep = 0;
	for(i=0;; i++) {
		c = Bgetrune(fi);
		if(c < 0)
			return 1;
		if(c == '\n')
			break;
		if(i < nelem(line))
			line[i] = c;
	}
	if(i >= nelem(line))
		i = nelem(line)-1;
	line[i] = 0;
	return 0;
}

Var*
lookup(int f)
{
	int i;
	Var *v, *w;
	double p;
	ulong h;

	h = 0;
	for(i=0; sym[i]; i++)
		h = h*13 + sym[i];
	h %= nelem(vars);

	for(v=vars[h]; v; v=v->link)
		if(memcmp(sym, v->name, sizeof(sym)) == 0)
			return v;
	if(f)
		return 0;
	v = malloc(sizeof(*v));
	if(v == nil) {
		fprint(2, "out of memory\n");
		exits("mem");
	}
	memset(v, 0, sizeof(*v));
	memcpy(v->name, sym, sizeof(sym));
	v->link = vars[h];
	vars[h] = v;

	p = 1;
	for(;;) {
		p = fmul(p, pname());
		if(p == 0)
			break;
		w = lookup(1);
		if(w) {
			v->node = w->node;
			v->node.val = fmul(v->node.val, p);
			break;
		}
	}
	return v;
}

Prefix	prefix[] =
{
	1e-24,	"yocto", 0,
	1e-21,	"zepto", 0,
	1e-18,	"atto", 0,
	1e-15,	"femto", 0,
	1e-12,	"pico", 0,
	1e-9,	"nano", 0,
	1e-6,	"micro", 0,
	1e-6,	"μ", 0,
	1e-3,	"milli", 0,
	1e-2,	"centi", 0,
	1e-1,	"deci", 0,
	1e1,	"deka", 0,
	1e2,	"hecta", 0,
	1e2,	"hecto", 0,
	1e3,	"kilo", 0,
	1e6,	"mega", 0,
	1e6,	"meg", 0,
	1e9,	"giga", 0,
	1e12,	"tera", 0,
	1e15,	"peta", 0,
	1e18,	"exa", 0,
	1e21,	"zetta", 0,
	1e24,	"yotta", 0,
	0,	0, 0,
};

double
pname(void)
{
	Rune *p;
	int i, j, c;

	/*
	 * rip off normal prefixs
	 */
	if(prefix[0].pname == nil){
		for(i=0; prefix[i].name; i++)
			prefix[i].pname = runesmprint("%s", prefix[i].name);
	}

	for(i=0; p=prefix[i].pname; i++) {
		for(j=0; c=p[j]; j++)
			if(c != sym[j])
				goto no;
		memmove(sym, sym+j, (Nsym-j)*sizeof(*sym));
		memset(sym+(Nsym-j), 0, j*sizeof(*sym));
		return prefix[i].val;
	no:;
	}

	/*
	 * rip off 's' suffixes
	 */
	for(j=0; sym[j]; j++)
		;
	j--;
	/* j>1 is special hack to disallow ms finding m */
	if(j > 1 && sym[j] == 's') {
		sym[j] = 0;
		return 1;
	}
	return 0;
}

/*
 * careful floating point
 */
double
fmul(double a, double b)
{
	double l;

	if(a <= 0) {
		if(a == 0)
			return 0;
		l = log(-a);
	} else
		l = log(a);

	if(b <= 0) {
		if(b == 0)
			return 0;
		l += log(-b);
	} else
		l += log(b);

	if(l > Maxe) {
		yyerror("overflow in multiply");
		return 1;
	}
	if(l < -Maxe) {
		yyerror("underflow in multiply");
		return 0;
	}
	return a*b;
}

double
fdiv(double a, double b)
{
	double l;

	if(a <= 0) {
		if(a == 0)
			return 0;
		l = log(-a);
	} else
		l = log(a);

	if(b <= 0) {
		if(b == 0) {
			yyerror("division by zero");
			return 1;
		}
		l -= log(-b);
	} else
		l -= log(b);

	if(l > Maxe) {
		yyerror("overflow in divide");
		return 1;
	}
	if(l < -Maxe) {
		yyerror("underflow in divide");
		return 0;
	}
	return a/b;
}

double
fadd(double a, double b)
{
	return a + b;
}
