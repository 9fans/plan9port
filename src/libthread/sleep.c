#include "threadimpl.h"

int _threadhighnrendez;
int _threadnrendez;

void
_threadsleep(_Procrend *r)
{
	Thread *t;

	t = _threadgetproc()->thread;
	r->arg = t;
	t->nextstate = Rendezvous;
	t->asleep = 1;
	unlock(r->l);
	_sched();
	t->asleep = 0;
	lock(r->l);
}

void
_threadwakeup(_Procrend *r)
{
	Thread *t;

	t = r->arg;
	while(t->state == Running)
		sleep(0);
	lock(&t->proc->lock);
	if(t->state == Dead){
		unlock(&t->proc->lock);
		return;
	}
	assert(t->state == Rendezvous && t->asleep);
	t->state = Ready;
	_threadready(t);
	unlock(&t->proc->lock);
}

