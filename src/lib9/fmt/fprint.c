/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
fprint(int fd, char *fmt, ...)
{
	int n;
	va_list args;

	va_start(args, fmt);
	n = vfprint(fd, fmt, args);
	va_end(args);
	return n;
}
