#include	"lib9.h"
#include	<bio.h>

int
Bprint(Biobuf *bp, char *fmt, ...)
{
	va_list args;
	Fmt f;
	int n;

	if(Bfmtinit(&f, bp) < 0)
		return -1;
	n = fmtprint(&f, fmt, args);
	if(n > 0 && Bfmtflush(&f) < 0)
		return -1;
	return n;
}
