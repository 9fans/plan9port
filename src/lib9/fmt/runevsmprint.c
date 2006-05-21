/* Copyright (c) 2002-2006 Lucent Technologies; see LICENSE */
/*
 * Plan 9 port version must include libc.h in order to 
 * get Plan 9 debugging malloc, which sometimes returns
 * different pointers than the standard malloc. 
 */
#ifdef PLAN9PORT
#include <u.h>
#include <libc.h>
#include "fmtdef.h"
#else
#include <stdlib.h>
#include <string.h>
#include "plan9.h"
#include "fmt.h"
#include "fmtdef.h"
#endif

static int
runeFmtStrFlush(Fmt *f)
{
	Rune *s;
	int n;

	if(f->start == nil)
		return 0;
	n = (uintptr)f->farg;
	n *= 2;
	s = (Rune*)f->start;
	f->start = realloc(s, sizeof(Rune)*n);
	if(f->start == nil){
		f->farg = nil;
		f->to = nil;
		f->stop = nil;
		free(s);
		return 0;
	}
	f->farg = (void*)(uintptr)n;
	f->to = (Rune*)f->start + ((Rune*)f->to - s);
	f->stop = (Rune*)f->start + n - 1;
	return 1;
}

int
runefmtstrinit(Fmt *f)
{
	int n;

	memset(f, 0, sizeof *f);
	f->runes = 1;
	n = 32;
	f->start = malloc(sizeof(Rune)*n);
	if(f->start == nil)
		return -1;
	f->to = f->start;
	f->stop = (Rune*)f->start + n - 1;
	f->flush = runeFmtStrFlush;
	f->farg = (void*)(uintptr)n;
	f->nfmt = 0;
	fmtlocaleinit(f, nil, nil, nil);
	return 0;
}

/*
 * print into an allocated string buffer
 */
Rune*
runevsmprint(char *fmt, va_list args)
{
	Fmt f;
	int n;

	if(runefmtstrinit(&f) < 0)
		return nil;
	VA_COPY(f.args,args);
	n = dofmt(&f, fmt);
	VA_END(f.args);
	if(f.start == nil)
		return nil;
	if(n < 0){
		free(f.start);
		return nil;
	}
	*(Rune*)f.to = '\0';
	return (Rune*)f.start;
}
