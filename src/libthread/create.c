#include "threadimpl.h"

Pqueue _threadpq;

int _threadmultiproc;

static int nextID(void);

/*
 * Create and initialize a new Thread structure attached to a given proc.
 */
void
_stackfree(void *v)
{
	free(v);
}

static int
newthread(Proc *p, void (*f)(void *arg), void *arg, uint stacksize, char *name, int grp)
{
	int id;
	Thread *t;
	char *s;

	if(stacksize < 32)
		sysfatal("bad stacksize %d", stacksize);
	t = _threadmalloc(sizeof(Thread), 1);
	s = _threadmalloc(stacksize, 0);
	t->stk = (char*)s;
	t->stksize = stacksize;
	_threaddebugmemset(s, 0xFE, stacksize);
	_threadinitstack(t, f, arg);
	t->proc = p;
	t->grp = grp;
	if(name)
		t->cmdname = strdup(name);
	t->id = nextID();
	id = t->id;
	t->next = (Thread*)~0;
	_threaddebug(DBGSCHED, "create thread %d.%d name %s", p->pid, t->id, name);
	lock(&p->lock);
	p->nthreads++;
	if(p->threads.head == nil)
		p->threads.head = t;
	else{
		t->prevt = p->threads.tail;
		t->prevt->nextt = t;
	}
	p->threads.tail = t;
	t->state = Ready;
	_threadready(t);
	unlock(&p->lock);
	return id;
}

static int
nextID(void)
{
	static Lock l;
	static int id;
	int i;

	lock(&l);
	i = ++id;
	unlock(&l);
	return i;
}
	
int
procrfork(void (*f)(void *), void *arg, uint stacksize, int rforkflag)
{
	Proc *p;
	int id;

	p = _threadgetproc();
	assert(p->newproc == nil);
	p->newproc = _newproc(f, arg, stacksize, nil, p->thread->grp, rforkflag);
	id = p->newproc->threads.head->id;
	_sched();
	return id;
}

int
proccreate(void (*f)(void*), void *arg, uint stacksize)
{
	Proc *p;

	p = _threadgetproc();
	if(p->idle){
		werrstr("cannot create procs once there is an idle thread");
		return -1;
	}
	_threadmultiproc = 1;
	return procrfork(f, arg, stacksize, 0);
}

void
_freeproc(Proc *p)
{
	Thread *t, *nextt;

	for(t = p->threads.head; t; t = nextt){
		if(t->cmdname)
			free(t->cmdname);
		assert(t->stk != nil);
		_stackfree(t->stk);
		nextt = t->nextt;
		free(t);
	}
	free(p);
}

/* 
 * Create a new thread and schedule it to run.
 * The thread grp is inherited from the currently running thread.
 */
int
threadcreate(void (*f)(void *arg), void *arg, uint stacksize)
{
	return newthread(_threadgetproc(), f, arg, stacksize, nil, threadgetgrp());
}

int
threadcreateidle(void (*f)(void *arg), void *arg, uint stacksize)
{
	int id;

	if(_threadmultiproc){
		werrstr("cannot have idle thread in multi-proc program");
		return -1;
	}
	id = newthread(_threadgetproc(), f, arg, stacksize, nil, threadgetgrp());
	_threadidle();
	return id;
}

/*
 * Create and initialize a new Proc structure with a single Thread
 * running inside it.  Add the Proc to the global process list.
 */
Proc*
_newproc(void (*f)(void *arg), void *arg, uint stacksize, char *name, int grp, int rforkflag)
{
	Proc *p;

	p = _threadmalloc(sizeof *p, 1);
	p->pid = -1;
	p->rforkflag = rforkflag;
	newthread(p, f, arg, stacksize, name, grp);

	lock(&_threadpq.lock);
	if(_threadpq.head == nil)
		_threadpq.head = p;
	else
		*_threadpq.tail = p;
	_threadpq.tail = &p->next;
	unlock(&_threadpq.lock);
	return p;
}

