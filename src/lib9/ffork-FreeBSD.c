#include <lib9.h>

extern int __isthreaded;
int
ffork(int flags, void(*fn)(void*), void *arg)
{
	int pid;
	void *p;

	_p9uproc(0);
	__isthreaded = 1;
	p = malloc(16384);
	if(p == nil)
		return -1;
	memset(p, 0xFE, 16384);
	pid = rfork_thread(RFPROC|flags, (char*)p+16000, (int(*)(void*))fn, arg);
	if(pid == 0)
		_p9uproc(0);
	return pid;
}

/*
 * For FreeBSD libc.
 */

typedef struct {
	volatile long	access_lock;
	volatile long	lock_owner;
	volatile char	*fname;
	volatile int	lineno;
} spinlock_t;

void
_spinlock(spinlock_t *lk)
{
	lock((Lock*)&lk->access_lock);
}

int
getfforkid(void)
{
	return getpid();
}

