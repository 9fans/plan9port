#include "a.h"

/*
 * Section 2 - Font and character size control.
 */

/* 2.1 - Character set */
/* XXX
 *
 * \C'name' - character named name
 * \N'n' - character number
 * \(xx - two-letter character
 * \-
 * \`
 * \'
 * `
 * '
 * -
 */

Rune*
getqarg(void)
{
	static Rune buf[MaxLine];
	int c;
	Rune *p, *e;

	p = buf;
	e = p + nelem(buf) - 1;

	if(getrune() != '\'')
		return nil;
	while(p < e){
		c = getrune();
		if(c < 0)
			return nil;
		if(c == '\'')
			break;
		*p++ = c;
	}
	*p = 0;
	return buf;
}

int
e_N(void)
{
	Rune *a;
	if((a = getqarg()) == nil)
		goto error;
	return eval(a);

error:
	warn("malformed %CN'...'", backslash);
	return 0;
}

int
e_paren(void)
{
	int c, cc;
	Rune buf[2], r;

	if((c = getrune()) < 0 || c == '\n')
		goto error;
	if((cc = getrune()) < 0 || cc == '\n')
		goto error;
	buf[0] = c;
	buf[1] = cc;
	r = troff2rune(buf);
 	if(r == Runeerror)
		warn("unknown char %C(%C%C", backslash, c, cc);
	return r;

error:
	warn("malformed %C(xx", backslash);
	return 0;
}

/* 2.2 - Fonts */
Rune fonttab[10][100];

/*
 * \fx \f(xx \fN - font change
 * number register .f - current font
 * \f0 previous font (undocumented?)
 */
/* change to font f.  also \fx, \f(xx, \fN */
/* .ft LongName is okay - temporarily at fp 0 */
void
ft(Rune *f)
{
	int i;
	int fn;

	if(f && runestrcmp(f, L("P")) == 0)
		f = nil;
	if(f == nil)
		fn = 0;
	else if(isdigit(f[0]))
		fn = eval(f);
	else{
		for(i=0; i<nelem(fonttab); i++){
			if(runestrcmp(fonttab[i], f) == 0){
				fn = i;
				goto have;
			}
		}
		warn("unknown font %S", f);
		fn = 1;
	}
have:
	if(fn < 0 || fn >= nelem(fonttab)){
		warn("unknown font %d", fn);
		fn = 1;
	}
	if(fn == 0)
		fn = getnr(L(".f0"));
	nr(L(".f0"), getnr(L(".f")));
	nr(L(".f"), fn);
	runmacro1(L("font"));
}

/* mount font named f on physical position N */
void
fp(int i, Rune *f)
{
	if(i <= 0 || i >= nelem(fonttab)){
		warn("bad font position %d", i);
		return;
	}
	runestrecpy(fonttab[i], fonttab[i]+sizeof fonttab[i], f);
}

int
e_f(void)
{
	ft(getname());
	return 0;
}

void
r_ft(int argc, Rune **argv)
{
	if(argc == 1)
		ft(nil);
	else
		ft(argv[1]);
}

void
r_fp(int argc, Rune **argv)
{
	if(argc < 3){
		warn("missing arguments to %Cfp", dot);
		return;
	}
	fp(eval(argv[1]), argv[2]);
}

/* 2.3 - Character size */

/* \H'±N' sets height */

void
ps(int s)
{
	if(s == 0)
		s = getnr(L(".s0"));
	nr(L(".s0"), getnr(L(".s")));
	nr(L(".s"), s);
	runmacro1(L("font"));
}

/* set point size */
void
r_ps(int argc, Rune **argv)
{
	Rune *p;

	if(argc == 1 || argv[1][0] == 0)
		ps(0);
	else{
		p = argv[1];
		if(p[0] == '-')
			ps(getnr(L(".s"))-eval(p+1));
		else if(p[0] == '+')
			ps(getnr(L(".s"))+eval(p+1));
		else
			ps(eval(p));
	}
}

int
e_s(void)
{
	int c, cc, ccc, n, twodigit;

	c = getnext();
	if(c < 0)
		return 0;
	if(c == '+' || c == '-'){
		cc = getnext();
		if(cc == '('){
			cc = getnext();
			ccc = getnext();
			if(cc < '0' || cc > '9' || ccc < '0' || ccc > '9'){
				warn("bad size %Cs%C(%C%C", backslash, c, cc, ccc);
				return 0;
			}
			n = (cc-'0')*10+ccc-'0';
		}else{
			if(cc < '0' || cc > '9'){
				warn("bad size %Cs%C%C", backslash, c, cc);
				return 0;
			}
			n = cc-'0';
		}
		if(c == '+')
			ps(getnr(L(".s"))+n);
		else
			ps(getnr(L(".s"))-n);
		return 0;
	}
	twodigit = 0;
	if(c == '('){
		twodigit = 1;
		c = getnext();
		if(c < 0)
			return 0;
	}
	if(c < '0' || c > '9'){
		warn("bad size %Cs%C", backslash, c);
		ungetnext(c);
		return 0;
	}
	if(twodigit || (c < '4' && c != '0')){
		cc = getnext();
		if(c < 0)
			return 0;
		n = (c-'0')*10+cc-'0';
	}else
		n = c-'0';
	ps(n);
	return 0;
}

void
t2init(void)
{
	fp(1, L("R"));
	fp(2, L("I"));
	fp(3, L("B"));
	fp(4, L("BI"));
	fp(5, L("CW"));

	nr(L(".s"), 10);
	nr(L(".s0"), 10);

	addreq(L("ft"), r_ft, -1);
	addreq(L("fp"), r_fp, -1);
	addreq(L("ps"), r_ps, -1);
	addreq(L("ss"), r_warn, -1);
	addreq(L("cs"), r_warn, -1);
	addreq(L("bd"), r_warn, -1);

	addesc('f', e_f, 0);
	addesc('s', e_s, 0);
	addesc('(', e_paren, 0);	/* ) */
	addesc('C', e_warn, 0);
	addesc('N', e_N, 0);
	/* \- \' \` are handled in html.c */
}
