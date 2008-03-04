/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <fmt.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
sprint(char *buf, char *fmt, ...)
{
	int n;
	uint len;
	va_list args;

	len = 1<<30;  /* big number, but sprint is deprecated anyway */
	/*
	 * on PowerPC, the stack is near the top of memory, so
	 * we must be sure not to overflow a 32-bit pointer.
	 *
	 * careful!  gcc-4.2 assumes buf+len < buf can never be true and
	 * optimizes the test away.  casting to uintptr works around this bug.
	 */
	if((uintptr)buf+len < (uintptr)buf)
		len = -(uintptr)buf-1;

	va_start(args, fmt);
	n = vsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}
