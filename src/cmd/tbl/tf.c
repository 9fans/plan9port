/* tf.c: save and restore fill mode around table */
# include "t.h"

void
savefill(void)
{
			/* remembers various things: fill mode, vs, ps in mac 35 (SF) */
	fprintf(tabout, ".de %d\n", SF);
	fprintf(tabout, ".ps \\n(.s\n");
	fprintf(tabout, ".vs \\n(.vu\n");
	fprintf(tabout, ".in \\n(.iu\n");
	fprintf(tabout, ".if \\n(.u .fi\n");
	fprintf(tabout, ".if \\n(.j .ad\n");
	fprintf(tabout, ".if \\n(.j=0 .na\n");
	fprintf(tabout, "..\n");
	fprintf(tabout, ".nf\n");
	/* set obx offset if useful */
	fprintf(tabout, ".nr #~ 0\n");
	fprintf(tabout, ".if \\n(.T .if n .nr #~ 0.6n\n");
}


void
rstofill(void)
{
	fprintf(tabout, ".%d\n", SF);
}


void
endoff(void)
{
	int	i;

	for (i = 0; i < MAXHEAD; i++)
		if (linestop[i])
			fprintf(tabout, ".nr #%c 0\n", linestop[i] + 'a' - 1);
	for (i = 0; i < texct; i++)
		fprintf(tabout, ".rm %c+\n", texstr[i]);
	fprintf(tabout, "%s\n", last);
}


void
ifdivert(void)
{
	fprintf(tabout, ".ds #d .d\n");
	fprintf(tabout, ".if \\(ts\\n(.z\\(ts\\(ts .ds #d nl\n");
}


void
saveline(void)
{
	fprintf(tabout, ".if \\n+(b.=1 .nr d. \\n(.c-\\n(c.-1\n");
	linstart = iline;
}


void
restline(void)
{
	fprintf(tabout, ".if \\n-(b.=0 .nr c. \\n(.c-\\n(d.-%d\n", iline - linstart);
	linstart = 0;
}


void
cleanfc(void)
{
	fprintf(tabout, ".fc\n");
}


