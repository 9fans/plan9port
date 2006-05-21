/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdlib.h>
#include <stdarg.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

char*
fmtstrflush(Fmt *f)
{
	if(f->start == nil)
		return nil;
	*(char*)f->to = '\0';
	f->to = f->start;
	return (char*)f->start;
}
