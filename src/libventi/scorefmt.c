#include <u.h>
#include <libc.h>
#include <venti.h>

int
vtscorefmt(Fmt *f)
{
	uchar *v;
	int i;

	v = va_arg(f->args, uchar*);
	if(v == nil)
		fmtprint(f, "*");
	else
		for(i = 0; i < VtScoreSize; i++)
			fmtprint(f, "%2.2ux", v[i]);
	return 0;
}
