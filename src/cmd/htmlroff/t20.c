#include "a.h"

/*
 * 20. Miscellaneous
 */

/* .mc - margin character */
/* .ig - ignore; treated like a macro in t7.c */

/* .pm - print macros and strings */

void
r_pm(int argc, Rune **argv)
{
	int i;

	if(argc == 1){
		printds(0);
		return;
	}
	if(runestrcmp(argv[1], L("t")) == 0){
		printds(1);
		return;
	}
	for(i=1; i<argc; i++)
		fprint(2, "%S: %S\n", argv[i], getds(argv[i]));
}

void
r_tm(Rune *name)
{
	Rune *line;

	USED(name);

	line = readline(CopyMode);
	fprint(2, "%S\n", line);
	free(line);
}

void
r_ab(Rune *name)
{
	USED(name);

	r_tm(L("ab"));
	exits(".ab");
}

void
r_lf(int argc, Rune **argv)
{
	if(argc == 1)
		return;
	if(argc == 2)
		setlinenumber(nil, eval(argv[1]));
	if(argc == 3)
		setlinenumber(argv[2], eval(argv[1]));
}

void
r_fl(int argc, Rune **argv)
{
	USED(argc);
	USED(argv);
	Bflush(&bout);
}

void
t20init(void)
{
	addreq(L("mc"), r_warn, -1);
	addraw(L("tm"), r_tm);
	addraw(L("ab"), r_ab);
	addreq(L("lf"), r_lf, -1);
	addreq(L("pm"), r_pm, -1);
	addreq(L("fl"), r_fl, 0);
}
