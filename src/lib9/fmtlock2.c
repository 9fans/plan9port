#include <u.h>
#include <libc.h>

static QLock fmtlock;

void
__fmtlock(void)
{
	qlock(&fmtlock);
}

void
__fmtunlock(void)
{
	qunlock(&fmtlock);
}
