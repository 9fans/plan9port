/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

char*
seprint(char *buf, char *e, char *fmt, ...)
{
	char *p;
	va_list args;

	va_start(args, fmt);
	p = vseprint(buf, e, fmt, args);
	va_end(args);
	return p;
}
