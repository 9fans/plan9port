#include <u.h>
#include <libc.h>

static RWLock fmtlock;

void
__fmtrlock(void)
{
	rlock(&fmtlock);
}

void
__fmtrunlock(void)
{
	runlock(&fmtlock);
}

void
__fmtwlock(void)
{
	wlock(&fmtlock);
}

void
__fmtwunlock(void)
{
	wunlock(&fmtlock);
}
