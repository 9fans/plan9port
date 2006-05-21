/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <stdlib.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

Rune*
runefmtstrflush(Fmt *f)
{
	if(f->start == nil)
		return nil;
	*(Rune*)f->to = '\0';
	f->to = f->start;
	return f->start;
}
