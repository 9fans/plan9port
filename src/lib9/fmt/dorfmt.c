/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

/* format the output into f->to and return the number of characters fmted  */

/* BUG: THIS FILE IS NOT UPDATED TO THE  NEW SPEC */
int
dorfmt(Fmt *f, const Rune *fmt)
{
	Rune *rt, *rs;
	int r;
	char *t, *s;
	int nfmt;

	nfmt = f->nfmt;
	for(;;){
		if(f->runes){
			rt = (Rune*)f->to;
			rs = (Rune*)f->stop;
			while((r = *fmt++) && r != '%'){
				FMTRCHAR(f, rt, rs, r);
			}
			f->nfmt += rt - (Rune *)f->to;
			f->to = rt;
			if(!r)
				return f->nfmt - nfmt;
			f->stop = rs;
		}else{
			t = (char*)f->to;
			s = (char*)f->stop;
			while((r = *fmt++) && r != '%'){
				FMTRUNE(f, t, f->stop, r);
			}
			f->nfmt += t - (char *)f->to;
			f->to = t;
			if(!r)
				return f->nfmt - nfmt;
			f->stop = s;
		}

		fmt = (Rune*)__fmtdispatch(f, (Rune*)fmt, 1);
		if(fmt == nil)
			return -1;
	}
	return 0;		/* not reached */
}
