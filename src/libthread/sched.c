#include <signal.h>
#include "threadimpl.h"

//static Thread	*runthread(Proc*);

#if 0
static char *_psstate[] = {
	"Dead",
	"Running",
	"Ready",
	"Rendezvous",
};

static char*
psstate(int s)
{
	if(s < 0 || s >= nelem(_psstate))
		return "unknown";
	return _psstate[s];
}
#endif

void
_schedinit(void *arg)
{
	Proc *p;
	Thread *t;
	extern void ignusr1(void), _threaddie(int);
	ignusr1();
	signal(SIGTERM, _threaddie);
  
	p = arg;
	lock(&p->lock);
	p->pid = _threadgetpid();
	_threadsetproc(p);
	unlock(&p->lock);
	while(_setlabel(&p->sched))
		;
	_threaddebug(DBGSCHED, "top of schedinit, _threadexitsallstatus=%p", _threadexitsallstatus);
	if(_threadexitsallstatus)
		exits(_threadexitsallstatus);
	lock(&p->lock);
	if((t=p->thread) != nil){
		p->thread = nil;
		if(t->moribund){
			if(t->moribund != 1)
				fprint(2, "moribund %d\n", t->moribund);
			assert(t->moribund == 1);
			t->state = Dead;
			if(t->prevt)
				t->prevt->nextt = t->nextt;
			else
				p->threads.head = t->nextt;
			if(t->nextt)
				t->nextt->prevt = t->prevt;
			else
				p->threads.tail = t->prevt;
			unlock(&p->lock);
			if(t->inrendez){
				_threadflagrendez(t);
				_threadbreakrendez();
			}
			_stackfree(t->stk);
			free(t->cmdname);
			free(t);	/* XXX how do we know there are no references? */
			p->nthreads--;
			t = nil;
			_sched();
		}
		if(p->needexec){
			t->ret = _schedexec(&p->exec);
			p->needexec = 0;
		}
		if(p->newproc){
			t->ret = _schedfork(p->newproc);
			if(t->ret < 0){
//fprint(2, "_schedfork: %r\n");
				abort();
}
			p->newproc = nil;
		}
		t->state = t->nextstate;
		if(t->state == Ready)
			_threadready(t);
	}
	unlock(&p->lock);
	_sched();
}

static Thread*
runthread(Proc *p)
{
	Thread *t;
	Tqueue *q;

	if(p->nthreads==0 || (p->nthreads==1 && p->idle))
		return nil;
	q = &p->ready;
	lock(&p->readylock);
	if(q->head == nil){
		q->asleep = 1;
		if(p->idle){
			if(p->idle->state != Ready){
				fprint(2, "everyone is asleep\n");
				exits("everyone is asleep");
			}
			unlock(&p->readylock);
			return p->idle;
		}

		_threaddebug(DBGSCHED, "sleeping for more work (%d threads)", p->nthreads);
		unlock(&p->readylock);
		while(rendezvous((ulong)q, 0) == ~0){
			if(_threadexitsallstatus)
				exits(_threadexitsallstatus);
		}
		/* lock picked up from _threadready */
	}
	t = q->head;
	q->head = t->next;
	unlock(&p->readylock);
	return t;
}

void
_sched(void)
{
	Proc *p;
	Thread *t;

Resched:
	p = _threadgetproc();
//fprint(2, "p %p\n", p);
	if((t = p->thread) != nil){
		if((ulong)&p < (ulong)t->stk){	/* stack overflow */
			fprint(2, "stack overflow %lux %lux\n", (ulong)&p, (ulong)t->stk);
			abort();
		}
	//	_threaddebug(DBGSCHED, "pausing, state=%s set %p goto %p",
	//		psstate(t->state), &t->sched, &p->sched);
		if(_setlabel(&t->sched)==0)
			_gotolabel(&p->sched);
		return;
	}else{
		t = runthread(p);
		if(t == nil){
			_threaddebug(DBGSCHED, "all threads gone; exiting");
			_threaddelproc();
			_schedexit(p);
		}
	//	_threaddebug(DBGSCHED, "running %d.%d", t->proc->pid, t->id);
		p->thread = t;
		if(t->moribund){
			_threaddebug(DBGSCHED, "%d.%d marked to die");
			goto Resched;
		}
		t->state = Running;
		t->nextstate = Ready;
		_gotolabel(&t->sched);
	}
}

long
threadstack(void)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	t = p->thread;
	return (ulong)&p - (ulong)t->stk;
}

void
_threadready(Thread *t)
{
	Tqueue *q;

	if(t == t->proc->idle)
		return;

	assert(t->state == Ready);
	_threaddebug(DBGSCHED, "readying %d.%d", t->proc->pid, t->id);
	q = &t->proc->ready;
	lock(&t->proc->readylock);
	t->next = nil;
	if(q->head==nil)
		q->head = t;
	else
		q->tail->next = t;
	q->tail = t;
	if(q->asleep){
		assert(q->asleep == 1);
		q->asleep = 0;
		/* lock passes to runthread */
		_threaddebug(DBGSCHED, "waking process %d", t->proc->pid);
		while(rendezvous((ulong)q, 0) == ~0){
			if(_threadexitsallstatus)
				exits(_threadexitsallstatus);
		}
	}else
		unlock(&t->proc->readylock);
}

void
_threadidle(void)
{
	Tqueue *q;
	Thread *t;
	Proc *p;

	p = _threadgetproc();
	q = &p->ready;
	lock(&p->readylock);
	assert(q->head);
	t = q->head;
	q->head = t->next;
	if(q->tail == t)
		q->tail = nil;
	p->idle = t;
	unlock(&p->readylock);
}

void
yield(void)
{
	_sched();
}

