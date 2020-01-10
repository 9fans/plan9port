/* tg.c: process included text blocks */
# include "t.h"

int
gettext(char *sp, int ilin, int icol, char *fn, char *sz)
{
					/* get a section of text */
	char	line[4096];
	int	oname, startline;
	char	*vs;

	startline = iline;
	if (texname == 0)
		error("Too many text block diversions");
	if (textflg == 0) {
		Bprint(&tabout, ".nr %d \\n(.lu\n", SL); /* remember old line length */
		textflg = 1;
	}
	Bprint(&tabout, ".eo\n");
	Bprint(&tabout, ".am %s\n", reg(icol, CRIGHT));
	Bprint(&tabout, ".br\n");
	Bprint(&tabout, ".di %c+\n", texname);
	rstofill();
	if (fn && *fn)
		Bprint(&tabout, ".nr %d \\n(.f\n.ft %s\n", S1, fn);
	Bprint(&tabout, ".ft \\n(.f\n"); /* protect font */
	vs = vsize[icol][stynum[ilin]];
	if ((sz && *sz) || (vs && *vs)) {
		Bprint(&tabout, ".nr %d \\n(.v\n", S9);
		if (vs == 0 || *vs == 0)
			vs = "\\n(.s+2";
		if (sz && *sz)
			Bprint(&tabout, ".ps %s\n", sz);
		Bprint(&tabout, ".vs %s\n", vs);
		Bprint(&tabout, ".if \\n(%du>\\n(.vu .sp \\n(%du-\\n(.vu\n", S9, S9);
	}
	if (cll[icol][0])
		Bprint(&tabout, ".ll %sn\n", cll[icol]);
	else
		Bprint(&tabout, ".ll \\n(%du*%du/%du\n", SL, ctspan(ilin, icol), ncol + 1);
	Bprint(&tabout, ".if \\n(.l<\\n(%2s .ll \\n(%2su\n", reg(icol, CRIGHT),
	     reg(icol, CRIGHT));
	if (ctype(ilin, icol) == 'a')
		Bprint(&tabout, ".ll -2n\n");
	Bprint(&tabout, ".in 0\n");
	for (;;) {
		if (gets1(line, sizeof(line)) == nil) {
			iline = startline;
			error("missing closing T}");
		}
		if (line[0] == 'T' && line[1] == '}' && line[2] == tab)
			break;
		if (match("T}", line))
			break;
		Bprint(&tabout, "%s\n", line);
	}
	if (fn && *fn)
		Bprint(&tabout, ".ft \\n(%d\n", S1);
	if (sz && *sz)
		Bprint(&tabout, ".br\n.ps\n.vs\n");
	Bprint(&tabout, ".br\n");
	Bprint(&tabout, ".di\n");
	Bprint(&tabout, ".nr %c| \\n(dn\n", texname);
	Bprint(&tabout, ".nr %c- \\n(dl\n", texname);
	Bprint(&tabout, "..\n");
	Bprint(&tabout, ".ec \\\n");
	/* copy remainder of line */
	if (line[2])
		tcopy (sp, line + 3);
	else
		*sp = 0;
	oname = texname;
	texname = texstr[++texct];
	return(oname);
}


void
untext(void)
{
	rstofill();
	Bprint(&tabout, ".nf\n");
	Bprint(&tabout, ".ll \\n(%du\n", SL);
}
