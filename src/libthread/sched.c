#include <u.h>
#include <signal.h>
#include <errno.h>
#include "threadimpl.h"

//static Thread	*runthread(Proc*);

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

void
_schedinit(void *arg)
{
	Proc *p;
	Thread *t;
	extern void ignusr1(int), _threaddie(int);
	ignusr1(1);
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
		_exits(_threadexitsallstatus);
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
/*
		if(p->needexec){
			t->ret = _schedexec(&p->exec);
			p->needexec = 0;
		}
*/
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
	Channel *c;
	Thread *t;
	Tqueue *q;
	Waitmsg *w;
	int e, sent;

	if(p->nthreads==0 || (p->nthreads==1 && p->idle))
		return nil;
	q = &p->ready;
relock:
	lock(&p->readylock);
	if(p->nsched%128 == 0){
		/* clean up children */
		e = errno;
		if((c = _threadwaitchan) != nil){
			if(c->n <= c->s){
				sent = 0;
				for(;;){
					if((w = p->waitmsg) != nil)
						p->waitmsg = nil;
					else
						w = waitnohang();
					if(w == nil)
						break;
					if(sent == 0){
						unlock(&p->readylock);
						sent = 1;
					}
					if(nbsendp(c, w) != 1)
						break;
				}
				p->waitmsg = w;
				if(sent)
					goto relock;
			}
		}else{
			while((w = waitnohang()) != nil)
				free(w);
		}
		errno = e;
	}
	if(q->head == nil){
		if(p->idle){
			if(p->idle->state != Ready){
				fprint(2, "everyone is asleep\n");
				exits("everyone is asleep");
			}
			unlock(&p->readylock);
			_threaddebug(DBGSCHED, "running idle thread", p->nthreads);
			return p->idle;
		}

		_threaddebug(DBGSCHED, "sleeping for more work (%d threads)", p->nthreads);
		q->asleep = 1;
		unlock(&p->readylock);
		while(rendezvous((ulong)q, 0) == ~0){
			if(_threadexitsallstatus)
				_exits(_threadexitsallstatus);
		}
		/* lock picked up from _threadready */
	}
	t = q->head;
	q->head = t->next;
	unlock(&p->readylock);
	return t;
}

void
needstack(int howmuch)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	if(p == nil || (t=p->thread) == nil)
		return;
	if((ulong)&howmuch < (ulong)t->stk+howmuch){	/* stack overflow waiting to happen */
		fprint(2, "stack overflow: stack at 0x%lux, limit at 0x%lux, need 0x%lux\n", (ulong)&p, (ulong)t->stk, howmuch);
		abort();
	}
}

int
_sched(void)
{
	Proc *p;
	Thread *t;

Resched:
	p = _threadgetproc();
//fprint(2, "p %p\n", p);
	if((t = p->thread) != nil){
		needstack(512);
	//	_threaddebug(DBGSCHED, "pausing, state=%s set %p goto %p",
	//		psstate(t->state), &t->sched, &p->sched);
		if(_setlabel(&t->sched)==0)
			_gotolabel(&p->sched);
		_threadstacklimit(t->stk, t->stk+t->stksize);
		return p->nsched++;
	}else{
		t = runthread(p);
		if(t == nil){
			_threaddebug(DBGSCHED, "all threads gone; exiting");
			_threaddelproc();
			_schedexit(p);
		}
		_threaddebug(DBGSCHED, "running %d.%d", t->proc->pid, t->id);
		p->thread = t;
		if(t->moribund){
			_threaddebug(DBGSCHED, "%d.%d marked to die");
			goto Resched;
		}
		t->state = Running;
		t->nextstate = Ready;
		_gotolabel(&t->sched);
		for(;;);
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

	if(t == t->proc->idle){
		_threaddebug(DBGSCHED, "idle thread is ready");
		return;
	}

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
				_exits(_threadexitsallstatus);
		}
	}else
		unlock(&t->proc->readylock);
}

void
_threadidle(void)
{
	Tqueue *q;
	Thread *t, *idle;
	Proc *p;

	p = _threadgetproc();
	q = &p->ready;
	lock(&p->readylock);
	assert(q->tail);
	idle = q->tail;
	if(q->head == idle){
		q->head = nil;
		q->tail = nil;
	}else{
		for(t=q->head; t->next!=q->tail; t=t->next)
			;
		t->next = nil;
		q->tail = t;
	}
	p->idle = idle;
	_threaddebug(DBGSCHED, "p->idle is %d\n", idle->id);
	unlock(&p->readylock);
}

int
yield(void)
{
	Proc *p;
	int nsched;

	p = _threadgetproc();
	nsched = p->nsched;
	return _sched() - nsched;
}

void
threadstatus(void)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	for(t=p->threads.head; t; t=t->nextt)
		fprint(2, "[%3d] %s userpc=%lux\n",
			t->id, psstate(t->state), t->userpc);
}
	
