#include "threadimpl.h"

Rgrp _threadrgrp;
static int isdirty;
int _threadhighnrendez;
int _threadnrendez;
static int nrendez;

static ulong
finish(Thread *t, ulong val)
{
	ulong ret;

	ret = t->rendval;
	t->rendval = val;
	while(t->state == Running)
		sleep(0);
	lock(&t->proc->lock);
	if(t->state == Rendezvous){	/* not always true: might be Dead */
		t->state = Ready;
		_threadready(t);
	}
	unlock(&t->proc->lock);
	return ret;
}

ulong
_threadrendezvous(ulong tag, ulong val)
{
	ulong ret;
	Thread *t, **l;

	lock(&_threadrgrp.lock);
_threadnrendez++;
	l = &_threadrgrp.hash[tag%nelem(_threadrgrp.hash)];
	for(t=*l; t; l=&t->rendhash, t=*l){
		if(t->rendtag==tag){
			_threaddebug(DBGREND, "Rendezvous with thread %d.%d", t->proc->pid, t->id);
			*l = t->rendhash;
			ret = finish(t, val);
			--nrendez;
			unlock(&_threadrgrp.lock);
			return ret;
		}
	}

	/* Going to sleep here. */
	t = _threadgetproc()->thread;
	t->rendbreak = 0;
	t->inrendez = 1;
	t->rendtag = tag;
	t->rendval = val;
	t->rendhash = *l;
	*l = t;
	++nrendez;
	if(nrendez > _threadhighnrendez)
		_threadhighnrendez = nrendez;
	_threaddebug(DBGREND, "Rendezvous for tag %lud (m=%d)", t->rendtag, t->moribund);
	unlock(&_threadrgrp.lock);
	t->nextstate = Rendezvous;
	_sched();
	t->inrendez = 0;
	_threaddebug(DBGREND, "Woke after rendezvous; val is %lud", t->rendval);
	return t->rendval;
}

/*
 * This is called while holding _threadpq.lock and p->lock,
 * so we can't lock _threadrgrp.lock.  Instead our caller has 
 * to call _threadbreakrendez after dropping those locks.
 */
void
_threadflagrendez(Thread *t)
{
	t->rendbreak = 1;
	isdirty = 1;
}

void
_threadbreakrendez(void)
{
	int i;
	Thread *t, **l;

	if(isdirty == 0)
		return;
	lock(&_threadrgrp.lock);
	if(isdirty == 0){
		unlock(&_threadrgrp.lock);
		return;
	}
	isdirty = 0;
	for(i=0; i<nelem(_threadrgrp.hash); i++){
		l = &_threadrgrp.hash[i];
		for(t=*l; t; t=*l){
			if(t->rendbreak){
				*l = t->rendhash;
				finish(t, ~0);
			}else
				 l=&t->rendhash;
		}
	}
	unlock(&_threadrgrp.lock);
}
