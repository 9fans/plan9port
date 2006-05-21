/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
runesprint(Rune *buf, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = runevsnprint(buf, 256, fmt, args);
	va_end(args);
	return n;
}
