/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

Rune*
runeseprint(Rune *buf, Rune *e, char *fmt, ...)
{
	Rune *p;
	va_list args;

	va_start(args, fmt);
	p = runevseprint(buf, e, fmt, args);
	va_end(args);
	return p;
}
