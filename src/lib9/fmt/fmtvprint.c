/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"


/*
 * format a string into the output buffer
 * designed for formats which themselves call fmt,
 * but ignore any width flags
 */
int
fmtvprint(Fmt *f, char *fmt, va_list args)
{
	va_list va;
	int n;

	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_COPY(va,f->args);
	VA_END(f->args);
	VA_COPY(f->args,args);
	n = dofmt(f, fmt);
	f->flags = 0;
	f->width = 0;
	f->prec = 0;
	VA_END(f->args);
	VA_COPY(f->args,va);
	VA_END(va);
	if(n >= 0)
		return 0;
	return n;
}

