#include "threadimpl.h"

Pqueue _threadpq;

static int nextID(void);

/*
 * Create and initialize a new Thread structure attached to a given proc.
 */

typedef struct Stack Stack;
struct Stack {
	ulong magic;
	Thread *thr;
	Stack *next;
	uchar buf[STKSIZE-12];
};

static Stack *stkfree;
static Lock stklock;

void
_stackfree(void *v)
{
	Stack *s;

	s = v;
	lock(&stklock);
	s->thr = nil;
	s->magic = 0;
	s->next = stkfree;
	stkfree = s;
	unlock(&stklock);
}

static Stack*
stackalloc(void)
{
	char *buf;
	Stack *s;
	int i;

	lock(&stklock);
	while(stkfree == nil){
		unlock(&stklock);
		assert(STKSIZE == sizeof(Stack));
		buf = malloc(STKSIZE+128*STKSIZE);
		s = (Stack*)(((ulong)buf+STKSIZE)&~(STKSIZE-1));
		for(i=0; i<128; i++)
			_stackfree(&s[i]);
		lock(&stklock);
	}
	s = stkfree;
	stkfree = stkfree->next;
	unlock(&stklock);
	s->magic = STKMAGIC;
	return s;
}

static int
newthread(Proc *p, void (*f)(void *arg), void *arg, uint stacksize, char *name, int grp)
{
	int id;
	Thread *t;
	Stack *s;

	if(stacksize < 32)
		sysfatal("bad stacksize %d", stacksize);
	t = _threadmalloc(sizeof(Thread), 1);
	s = stackalloc();
	s->thr = t;
	t->stk = (char*)s;
	t->stksize = STKSIZE;
	_threaddebugmemset(s->buf, 0xFE, sizeof s->buf);
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
		_stackfree((Stack*)t->stk);
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

