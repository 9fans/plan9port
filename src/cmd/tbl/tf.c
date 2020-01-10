/* tf.c: save and restore fill mode around table */
# include "t.h"

void
savefill(void)
{
			/* remembers various things: fill mode, vs, ps in mac 35 (SF) */
	Bprint(&tabout, ".de %d\n", SF);
	Bprint(&tabout, ".ps \\n(.s\n");
	Bprint(&tabout, ".vs \\n(.vu\n");
	Bprint(&tabout, ".in \\n(.iu\n");
	Bprint(&tabout, ".if \\n(.u .fi\n");
	Bprint(&tabout, ".if \\n(.j .ad\n");
	Bprint(&tabout, ".if \\n(.j=0 .na\n");
	Bprint(&tabout, "..\n");
	Bprint(&tabout, ".nf\n");
	/* set obx offset if useful */
	Bprint(&tabout, ".nr #~ 0\n");
	Bprint(&tabout, ".if \\n(.T .if n .nr #~ 0.6n\n");
}


void
rstofill(void)
{
	Bprint(&tabout, ".%d\n", SF);
}


void
endoff(void)
{
	int	i;

	for (i = 0; i < MAXHEAD; i++)
		if (linestop[i])
			Bprint(&tabout, ".nr #%c 0\n", linestop[i] + 'a' - 1);
	for (i = 0; i < texct; i++)
		Bprint(&tabout, ".rm %c+\n", texstr[i]);
	Bprint(&tabout, "%s\n", last);
}


void
ifdivert(void)
{
	Bprint(&tabout, ".ds #d .d\n");
	Bprint(&tabout, ".if \\(ts\\n(.z\\(ts\\(ts .ds #d nl\n");
}


void
saveline(void)
{
	Bprint(&tabout, ".if \\n+(b.=1 .nr d. \\n(.c-\\n(c.-1\n");
	linstart = iline;
}


void
restline(void)
{
	Bprint(&tabout, ".if \\n-(b.=0 .nr c. \\n(.c-\\n(d.-%d\n", iline - linstart);
	linstart = 0;
}


void
cleanfc(void)
{
	Bprint(&tabout, ".fc\n");
}
