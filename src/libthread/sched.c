/*
 * Thread scheduler.
 */
#include "threadimpl.h"

static Thread *runthread(Proc*);
static void schedexit(Proc*);

/*
 * Main scheduling loop.
 */
void
_threadscheduler(void *arg)
{
	Proc *p;
	Thread *t;

	p = arg;

	_threadlinkmain();
	_threadinitproc(p);

	for(;;){
		/* 
		 * Clean up zombie children.
		 */
		_threadwaitkids(p);

		/*
		 * Find next thread to run.
		 */
		_threaddebug(DBGSCHED, "runthread");
		t = runthread(p);
		if(t == nil)
			schedexit(p);
	
		/*
		 * If it's ready, run it (might instead be marked to die).
		 */
		lock(&p->lock);
		if(t->state == Ready){
			_threaddebug(DBGSCHED, "running %d.%d", p->id, t->id);
			t->state = Running;
			t->nextstate = Ready;
			p->thread = t;
			unlock(&p->lock);
			_swaplabel(&p->context, &t->context);
			lock(&p->lock);
			p->thread = nil;
		}

		/*
		 * If thread needs to die, kill it.
		 */
		if(t->moribund){
			_threaddebug(DBGSCHED, "moribund %d.%d", p->id, t->id);
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
			_threadfree(t);
			p->nthreads--;
			t = nil;
			continue;
		}
		unlock(&p->lock);

		/*
		 * If there is a request to run a function on the 
		 * scheduling stack, do so.
		 */
		if(p->schedfn){
			_threaddebug(DBGSCHED, "schedfn");
			p->schedfn(p);
			p->schedfn = nil;
			_threaddebug(DBGSCHED, "schedfn ended");
		}

		/*
		 * Move the thread along.
		 */
		t->state = t->nextstate;
		_threaddebug(DBGSCHED, "moveon %d.%d", p->id, t->id);
		if(t->state == Ready)
			_threadready(t);
	}
}

/*
 * Called by thread to give up control of processor to scheduler.
 */
int
_sched(void)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	t = p->thread;
	assert(t != nil);
	_swaplabel(&t->context, &p->context);
	return p->nsched++;
}

/*
 * Called by thread to yield the processor to other threads.
 * Returns number of other threads run between call and return.
 */
int
yield(void)
{
	Proc *p;
	int nsched;

	p = _threadgetproc();
	nsched = p->nsched;
	return _sched() - nsched;
}

/*
 * Choose the next thread to run.
 */
static Thread*
runthread(Proc *p)
{
	Thread *t;
	Tqueue *q;

	/*
	 * No threads left?
	 */
	if(p->nthreads==0 || (p->nthreads==1 && p->idle))
		return nil;

	lock(&p->readylock);
	q = &p->ready;
	if(q->head == nil){
		/*
		 * Is this a single-process program with an idle thread?
		 */
		if(p->idle){
			/*
			 * The idle thread had better be ready!
			 */
			if(p->idle->state != Ready)
				sysfatal("all threads are asleep");

			/*
			 * Run the idle thread.
			 */
			unlock(&p->readylock);
			_threaddebug(DBGSCHED, "running idle thread", p->nthreads);
			return p->idle;
		}

		/*
		 * Wait until one of our threads is readied (by another proc!).
		 */
		q->asleep = 1;
		p->rend.l = &p->readylock;
		_procsleep(&p->rend);

		/*
		 * Maybe we were awakened to exit?
		 */
		if(_threadexitsallstatus)
			_exits(_threadexitsallstatus);

		assert(q->head != nil);
	}

	t = q->head;
	q->head = t->next;
	unlock(&p->readylock);

	return t;
}

/*
 * Add a newly-ready thread to its proc's run queue.
 */
void
_threadready(Thread *t)
{
	Tqueue *q;

	/*
	 * The idle thread does not go on the run queue.
	 */
	if(t == t->proc->idle){
		_threaddebug(DBGSCHED, "idle thread is ready");
		return;
	}

	assert(t->state == Ready);
	_threaddebug(DBGSCHED, "readying %d.%d", t->proc->id, t->id);

	/*
	 * Add thread to run queue.
	 */
	q = &t->proc->ready;
	lock(&t->proc->readylock);

	t->next = nil;
	if(q->head == nil)
		q->head = t;
	else
		q->tail->next = t;
	q->tail = t;

	/*
	 * Wake proc scheduler if it is sleeping.
	 */
	if(q->asleep){
		assert(q->asleep == 1);
		q->asleep = 0;
		_procwakeup(&t->proc->rend);
	}
	unlock(&t->proc->readylock);
}

/*
 * Mark the given thread as the idle thread.
 * Since the idle thread was just created, it is sitting
 * somewhere on the ready queue.
 */
void
_threadsetidle(int id)
{
	Tqueue *q;
	Thread *t, **l, *last;
	Proc *p;

	p = _threadgetproc();

	lock(&p->readylock);

	/*
	 * Find thread on ready queue.
	 */
	q = &p->ready;
	for(l=&q->head, last=nil; (t=*l) != nil; l=&t->next, last=t)
		if(t->id == id)
			break;
	assert(t != nil);

	/* 
	 * Remove it from ready queue.
	 */
	*l = t->next;
	if(t == q->head)
		q->head = t->next;
	if(t->next == nil)
		q->tail = last;

	/*
	 * Set as idle thread.
	 */
	p->idle = t;
	_threaddebug(DBGSCHED, "p->idle is %d\n", t->id);
	unlock(&p->readylock);
}

static void
schedexit(Proc *p)
{
	char ex[ERRMAX];
	int n;
	Proc **l;

	_threaddebug(DBGSCHED, "exiting proc %d", p->id);
	lock(&_threadpq.lock);
	for(l=&_threadpq.head; *l; l=&(*l)->next){
		if(*l == p){
			*l = p->next;
			if(*l == nil)
				_threadpq.tail = l;
			break;
		}
	}
	n = --_threadnprocs;
	unlock(&_threadpq.lock);

	strncpy(ex, p->exitstr, sizeof ex);
	ex[sizeof ex-1] = '\0';
	free(p);
	if(n == 0)
		_threadexitallproc(ex);
	else
		_threadexitproc(ex);
}

