#include "u.h"
#include <errno.h>
#include "libc.h"
#include "thread.h"
#include "threadimpl.h"

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

int
_threadlock(Lock *lk, int block, ulong pc)
{
	int r;

	if(!lk->init)
		lockinit(lk);
	if(block){
		if(pthread_mutex_lock(&lk->mutex) != 0)
			abort();
		return 1;
	}else{
		r = pthread_mutex_trylock(&lk->mutex);
		if(r == 0)
			return 1;
		if(r == EBUSY)
			return 0;
		abort();
		return 0;
	}
}

void
_threadunlock(Lock *lk, ulong pc)
{
	if(pthread_mutex_unlock(&lk->mutex) != 0)
		abort();
}

void
_procsleep(_Procrendez *r)
{
	/* r is protected by r->l, which we hold */
	pthread_cond_init(&r->cond, 0);
	r->asleep = 1;
	pthread_cond_wait(&r->cond, &r->l->mutex);
	pthread_cond_destroy(&r->cond);
	r->asleep = 0;
}

void
_procwakeup(_Procrendez *r)
{
	if(r->asleep){
		r->asleep = 0;
		pthread_cond_signal(&r->cond);
	}
}

void
_procstart(Proc *p, void (*fn)(void*))
{
//print("pc\n");
	if(pthread_create(&p->tid, nil, (void*(*)(void*))fn, p) < 0){
//print("pc1\n");
		fprint(2, "pthread_create: %r\n");
		abort();
	}
//print("pc2\n");
}

static pthread_key_t prockey;

Proc*
_threadproc(void)
{
	Proc *p;

	p = pthread_getspecific(prockey);
	return p;
}

void
_threadsetproc(Proc *p)
{
	pthread_setspecific(prockey, p);
}

void
_pthreadinit(void)
{
	pthread_key_create(&prockey, 0);
}

