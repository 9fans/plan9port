/* t7.c: control to write table entries */
# include "t.h"
# define realsplit ((ct=='a'||ct=='n') && table[ldata][c].rcol)

void
runout(void)
{
	int	i;

	if (boxflg || allflg || dboxflg)
		need();
	if (ctrflg) {
		Bprint(&tabout, ".nr #I \\n(.i\n");
		Bprint(&tabout, ".in +(\\n(.lu-\\n(TWu-\\n(.iu)/2u\n");
	}
	Bprint(&tabout, ".fc %c %c\n", F1, F2);
	Bprint(&tabout, ".nr #T 0-1\n");
	deftail();
	for (i = 0; i < nlin; i++)
		putline(i, i);
	if (leftover)
		yetmore();
	Bprint(&tabout, ".fc\n");
	Bprint(&tabout, ".nr T. 1\n");
	Bprint(&tabout, ".T# 1\n");
	if (ctrflg)
		Bprint(&tabout, ".in \\n(#Iu\n");
}


void
runtabs(int lform, int ldata)
{
	int	c, ct, vforml, lf;

	Bprint(&tabout, ".ta ");
	for (c = 0; c < ncol; c++) {
		vforml = lform;
		for (lf = prev(lform); lf >= 0 && vspen(table[lf][c].col); lf = prev(lf))
			vforml = lf;
		if (fspan(vforml, c))
			continue;
		switch (ct = ctype(vforml, c)) {
		case 'n':
		case 'a':
			if (table[ldata][c].rcol)
				if (lused[c]) /*Zero field width*/
					Bprint(&tabout, "\\n(%2su ", reg(c, CMID));
		case 'c':
		case 'l':
		case 'r':
			if (realsplit ? rused[c] : (used[c] + lused[c]))
				Bprint(&tabout, "\\n(%2su ", reg(c, CRIGHT));
			continue;
		case 's':
			if (lspan(lform, c))
				Bprint(&tabout, "\\n(%2su ", reg(c, CRIGHT));
			continue;
		}
	}
	Bprint(&tabout, "\n");
}


int
ifline(char *s)
{
	if (!point(s))
		return(0);
	if (s[0] == '\\')
		s++;
	if (s[1] )
		return(0);
	if (s[0] == '_')
		return('-');
	if (s[0] == '=')
		return('=');
	return(0);
}


void
need(void)
{
	int	texlin, horlin, i;

	for (texlin = horlin = i = 0; i < nlin; i++) {
		if (fullbot[i] != 0)
			horlin++;
		else if (instead[i] != 0)
			continue;
		else
			texlin++;
	}
	Bprint(&tabout, ".ne %dv+%dp\n", texlin, 2 * horlin);
}


void
deftail(void)
{
	int	i, c, lf, lwid;

	for (i = 0; i < MAXHEAD; i++)
		if (linestop[i])
			Bprint(&tabout, ".nr #%c 0-1\n", linestop[i] + 'a' - 1);
	Bprint(&tabout, ".nr #a 0-1\n");
	Bprint(&tabout, ".eo\n");
	Bprint(&tabout, ".de T#\n");
	Bprint(&tabout, ".nr 35 1m\n");
	Bprint(&tabout, ".ds #d .d\n");
	Bprint(&tabout, ".if \\(ts\\n(.z\\(ts\\(ts .ds #d nl\n");
	Bprint(&tabout, ".mk ##\n");
	Bprint(&tabout, ".nr ## -1v\n");
	Bprint(&tabout, ".ls 1\n");
	for (i = 0; i < MAXHEAD; i++)
		if (linestop[i])
			Bprint(&tabout, ".if \\n(#T>=0 .nr #%c \\n(#T\n",
			     linestop[i] + 'a' - 1);
	if (boxflg || allflg || dboxflg) /* bottom of table line */
		if (fullbot[nlin-1] == 0) {
			if (!pr1403)
				Bprint(&tabout, ".if \\n(T. .vs \\n(.vu-\\n(.sp\n");
			Bprint(&tabout, ".if \\n(T. ");
			drawline(nlin, 0, ncol, dboxflg ? '=' : '-', 1, 0);
			Bprint(&tabout, "\n.if \\n(T. .vs\n");
			/* T. is really an argument to a macro but because of
		   eqn we don't dare pass it as an argument and reference by $1 */
		}
	for (c = 0; c < ncol; c++) {
		if ((lf = left(nlin - 1, c, &lwid)) >= 0) {
			Bprint(&tabout, ".if \\n(#%c>=0 .sp -1\n", linestop[lf] + 'a' - 1);
			Bprint(&tabout, ".if \\n(#%c>=0 ", linestop[lf] + 'a' - 1);
			tohcol(c);
			drawvert(lf, nlin - 1, c, lwid);
			Bprint(&tabout, "\\h'|\\n(TWu'\n");
		}
	}
	if (boxflg || allflg || dboxflg) /* right hand line */ {
		Bprint(&tabout, ".if \\n(#a>=0 .sp -1\n");
		Bprint(&tabout, ".if \\n(#a>=0 \\h'|\\n(TWu'");
		drawvert (0, nlin - 1, ncol, dboxflg ? 2 : 1);
		Bprint(&tabout, "\n");
	}
	Bprint(&tabout, ".ls\n");
	Bprint(&tabout, "..\n");
	Bprint(&tabout, ".ec\n");
}
