/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
snprint(char *buf, int len, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}
