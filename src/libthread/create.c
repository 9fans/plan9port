#include "threadimpl.h"

Pqueue _threadpq;	/* list of all procs */
int _threadnprocs;	/* count of procs */

static int newthreadid(void);
static int newprocid(void);

/*
 * Create and initialize a new Thread structure attached to a given proc.
 */
int
_newthread(Proc *p, void (*f)(void *arg), void *arg, uint stacksize, 
	char *name, int grp)
{
	int id;
	Thread *t;

	t = _threadmalloc(sizeof(Thread), 1);
	t->proc = p;
	t->grp = grp;
	t->id = id = newthreadid();
	if(name)
		t->name = strdup(name);
	_threaddebug(DBGSCHED, "create thread %d.%d name %s", p->id, id, name);

	/*
	 * Allocate and clear stack.
	 */
	if(stacksize < 1024)
		sysfatal("bad stacksize %d", stacksize);
	t->stk = _threadmalloc(stacksize, 0);
	t->stksize = stacksize;
	_threaddebugmemset(t->stk, 0xFE, stacksize);

	/*
	 * Set up t->context to call f(arg).
	 */
	_threadinitstack(t, f, arg);

	/*
	 * Add thread to proc.
	 */
	lock(&p->lock);
	p->nthreads++;
	if(p->threads.head == nil)
		p->threads.head = t;
	else{
		t->prevt = p->threads.tail;
		t->prevt->nextt = t;
	}
	p->threads.tail = t;
	t->next = (Thread*)~0;

	/*
	 * Mark thread as ready to run.
	 */
	t->state = Ready;
	_threadready(t);
	unlock(&p->lock);

	return id;
}

/* 
 * Free a Thread structure.
 */
void
_threadfree(Thread *t)
{
	free(t->stk);
	free(t->name);
	free(t);
}

/*
 * Create and initialize a new Proc structure with a single Thread
 * running inside it.  Add the Proc to the global process list.
 */
Proc*
_newproc(void)
{
	Proc *p;

	/*
	 * Allocate.
	 */
	p = _threadmalloc(sizeof *p, 1);
	p->id = newprocid();
	
	/*
	 * Add to list.  Record if we're now multiprocess.
	 */
	lock(&_threadpq.lock);
	if(_threadpq.head == nil)
		_threadpq.head = p;
	else
		*_threadpq.tail = p;
	_threadpq.tail = &p->next;
	if(_threadnprocs == 1)
		_threadmultiproc();
	_threadnprocs++;
	unlock(&_threadpq.lock);

	return p;
}

/*
 * Allocate a new thread running f(arg) on a stack of size stacksize.
 * Return the thread id.  The thread group inherits from the current thread.
 */
int
threadcreate(void (*f)(void*), void *arg, uint stacksize)
{
	return _newthread(_threadgetproc(), f, arg, stacksize, nil, threadgetgrp());
}

/* 
 * Allocate a new idle thread.  Only allowed in a single-proc program.
 */
int
threadcreateidle(void (*f)(void *arg), void *arg, uint stacksize)
{
	int id;

	assert(_threadnprocs == 1);

	id = threadcreate(f, arg, stacksize);
	_threaddebug(DBGSCHED, "idle is %d", id);
	_threadsetidle(id);
	return id;
}

/*
 * Threadcreate, but do it inside a fresh proc.
 */
int
proccreate(void (*f)(void*), void *arg, uint stacksize)
{
	int id;
	Proc *p, *np;

	p = _threadgetproc();
	np = _newproc();
	p->newproc = np;
	p->schedfn = _threadstartproc;
	id = _newthread(np, f, arg, stacksize, nil, p->thread->grp);
	_sched();	/* call into scheduler to create proc XXX */
	return id;
}

/*
 * Allocate a new thread id.
 */
static int
newthreadid(void)
{
	static Lock l;
	static int id;
	int i;

	lock(&l);
	i = ++id;
	unlock(&l);
	return i;
}

/*
 * Allocate a new proc id.
 */
static int
newprocid(void)
{
	static Lock l;
	static int id;
	int i;

	lock(&l);
	i = ++id;
	unlock(&l);
	return i;
}

