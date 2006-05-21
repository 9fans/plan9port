/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

Rune*
runesmprint(char *fmt, ...)
{
	va_list args;
	Rune *p;

	va_start(args, fmt);
	p = runevsmprint(fmt, args);
	va_end(args);
	return p;
}
