#include "e.h"

void mark(int p1)
{
	markline = 1;
	printf(".ds %d \\k(09\\*(%d\n", p1, p1);
	yyval = p1;
	dprintf(".\tmark %d\n", p1);
}

void lineup(int p1)
{
	markline = 2;
	if (p1 == 0) {
		yyval = salloc();
		printf(".ds %d \\h'|\\n(09u'\n", yyval);
	}
	dprintf(".\tlineup %d\n", p1);
}
