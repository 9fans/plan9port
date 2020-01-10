#include "a.h"

/*
 * 11. Local Horizontal and Vertical Motions, and the Width Function.
 */

int
e_0(void)
{
	/* digit-width space */
	return ' ';
}

int
dv(int d)
{
	Rune sub[6];

	d += getnr(L(".dv"));
	nr(L(".dv"), d);

	runestrcpy(sub, L("<sub>"));
	sub[0] = Ult;
	sub[4] = Ugt;
	if(d < 0){
		sub[3] = 'p';
		ihtml(L(".dv"), sub);
	}else if(d > 0)
		ihtml(L(".dv"), sub);
	else
		ihtml(L(".dv"), nil);
	return 0;
}

int
e_v(void)
{
	dv(eval(getqarg()));
	return 0;
}

int
e_u(void)
{
	dv(eval(L("-0.5m")));
	return 0;
}

int
e_d(void)
{
	dv(eval(L("0.5m")));
	return 0;
}

int
e_r(void)
{
	dv(eval(L("-1m")));
	return 0;
}

int
e_h(void)
{
	getqarg();
	return 0;
}

int
e_w(void)
{
	Rune *a;
	Rune buf[40];
	static Rune zero;

	a = getqarg();
	if(a == nil){
		warn("no arg for \\w");
		a = &zero;
	}
	runesnprint(buf, sizeof buf, "%ld", runestrlen(a));
	pushinputstring(buf);
	nr(L("st"), 0);
	nr(L("sb"), 0);
	nr(L("ct"), 0);
	return 0;
}

int
e_k(void)
{
	getname();
	warn("%Ck not available", backslash);
	return 0;
}

void
t11init(void)
{
	addesc('|', e_nop, 0);
	addesc('^', e_nop, 0);
	addesc('v', e_v, 0);
	addesc('h', e_h, 0);
	addesc('w', e_w, 0);
	addesc('0', e_0, 0);
	addesc('u', e_u, 0);
	addesc('d', e_d, 0);
	addesc('r', e_r, 0);
	addesc('k', e_k, 0);
}
