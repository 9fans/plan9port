/* t8.c: write out one line of output table */
# include "t.h"
# define realsplit ((ct=='a'||ct=='n') && table[nl][c].rcol)
int	watchout;
int	once;

void
putline(int i, int nl)
				/* i is line number for deciding format */
				/* nl is line number for finding data   usually identical */
{
	int	c, s, lf, ct, form, lwid, vspf, ip, cmidx, exvspen, vforml;
	int	vct, chfont, uphalf;
	char	*ss, *size, *fn, *rct;

	cmidx = watchout = vspf = exvspen = 0;
	if (i == 0)
		once = 0;
	if (i == 0 && ( allflg || boxflg || dboxflg))
		fullwide(0,   dboxflg ? '=' : '-');
	if (instead[nl] == 0 && fullbot[nl] == 0)
		for (c = 0; c < ncol; c++) {
			ss = table[nl][c].col;
			if (ss == 0)
				continue;
			if (vspen(ss)) {
				for (ip = nl; ip < nlin; ip = next(ip))
					if (!vspen(ss = table[ip][c].col))
						break;
				s = (int)(uintptr)ss;
				if (s > 0 && s < 128)
					Bprint(&tabout, ".ne \\n(%c|u+\\n(.Vu\n", (int)s);
				continue;
			}
			if (point(ss))
				continue;
			s = (int)(uintptr)ss;
			Bprint(&tabout, ".ne \\n(%c|u+\\n(.Vu\n", s);
			watchout = 1;
		}
	if (linestop[nl])
		Bprint(&tabout, ".mk #%c\n", linestop[nl] + 'a' - 1);
	lf = prev(nl);
	if (instead[nl]) {
		Bprint(&tabout, "%s\n", instead[nl]);
		return;
	}
	if (fullbot[nl]) {
		switch (ct = fullbot[nl]) {
		case '=':
		case '-':
			fullwide(nl, ct);
		}
		return;
	}
	for (c = 0; c < ncol; c++) {
		if (instead[nl] == 0 && fullbot[nl] == 0)
			if (vspen(table[nl][c].col))
				vspf = 1;
		if (lf >= 0)
			if (vspen(table[lf][c].col))
				vspf = 1;
	}
	if (vspf) {
		Bprint(&tabout, ".nr #^ \\n(\\*(#du\n");
		Bprint(&tabout, ".nr #- \\n(#^\n"); /* current line position relative to bottom */
	}
	vspf = 0;
	chfont = 0;
	for (c = 0; c < ncol; c++) {
		ss = table[nl][c].col;
		if (ss == 0)
			continue;
		if(font[c][stynum[nl]])
			chfont = 1;
		if (point(ss) )
			continue;
		s = (int)(uintptr)ss;
		lf = prev(nl);
		if (lf >= 0 && vspen(table[lf][c].col))
			Bprint(&tabout,
			   ".if (\\n(%c|+\\n(^%c-1v)>\\n(#- .nr #- +(\\n(%c|+\\n(^%c-\\n(#--1v)\n",
			    s, 'a' + c, s, 'a' + c);
		else
			Bprint(&tabout,
			    ".if (\\n(%c|+\\n(#^-1v)>\\n(#- .nr #- +(\\n(%c|+\\n(#^-\\n(#--1v)\n",
			    (int)s, (int)s);
	}
	if (allflg && once > 0 )
		fullwide(i, '-');
	once = 1;
	runtabs(i, nl);
	if (allh(i) && !pr1403) {
		Bprint(&tabout, ".nr %d \\n(.v\n", SVS);
		Bprint(&tabout, ".vs \\n(.vu-\\n(.sp\n");
		Bprint(&tabout, ".nr 35 \\n(.vu\n");
	} else
		Bprint(&tabout, ".nr 35 1m\n");
	if (chfont)
		Bprint(&tabout, ".nr %2d \\n(.f\n", S1);
	Bprint(&tabout, "\\&");
	vct = 0;
	for (c = 0; c < ncol; c++) {
		uphalf = 0;
		if (watchout == 0 && i + 1 < nlin && (lf = left(i, c, &lwid)) >= 0) {
			tohcol(c);
			drawvert(lf, i, c, lwid);
			vct += 2;
		}
		if (rightl && c + 1 == ncol)
			continue;
		vforml = i;
		for (lf = prev(nl); lf >= 0 && vspen(table[lf][c].col); lf = prev(lf))
			vforml = lf;
		form = ctype(vforml, c);
		if (form != 's') {
			rct = reg(c, CLEFT);
			if (form == 'a')
				rct = reg(c, CMID);
			if (form == 'n' && table[nl][c].rcol && lused[c] == 0)
				rct = reg(c, CMID);
			Bprint(&tabout, "\\h'|\\n(%2su'", rct);
		}
		ss = table[nl][c].col;
		fn = font[c][stynum[vforml]];
		size = csize[c][stynum[vforml]];
		if (*size == 0)
			size = 0;
		if ((flags[c][stynum[nl]] & HALFUP) != 0 && pr1403 == 0)
			uphalf = 1;
		switch (ct = ctype(vforml, c)) {
		case 'n':
		case 'a':
			if (table[nl][c].rcol) {
				if (lused[c]) /*Zero field width*/ {
					ip = prev(nl);
					if (ip >= 0)
						if (vspen(table[ip][c].col)) {
							if (exvspen == 0) {
								Bprint(&tabout, "\\v'-(\\n(\\*(#du-\\n(^%cu", c + 'a');
								if (cmidx)
/* code folded from here */
	Bprint(&tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c + 'a');
/* unfolding */
								vct++;
								if (pr1403) /* must round to whole lines */
/* code folded from here */
	Bprint(&tabout, "/1v*1v");
/* unfolding */
								Bprint(&tabout, "'");
								exvspen = 1;
							}
						}
					Bprint(&tabout, "%c%c", F1, F2);
					if (uphalf)
						Bprint(&tabout, "\\u");
					puttext(ss, fn, size);
					if (uphalf)
						Bprint(&tabout, "\\d");
					Bprint(&tabout, "%c", F1);
				}
				ss = table[nl][c].rcol;
				form = 1;
				break;
			}
		case 'c':
			form = 3;
			break;
		case 'r':
			form = 2;
			break;
		case 'l':
			form = 1;
			break;
		case '-':
		case '=':
			if (real(table[nl][c].col))
				fprint(2, "%s: line %d: Data ignored on table line %d\n", ifile, iline - 1, i + 1);
			makeline(i, c, ct);
			continue;
		default:
			continue;
		}
		if (realsplit ? rused[c] : used[c]) /*Zero field width*/ {
			/* form: 1 left, 2 right, 3 center adjust */
			if (ifline(ss)) {
				makeline(i, c, ifline(ss));
				continue;
			}
			if (filler(ss)) {
				Bprint(&tabout, "\\l'|\\n(%2su\\&%s'", reg(c, CRIGHT), ss + 2);
				continue;
			}
			ip = prev(nl);
			cmidx = (flags[c][stynum[nl]] & (CTOP | CDOWN)) == 0;
			if (ip >= 0)
				if (vspen(table[ip][c].col)) {
					if (exvspen == 0) {
						Bprint(&tabout, "\\v'-(\\n(\\*(#du-\\n(^%cu", c + 'a');
						if (cmidx)
							Bprint(&tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c + 'a');
						vct++;
						if (pr1403) /* round to whole lines */
							Bprint(&tabout, "/1v*1v");
						Bprint(&tabout, "'");
					}
				}
			Bprint(&tabout, "%c", F1);
			if (form != 1)
				Bprint(&tabout, "%c", F2);
			if (vspen(ss))
				vspf = 1;
			else
			 {
				if (uphalf)
					Bprint(&tabout, "\\u");
				puttext(ss, fn, size);
				if (uphalf)
					Bprint(&tabout, "\\d");
			}
			if (form != 2)
				Bprint(&tabout, "%c", F2);
			Bprint(&tabout, "%c", F1);
		}
		ip = prev(nl);
		if (ip >= 0)
			if (vspen(table[ip][c].col)) {
				exvspen = (c + 1 < ncol) && vspen(table[ip][c+1].col) &&
				    (topat[c] == topat[c+1]) &&
				    (cmidx == (flags[c+1] [stynum[nl]] & (CTOP | CDOWN) == 0))
				     && (left(i, c + 1, &lwid) < 0);
				if (exvspen == 0) {
					Bprint(&tabout, "\\v'(\\n(\\*(#du-\\n(^%cu", c + 'a');
					if (cmidx)
						Bprint(&tabout, "-((\\n(#-u-\\n(^%cu)/2u)", c + 'a');
					vct++;
					if (pr1403) /* round to whole lines */
						Bprint(&tabout, "/1v*1v");
					Bprint(&tabout, "'");
				}
			}
			else
				exvspen = 0;
		/* if lines need to be split for gcos here is the place for a backslash */
		if (vct > 7 && c < ncol) {
			Bprint(&tabout, "\n.sp-1\n\\&");
			vct = 0;
		}
	}
	Bprint(&tabout, "\n");
	if (allh(i) && !pr1403)
		Bprint(&tabout, ".vs \\n(%du\n", SVS);
	if (watchout)
		funnies(i, nl);
	if (vspf) {
		for (c = 0; c < ncol; c++)
			if (vspen(table[nl][c].col) && (nl == 0 || (lf = prev(nl)) < 0 ||
			    !vspen(table[lf][c].col))) {
				Bprint(&tabout, ".nr ^%c \\n(#^u\n", 'a' + c);
				topat[c] = nl;
			}
	}
}


void
puttext(char *s, char *fn, char *size)
{
	if (point(s)) {
		putfont(fn);
		putsize(size);
		Bprint(&tabout, "%s", s);
		if (*fn > 0)
			Bprint(&tabout, "\\f\\n(%2d", S1);
		if (size != 0)
			putsize("0");
	}
}


void
funnies(int stl, int lin)
{
					/* write out funny diverted things */
	int	c, s, pl, lwid, dv, lf, ct;
	char	*fn, *ss;

	Bprint(&tabout, ".mk ##\n");	 /* rmember current vertical position */
	Bprint(&tabout, ".nr %d \\n(##\n", S1);		 /* bottom position */
	for (c = 0; c < ncol; c++) {
		ss = table[lin][c].col;
		if (point(ss))
			continue;
		if (ss == 0)
			continue;
		s = (int)(uintptr)ss;
		Bprint(&tabout, ".sp |\\n(##u-1v\n");
		Bprint(&tabout, ".nr %d ", SIND);
		ct = 0;
		for (pl = stl; pl >= 0 && !isalpha(ct = ctype(pl, c)); pl = prev(pl))
			;
		switch (ct) {
		case 'n':
		case 'c':
			Bprint(&tabout, "(\\n(%2su+\\n(%2su-\\n(%c-u)/2u\n", reg(c, CLEFT),
			     reg(c - 1 + ctspan(lin, c), CRIGHT),
			     s);
			break;
		case 'l':
			Bprint(&tabout, "\\n(%2su\n", reg(c, CLEFT));
			break;
		case 'a':
			Bprint(&tabout, "\\n(%2su\n", reg(c, CMID));
			break;
		case 'r':
			Bprint(&tabout, "\\n(%2su-\\n(%c-u\n", reg(c, CRIGHT), s);
			break;
		}
		Bprint(&tabout, ".in +\\n(%du\n", SIND);
		fn = font[c][stynum[stl]];
		putfont(fn);
		pl = prev(stl);
		if (stl > 0 && pl >= 0 && vspen(table[pl][c].col)) {
			Bprint(&tabout, ".sp |\\n(^%cu\n", 'a' + c);
			if ((flags[c][stynum[stl]] & (CTOP | CDOWN)) == 0) {
				Bprint(&tabout, ".nr %d \\n(#-u-\\n(^%c-\\n(%c|+1v\n",
				     TMP, 'a' + c, s);
				Bprint(&tabout, ".if \\n(%d>0 .sp \\n(%du/2u", TMP, TMP);
				if (pr1403)		 /* round */
					Bprint(&tabout, "/1v*1v");
				Bprint(&tabout, "\n");
			}
		}
		Bprint(&tabout, ".%c+\n", s);
		Bprint(&tabout, ".in -\\n(%du\n", SIND);
		if (*fn > 0)
			putfont("P");
		Bprint(&tabout, ".mk %d\n", S2);
		Bprint(&tabout, ".if \\n(%d>\\n(%d .nr %d \\n(%d\n", S2, S1, S1, S2);
	}
	Bprint(&tabout, ".sp |\\n(%du\n", S1);
	for (c = dv = 0; c < ncol; c++) {
		if (stl + 1 < nlin && (lf = left(stl, c, &lwid)) >= 0) {
			if (dv++ == 0)
				Bprint(&tabout, ".sp -1\n");
			tohcol(c);
			dv++;
			drawvert(lf, stl, c, lwid);
		}
	}
	if (dv)
		Bprint(&tabout, "\n");
}


void
putfont(char *fn)
{
	if (fn && *fn)
		Bprint(&tabout,  fn[1] ? "\\f(%.2s" : "\\f%.2s",  fn);
}


void
putsize(char *s)
{
	if (s && *s)
		Bprint(&tabout, "\\s%s", s);
}
