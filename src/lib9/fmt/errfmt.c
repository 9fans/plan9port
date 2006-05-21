/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

int
__errfmt(Fmt *f)
{
	char *s;

	s = strerror(errno);
	return fmtstrcpy(f, s);
}
