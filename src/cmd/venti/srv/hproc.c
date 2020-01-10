#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "xml.h"

int
hproc(HConnect *c)
{
	int r;

	if((r = hsettext(c)) < 0)
		return r;
	hprint(&c->hout, "/proc only implemented on Plan 9\n");
	hflush(&c->hout);
	return 0;
}
