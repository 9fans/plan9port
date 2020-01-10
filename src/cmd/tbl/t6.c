/* t6.c: compute tab stops */
# define tx(a) (a>0 && a<128)
# include "t.h"
# define FN(i,c) font[c][stynum[i]]
# define SZ(i,c) csize[c][stynum[i]]
# define TMP1 S1
# define TMP2 S2

void
maktab(void)			/* define the tab stops of the table */
{
	int	icol, ilin, tsep, k, ik, vforml, il, s, text;
	char	*ss;

	for (icol = 0; icol < ncol; icol++) {
		doubled[icol] = acase[icol] = 0;
		Bprint(&tabout, ".nr %2s 0\n", reg(icol, CRIGHT));
		for (text = 0; text < 2; text++) {
			if (text)
				Bprint(&tabout, ".%2s\n.rm %2s\n", reg(icol, CRIGHT),
				    reg(icol, CRIGHT));
			for (ilin = 0; ilin < nlin; ilin++) {
				if (instead[ilin] || fullbot[ilin])
					continue;
				vforml = ilin;
				for (il = prev(ilin); il >= 0 && vspen(table[il][icol].col); il = prev(il))
					vforml = il;
				if (fspan(vforml, icol))
					continue;
				if (filler(table[ilin][icol].col))
					continue;
				if ((flags[icol][stynum[ilin]] & ZEROW) != 0)
					continue;
				switch (ctype(vforml, icol)) {
				case 'a':
					acase[icol] = 1;
					ss = table[ilin][icol].col;
					s = (int)(uintptr)ss;
					if (s > 0 && s < 128 && text) {
						if (doubled[icol] == 0)
							Bprint(&tabout, ".nr %d 0\n.nr %d 0\n",
							    S1, S2);
						doubled[icol] = 1;
						Bprint(&tabout, ".if \\n(%c->\\n(%d .nr %d \\n(%c-\n",
						    s, S2, S2, s);
					}
				case 'n':
					if (table[ilin][icol].rcol != 0) {
						if (doubled[icol] == 0 && text == 0)
							Bprint(&tabout, ".nr %d 0\n.nr %d 0\n",
							    S1, S2);
						doubled[icol] = 1;
						if (real(ss = table[ilin][icol].col) && !vspen(ss)) {
							s = (int)(uintptr)ss;
							if (tx(s) != text)
								continue;
							Bprint(&tabout, ".nr %d ", TMP);
							wide(ss, FN(vforml, icol), SZ(vforml, icol));
							Bprint(&tabout, "\n");
							Bprint(&tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n",
							    S1, TMP, S1, TMP);
						}
						if (text == 0 && real(ss = table[ilin][icol].rcol) && !vspen(ss) && !barent(ss)) {
							Bprint(&tabout, ".nr %d \\w%c%s%c\n",
							    TMP, F1, ss, F1);
							Bprint(&tabout, ".if \\n(%d<\\n(%d .nr %d \\n(%d\n", S2, TMP, S2,
							     TMP);
						}
						continue;
					}
				case 'r':
				case 'c':
				case 'l':
					if (real(ss = table[ilin][icol].col) && !vspen(ss)) {
						s = (int)(uintptr)ss;
						if (tx(s) != text)
							continue;
						Bprint(&tabout, ".nr %d ", TMP);
						wide(ss, FN(vforml, icol), SZ(vforml, icol));
						Bprint(&tabout, "\n");
						Bprint(&tabout, ".if \\n(%2s<\\n(%d .nr %2s \\n(%d\n",
						     reg(icol, CRIGHT), TMP, reg(icol, CRIGHT), TMP);
					}
				}
			}
		}
		if (acase[icol]) {
			Bprint(&tabout, ".if \\n(%d>=\\n(%2s .nr %2s \\n(%du+2n\n",
			     S2, reg(icol, CRIGHT), reg(icol, CRIGHT), S2);
		}
		if (doubled[icol]) {
			Bprint(&tabout, ".nr %2s \\n(%d\n", reg(icol, CMID), S1);
			Bprint(&tabout, ".nr %d \\n(%2s+\\n(%d\n", TMP, reg(icol, CMID), S2);
			Bprint(&tabout, ".if \\n(%d>\\n(%2s .nr %2s \\n(%d\n", TMP,
			    reg(icol, CRIGHT), reg(icol, CRIGHT), TMP);
			Bprint(&tabout, ".if \\n(%d<\\n(%2s .nr %2s +(\\n(%2s-\\n(%d)/2\n",
			     TMP, reg(icol, CRIGHT), reg(icol, CMID), reg(icol, CRIGHT), TMP);
		}
		if (cll[icol][0]) {
			Bprint(&tabout, ".nr %d %sn\n", TMP, cll[icol]);
			Bprint(&tabout, ".if \\n(%2s<\\n(%d .nr %2s \\n(%d\n",
			    reg(icol, CRIGHT), TMP, reg(icol, CRIGHT), TMP);
		}
		for (ilin = 0; ilin < nlin; ilin++)
			if (k = lspan(ilin, icol)) {
				ss = table[ilin][icol-k].col;
				if (!real(ss) || barent(ss) || vspen(ss) )
					continue;
				Bprint(&tabout, ".nr %d ", TMP);
				wide(table[ilin][icol-k].col, FN(ilin, icol - k), SZ(ilin, icol - k));
				for (ik = k; ik >= 0; ik--) {
					Bprint(&tabout, "-\\n(%2s", reg(icol - ik, CRIGHT));
					if (!expflg && ik > 0)
						Bprint(&tabout, "-%dn", sep[icol-ik]);
				}
				Bprint(&tabout, "\n");
				Bprint(&tabout, ".if \\n(%d>0 .nr %d \\n(%d/%d\n", TMP,
				      TMP, TMP, k);
				Bprint(&tabout, ".if \\n(%d<0 .nr %d 0\n", TMP, TMP);
				for (ik = 1; ik <= k; ik++) {
					if (doubled[icol-k+ik])
						Bprint(&tabout, ".nr %2s +\\n(%d/2\n",
						     reg(icol - k + ik, CMID), TMP);
					Bprint(&tabout, ".nr %2s +\\n(%d\n",
					     reg(icol - k + ik, CRIGHT), TMP);
				}
			}
	}
	if (textflg)
		untext();
				/* if even requested, make all columns widest width */
	if (evenflg) {
		Bprint(&tabout, ".nr %d 0\n", TMP);
		for (icol = 0; icol < ncol; icol++) {
			if (evenup[icol] == 0)
				continue;
			Bprint(&tabout, ".if \\n(%2s>\\n(%d .nr %d \\n(%2s\n",
			    reg(icol, CRIGHT), TMP, TMP, reg(icol, CRIGHT));
		}
		for (icol = 0; icol < ncol; icol++) {
			if (evenup[icol] == 0)
				/* if column not evened just retain old interval */
				continue;
			if (doubled[icol])
				Bprint(&tabout, ".nr %2s (100*\\n(%2s/\\n(%2s)*\\n(%d/100\n",
				    reg(icol, CMID), reg(icol, CMID), reg(icol, CRIGHT), TMP);
			/* that nonsense with the 100's and parens tries
				   to avoid overflow while proportionally shifting
				   the middle of the number */
			Bprint(&tabout, ".nr %2s \\n(%d\n", reg(icol, CRIGHT), TMP);
		}
	}
				/* now adjust for total table width */
	for (tsep = icol = 0; icol < ncol; icol++)
		tsep += sep[icol];
	if (expflg) {
		Bprint(&tabout, ".nr %d 0", TMP);
		for (icol = 0; icol < ncol; icol++)
			Bprint(&tabout, "+\\n(%2s", reg(icol, CRIGHT));
		Bprint(&tabout, "\n");
		Bprint(&tabout, ".nr %d \\n(.l-\\n(%d\n", TMP, TMP);
		if (boxflg || dboxflg || allflg)
			/* tsep += 1; */ {}
		else
			tsep -= sep[ncol-1];
		Bprint(&tabout, ".nr %d \\n(%d/%d\n", TMP, TMP,  tsep);
		Bprint(&tabout, ".if \\n(%d<0 .nr %d 0\n", TMP, TMP);
	} else
		Bprint(&tabout, ".nr %d 1n\n", TMP);
	Bprint(&tabout, ".nr %2s 0\n", reg(-1, CRIGHT));
	tsep = (boxflg || allflg || dboxflg || left1flg) ? 2 : 0;
	if (sep[-1] >= 0)
		tsep = sep[-1];
	for (icol = 0; icol < ncol; icol++) {
		Bprint(&tabout, ".nr %2s \\n(%2s+((%d*\\n(%d)/2)\n", reg(icol, CLEFT),
		    reg(icol - 1, CRIGHT), tsep, TMP);
		Bprint(&tabout, ".nr %2s +\\n(%2s\n", reg(icol, CRIGHT), reg(icol, CLEFT));
		if (doubled[icol]) {
			/* the next line is last-ditch effort to avoid zero field width */
			/*Bprint(&tabout, ".if \\n(%2s=0 .nr %2s 1\n",reg(icol,CMID), reg(icol,CMID));*/
			Bprint(&tabout, ".nr %2s +\\n(%2s\n", reg(icol, CMID),
			    reg(icol, CLEFT));
			/*  Bprint(&tabout, ".if n .if \\n(%s%%24>0 .nr %s +12u\n",reg(icol,CMID), reg(icol,CMID)); */
		}
		tsep = sep[icol] * 2;
	}
	if (rightl)
		Bprint(&tabout, ".nr %s (\\n(%s+\\n(%s)/2\n", reg(ncol - 1, CRIGHT),
		      reg(ncol - 1, CLEFT), reg(ncol - 2, CRIGHT));
	Bprint(&tabout, ".nr TW \\n(%2s\n", reg(ncol - 1, CRIGHT));
	tsep = sep[ncol-1];
	if (boxflg || allflg || dboxflg)
		Bprint(&tabout, ".nr TW +((%d*\\n(%d)/2)\n", tsep, TMP);
	Bprint(&tabout,
	    ".if t .if (\\n(TW+\\n(.o)>7.65i .tm Table at line %d file %s is too wide - \\n(TW units\n", iline - 1, ifile);
	return;
}


void
wide(char *s, char *fn, char *size)
{
	if (point(s)) {
		Bprint(&tabout, "\\w%c", F1);
		if (*fn > 0)
			putfont(fn);
		if (*size)
			putsize(size);
		Bprint(&tabout, "%s", s);
		if (*fn > 0)
			putfont("P");
		if (*size)
			putsize("0");
		Bprint(&tabout, "%c", F1);
	} else
		Bprint(&tabout, "\\n(%c-", (int)(uintptr)s);
}


int
filler(char *s)
{
	return (point(s) && s[0] == '\\' && s[1] == 'R');
}
