/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */
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
	 */
	if(buf+len < buf)
		len = -(uint)buf-1;

	va_start(args, fmt);
	n = vsnprint(buf, len, fmt, args);
	va_end(args);
	return n;
}
