#include "threadimpl.h"

#include "BSD.c"

static spinlock_t mlock = { 0, 0, NULL, 0 };

void
_thread_malloc_lock(void)
{
	_spinlock(&mlock);
}

void
_thread_malloc_unlock(void)
{
	_spinunlock(&mlock);
}

void
_thread_malloc_init(void)
{
}
