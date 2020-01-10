#include "a.h"
/*
 * 8. Number Registers
 * (Reg register implementation is also here.)
 */

/*
 *	\nx		N
 *	\n(xx	N
 *	\n+x		N+=M
 *	\n-x		N-=M
 *
 *	.nr R ±N M
 *	.af R c
 *
 *	formats
 *		1	0, 1, 2, 3, ...
 *		001	001, 002, 003, ...
 *		i	0, i, ii, iii, iv, v, ...
 *		I	0, I, II, III, IV, V, ...
 *		a	0, a, b, ..., aa, ab, ..., zz, aaa, ...
 *		A	0, A, B, ..., AA, AB, ..., ZZ, AAA, ...
 *
 *	\gx \g(xx return format of number register
 *
 *	.rr R
 */

typedef struct Reg Reg;
struct Reg
{
	Reg *next;
	Rune *name;
	Rune *val;
	Rune *fmt;
	int inc;
};

Reg *dslist;
Reg *nrlist;

/*
 * Define strings and numbers.
 */
void
dsnr(Rune *name, Rune *val, Reg **l)
{
	Reg *s;

	for(s = *l; s != nil; s = *l){
		if(runestrcmp(s->name, name) == 0)
			break;
		l = &s->next;
	}
	if(val == nil){
		if(s){
			*l = s->next;
			free(s->val);
			free(s->fmt);
			free(s);
		}
		return;
	}
	if(s == nil){
		s = emalloc(sizeof(Reg));
		*l = s;
		s->name = erunestrdup(name);
	}else
		free(s->val);
	s->val = erunestrdup(val);
}

Rune*
getdsnr(Rune *name, Reg *list)
{
	Reg *s;

	for(s=list; s; s=s->next)
		if(runestrcmp(name, s->name) == 0)
			return s->val;
	return nil;
}

void
ds(Rune *name, Rune *val)
{
	dsnr(name, val, &dslist);
}

void
as(Rune *name, Rune *val)
{
	Rune *p, *q;

	p = getds(name);
	if(p == nil)
		p = L("");
	q = runemalloc(runestrlen(p)+runestrlen(val)+1);
	runestrcpy(q, p);
	runestrcat(q, val);
	ds(name, q);
	free(q);
}

Rune*
getds(Rune *name)
{
	return getdsnr(name, dslist);
}

void
printds(int t)
{
	int n, total;
	Reg *s;

	total = 0;
	for(s=dslist; s; s=s->next){
		if(s->val)
			n = runestrlen(s->val);
		else
			n = 0;
		total += n;
		if(!t)
			fprint(2, "%S\t%d\n", s->name, n);
	}
	fprint(2, "total\t%d\n", total);
}

void
nr(Rune *name, int val)
{
	Rune buf[20];

	runesnprint(buf, nelem(buf), "%d", val);
	_nr(name, buf);
}

void
af(Rune *name, Rune *fmt)
{
	Reg *s;

	if(_getnr(name) == nil)
		_nr(name, L("0"));
	for(s=nrlist; s; s=s->next)
		if(runestrcmp(s->name, name) == 0)
			s->fmt = erunestrdup(fmt);
}

Rune*
getaf(Rune *name)
{
	Reg *s;

	for(s=nrlist; s; s=s->next)
		if(runestrcmp(s->name, name) == 0)
			return s->fmt;
	return nil;
}

void
printnr(void)
{
	Reg *r;

	for(r=nrlist; r; r=r->next)
		fprint(2, "%S %S %d\n", r->name, r->val, r->inc);
}

/*
 * Some internal number registers are actually strings,
 * so provide _ versions to get at them.
 */
void
_nr(Rune *name, Rune *val)
{
	dsnr(name, val, &nrlist);
}

Rune*
_getnr(Rune *name)
{
	return getdsnr(name, nrlist);
}

int
getnr(Rune *name)
{
	Rune *p;

	p = _getnr(name);
	if(p == nil)
		return 0;
	return eval(p);
}

/* new register */
void
r_nr(int argc, Rune **argv)
{
	Reg *s;

	if(argc < 2)
		return;
	if(argc < 3)
		nr(argv[1], 0);
	else{
		if(argv[2][0] == '+')
			nr(argv[1], getnr(argv[1])+eval(argv[2]+1));
		else if(argv[2][0] == '-')
			nr(argv[1], getnr(argv[1])-eval(argv[2]+1));
		else
			nr(argv[1], eval(argv[2]));
	}
	if(argc > 3){
		for(s=nrlist; s; s=s->next)
			if(runestrcmp(s->name, argv[1]) == 0)
				s->inc = eval(argv[3]);
	}
}

/* assign format */
void
r_af(int argc, Rune **argv)
{
	USED(argc);

	af(argv[1], argv[2]);
}

/* remove register */
void
r_rr(int argc, Rune **argv)
{
	int i;

	for(i=1; i<argc; i++)
		_nr(argv[i], nil);
}

/* fmt integer in base 26 */
void
alpha(Rune *buf, int n, int a)
{
	int i, v;

	i = 1;
	for(v=n; v>0; v/=26)
		i++;
	if(i == 0)
		i = 1;
	buf[i] = 0;
	while(i > 0){
		buf[--i] = a+n%26;
		n /= 26;
	}
}

struct romanv {
	char *s;
	int v;
} romanv[] =
{
	"m",	1000,
	"cm", 900,
	"d", 500,
	"cd", 400,
	"c", 100,
	"xc", 90,
	"l", 50,
	"xl", 40,
	"x", 10,
	"ix", 9,
	"v", 5,
	"iv", 4,
	"i", 1
};

/* fmt integer in roman numerals! */
void
roman(Rune *buf, int n, int upper)
{
	Rune *p;
	char *q;
	struct romanv *r;

	if(upper)
		upper = 'A' - 'a';
	if(n >= 5000 || n <= 0){
		runestrcpy(buf, L("-"));
		return;
	}
	p = buf;
	r = romanv;
	while(n > 0){
		while(n >= r->v){
			for(q=r->s; *q; q++)
				*p++ = *q + upper;
			n -= r->v;
		}
		r++;
	}
	*p = 0;
}

Rune*
getname(void)
{
	int i, c, cc;
	static Rune buf[100];

	/* XXX add [name] syntax as in groff */
	c = getnext();
	if(c < 0)
		return L("");
	if(c == '\n'){
		warn("newline in name\n");
		ungetnext(c);
		return L("");
	}
	if(c == '['){
		for(i=0; i<nelem(buf)-1; i++){
			if((c = getrune()) < 0)
				return L("");
			if(c == ']'){
				buf[i] = 0;
				return buf;
			}
			buf[i] = c;
		}
		return L("");
	}
	if(c != '('){
		buf[0] = c;
		buf[1] = 0;
		return buf;
	}
	c = getnext();
	cc = getnext();
	if(c < 0 || cc < 0)
		return L("");
	if(c == '\n' | cc == '\n'){
		warn("newline in \\n");
		ungetnext(cc);
		if(c == '\n')
			ungetnext(c);
	}
	buf[0] = c;
	buf[1] = cc;
	buf[2] = 0;
	return buf;
}

/* \n - return number register */
int
e_n(void)
{
	int inc, v, l;
	Rune *name, *fmt, buf[100];
	Reg *s;

	inc = getnext();
	if(inc < 0)
		return -1;
	if(inc != '+' && inc != '-'){
		ungetnext(inc);
		inc = 0;
	}
	name = getname();
	if(_getnr(name) == nil)
		_nr(name, L("0"));
	for(s=nrlist; s; s=s->next){
		if(runestrcmp(s->name, name) == 0){
			if(s->fmt == nil && !inc && s->val[0]){
				/* might be a string! */
				pushinputstring(s->val);
				return 0;
			}
			v = eval(s->val);
			if(inc){
				if(inc == '+')
					v += s->inc;
				else
					v -= s->inc;
				runesnprint(buf, nelem(buf), "%d", v);
				free(s->val);
				s->val = erunestrdup(buf);
			}
			fmt = s->fmt;
			if(fmt == nil)
				fmt = L("1");
			switch(fmt[0]){
			case 'i':
			case 'I':
				roman(buf, v, fmt[0]=='I');
				break;
			case 'a':
			case 'A':
				alpha(buf, v, fmt[0]);
				break;
			default:
				l = runestrlen(fmt);
				if(l == 0)
					l = 1;
				runesnprint(buf, sizeof buf, "%0*d", l, v);
				break;
			}
			pushinputstring(buf);
			return 0;
		}
	}
	pushinputstring(L(""));
	return 0;
}

/* \g - number register format */
int
e_g(void)
{
	Rune *p;

	p = getaf(getname());
	if(p == nil)
		p = L("1");
	pushinputstring(p);
	return 0;
}

void
r_pnr(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
	printnr();
}

void
t8init(void)
{
	addreq(L("nr"), r_nr, -1);
	addreq(L("af"), r_af, 2);
	addreq(L("rr"), r_rr, -1);
	addreq(L("pnr"), r_pnr, 0);

	addesc('n', e_n, CopyMode|ArgMode|HtmlMode);
	addesc('g', e_g, 0);
}
