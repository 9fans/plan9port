/* tg.c: process included text blocks */
# include "t.h"

int
gettext(char *sp, int ilin, int icol, char *fn, char *sz)
{
					/* get a section of text */
	char	line[4096];
	int	oname;
	char	*vs;

	if (texname == 0) 
		error("Too many text block diversions");
	if (textflg == 0) {
		fprintf(tabout, ".nr %d \\n(.lu\n", SL); /* remember old line length */
		textflg = 1;
	}
	fprintf(tabout, ".eo\n");
	fprintf(tabout, ".am %s\n", reg(icol, CRIGHT));
	fprintf(tabout, ".br\n");
	fprintf(tabout, ".di %c+\n", texname);
	rstofill();
	if (fn && *fn) 
		fprintf(tabout, ".nr %d \\n(.f\n.ft %s\n", S1, fn);
	fprintf(tabout, ".ft \\n(.f\n"); /* protect font */
	vs = vsize[icol][stynum[ilin]];
	if ((sz && *sz) || (vs && *vs)) {
		fprintf(tabout, ".nr %d \\n(.v\n", S9);
		if (vs == 0 || *vs == 0) 
			vs = "\\n(.s+2";
		if (sz && *sz)
			fprintf(tabout, ".ps %s\n", sz);
		fprintf(tabout, ".vs %s\n", vs);
		fprintf(tabout, ".if \\n(%du>\\n(.vu .sp \\n(%du-\\n(.vu\n", S9, S9);
	}
	if (cll[icol][0])
		fprintf(tabout, ".ll %sn\n", cll[icol]);
	else
		fprintf(tabout, ".ll \\n(%du*%du/%du\n", SL, ctspan(ilin, icol), ncol + 1);
	fprintf(tabout, ".if \\n(.l<\\n(%2s .ll \\n(%2su\n", reg(icol, CRIGHT),
	     reg(icol, CRIGHT));
	if (ctype(ilin, icol) == 'a')
		fprintf(tabout, ".ll -2n\n");
	fprintf(tabout, ".in 0\n");
	while (gets1(line, sizeof(line))) {
		if (line[0] == 'T' && line[1] == '}' && line[2] == tab) 
			break;
		if (match("T}", line)) 
			break;
		fprintf(tabout, "%s\n", line);
	}
	if (fn && *fn) 
		fprintf(tabout, ".ft \\n(%d\n", S1);
	if (sz && *sz) 
		fprintf(tabout, ".br\n.ps\n.vs\n");
	fprintf(tabout, ".br\n");
	fprintf(tabout, ".di\n");
	fprintf(tabout, ".nr %c| \\n(dn\n", texname);
	fprintf(tabout, ".nr %c- \\n(dl\n", texname);
	fprintf(tabout, "..\n");
	fprintf(tabout, ".ec \\\n");
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
	fprintf(tabout, ".nf\n");
	fprintf(tabout, ".ll \\n(%du\n", SL);
}


