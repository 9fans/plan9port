#include <u.h>
#include <unistd.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>
#include <libc.h>

static pthread_mutex_t initmutex = PTHREAD_MUTEX_INITIALIZER;

static void
lockinit(Lock *lk)
{
	pthread_mutexattr_t attr;

	pthread_mutex_lock(&initmutex);
	if(lk->init == 0){
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&lk->mutex, &attr);
		pthread_mutexattr_destroy(&attr);
		lk->init = 1;
	}
	pthread_mutex_unlock(&initmutex);
}

void
lock(Lock *lk)
{
	if(!lk->init)
		lockinit(lk);
	if(pthread_mutex_lock(&lk->mutex) != 0)
		abort();
}

int
canlock(Lock *lk)
{
	int r;

	if(!lk->init)
		lockinit(lk);
	r = pthread_mutex_trylock(&lk->mutex);
	if(r == 0)
		return 1;
	if(r == EBUSY)
		return 0;
	abort();
}

void
unlock(Lock *lk)
{
	if(pthread_mutex_unlock(&lk->mutex) != 0)
		abort();
}
