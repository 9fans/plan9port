#include <signal.h>
#include "threadimpl.h"

static void tinterrupt(Proc*, Thread*);

static void
threadxxxgrp(int grp, int dokill)
{
	Proc *p;
	Thread *t;

	lock(&_threadpq.lock);
	for(p=_threadpq.head; p; p=p->next){
		lock(&p->lock);
		for(t=p->threads.head; t; t=t->nextt)
			if(t->grp == grp){
				if(dokill)
					t->moribund = 1;
				tinterrupt(p, t);
			}
		unlock(&p->lock);
	}
	unlock(&_threadpq.lock);
	_threadbreakrendez();
}

static void
threadxxx(int id, int dokill)
{
	Proc *p;
	Thread *t;

	lock(&_threadpq.lock);
	for(p=_threadpq.head; p; p=p->next){
		lock(&p->lock);
		for(t=p->threads.head; t; t=t->nextt)
			if(t->id == id){
				if(dokill)
					t->moribund = 1;
				tinterrupt(p, t);
				unlock(&p->lock);
				unlock(&_threadpq.lock);
				_threadbreakrendez();
				return;
			}
		unlock(&p->lock);
	}
	unlock(&_threadpq.lock);
	_threaddebug(DBGNOTE, "Can't find thread to kill");
	return;
}

void
threadkillgrp(int grp)
{
	threadxxxgrp(grp, 1);
}

void
threadkill(int id)
{
	threadxxx(id, 1);
}

void
threadintgrp(int grp)
{
	threadxxxgrp(grp, 0);
}

void
threadint(int id)
{
	threadxxx(id, 0);
}

static void
tinterrupt(Proc *p, Thread *t)
{
	switch(t->state){
	case Running:
		kill(p->pid, SIGINT);
	//	postnote(PNPROC, p->pid, "threadint");
		break;
	case Rendezvous:
		_threadflagrendez(t);
		break;
	}
}
