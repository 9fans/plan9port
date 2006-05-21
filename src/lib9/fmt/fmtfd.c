/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
#include <stdarg.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"

/*
 * public routine for final flush of a formatting buffer
 * to a file descriptor; returns total char count.
 */
int
fmtfdflush(Fmt *f)
{
	if(__fmtFdFlush(f) <= 0)
		return -1;
	return f->nfmt;
}

/*
 * initialize an output buffer for buffered printing
 */
int
fmtfdinit(Fmt *f, int fd, char *buf, int size)
{
	f->runes = 0;
	f->start = buf;
	f->to = buf;
	f->stop = buf + size;
	f->flush = __fmtFdFlush;
	f->farg = (void*)(uintptr_t)fd;
	f->flags = 0;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}
