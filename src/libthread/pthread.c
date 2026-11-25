#include "threadimpl.h"

#undef exits
#undef _exits

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

/* note: _procsleep can have spurious wakeups, like pthread_cond_wait */
void
_procsleep(_Procrendez *r)
{
	/* r is protected by r->l, which we hold */
	pthread_cond_init(&r->cond, 0);
	r->asleep = 1;
	if(pthread_cond_wait(&r->cond, &r->l->mutex) != 0)
		sysfatal("pthread_cond_wait: %r");
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
_procwakeupandunlock(_Procrendez *r)
{
	if(r->asleep){
		r->asleep = 0;
		pthread_cond_signal(&r->cond);
	}
	unlock(r->l);
}

static void
startprocfn(void *v)
{
	void **a;
	void (*fn)(void*);
	Proc *p;

	a = (void**)v;
	fn = (void(*)(void*))a[0];
	p = a[1];
	free(a);
	p->osprocid = pthread_self();
	pthread_detach(p->osprocid);

	(*fn)(p);

	pthread_exit(0);
}

static void
startpthreadfn(void *v)
{
	void **a;
	Proc *p;
	_Thread *t;

	a = (void**)v;
	p = a[0];
	t = a[1];
	free(a);
	t->osprocid = pthread_self();
	pthread_detach(t->osprocid);
	_threadpthreadmain(p, t);
	pthread_exit(0);
}

static void
pthreadcreate(pthread_t *id, void (*fn)(void*), void *a)
{
	sigset_t all, old;

	sigfillset(&all);
	pthread_sigmask(SIG_BLOCK, &all, &old);
	if(pthread_create(id, nil, (void*(*)(void*))fn, a) < 0){
		fprint(2, "pthread_create: %r\n");
		abort();
	}
	pthread_sigmask(SIG_SETMASK, &old, nil);
}

void
_procstart(Proc *p, void (*fn)(Proc*))
{
	void **a;

	a = malloc(2*sizeof a[0]);
	if(a == nil)
		sysfatal("_procstart malloc: %r");
	a[0] = (void*)fn;
	a[1] = p;

	pthreadcreate(&p->osprocid, startprocfn, a);
}

void
_threadpthreadstart(Proc *p, _Thread *t)
{
	void **a;

	a = malloc(3*sizeof a[0]);
	if(a == nil)
		sysfatal("_pthreadstart malloc: %r");
	a[0] = p;
	a[1] = t;
	pthreadcreate(&t->osprocid, startpthreadfn, a);
}

static pthread_key_t prockey;
static sigset_t procmask;

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
	sigset_t all;

	if(p == nil){
		sigfillset(&all);
		pthread_sigmask(SIG_BLOCK, &all, nil);
		pthread_setspecific(prockey, nil);
		return;
	}
	pthread_setspecific(prockey, p);
	pthread_sigmask(SIG_SETMASK, &procmask, nil);
}

void
_pthreadinit(void)
{
	pthread_key_create(&prockey, 0);
	pthread_sigmask(SIG_SETMASK, nil, &procmask);
}

void
threadexitsall(char *msg)
{
	exits(msg);
}

void
_threadpexit(void)
{
	pthread_exit(0);
}
