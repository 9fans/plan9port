#include <u.h>
#include <libc.h>

static Lock fmtlock;

void
__fmtlock(void)
{
	lock(&fmtlock);
}

void
__fmtunlock(void)
{
	unlock(&fmtlock);
}
