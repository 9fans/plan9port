/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
fmtrune(Fmt *f, int r)
{
	Rune *rt;
	char *t;
	int n;

	if(f->runes){
		rt = (Rune*)f->to;
		FMTRCHAR(f, rt, f->stop, r);
		f->to = rt;
		n = 1;
	}else{
		t = (char*)f->to;
		FMTRUNE(f, t, f->stop, r);
		n = t - (char*)f->to;
		f->to = t;
	}
	f->nfmt += n;
	return 0;
}
