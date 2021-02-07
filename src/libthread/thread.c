#include "threadimpl.h"

int	_threaddebuglevel = 0;

static	uint		threadnproc;
static	uint		threadnsysproc;
static	Lock		threadnproclock;
static	Ref		threadidref;
static	Proc		*threadmainproc;

static	void		addproc(Proc*);
static	void		delproc(Proc*);
static	void		addthread(_Threadlist*, _Thread*);
static	void		delthread(_Threadlist*, _Thread*);
static	int		onlist(_Threadlist*, _Thread*);
static	void		addthreadinproc(Proc*, _Thread*);
static	void		delthreadinproc(Proc*, _Thread*);
static	void		procmain(Proc*);
static	int		threadinfo(void*, char*);
static 	void		pthreadscheduler(Proc *p);
static	void		pthreadsleepschedlocked(Proc *p, _Thread *t);
static	void		pthreadwakeupschedlocked(Proc *p, _Thread *self, _Thread *t);
static	_Thread*	procnext(Proc*, _Thread*);

static void
_threaddebug(_Thread *t, char *fmt, ...)
{
	va_list arg;
	char buf[128];
	char *p;
	static int fd = -1;

	if(_threaddebuglevel == 0)
		return;

	if(fd < 0){
		p = strrchr(argv0, '/');
		if(p)
			p++;
		else
			p = argv0;
		snprint(buf, sizeof buf, "/tmp/%s.tlog", p);
		if((fd = create(buf, OWRITE, 0666)) < 0)
			fd = open("/dev/null", OWRITE);
		if(fd >= 0 && fd != 2){
			dup(fd, 2);
			close(fd);
			fd = 2;
		}
	}

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	if(t == nil)
		t = proc()->thread;
	if(t)
		fprint(fd, "%p %d.%d: %s\n", proc(), getpid(), t->id, buf);
	else
		fprint(fd, "%p %d._: %s\n", proc(), getpid(), buf);
}

static _Thread*
getthreadnow(void)
{
	return proc()->thread;
}
_Thread	*(*threadnow)(void) = getthreadnow;

static Proc*
procalloc(void)
{
	Proc *p;

	p = malloc(sizeof *p);
	if(p == nil)
		sysfatal("procalloc malloc: %r");
	memset(p, 0, sizeof *p);
	addproc(p);
	lock(&threadnproclock);
	threadnproc++;
	unlock(&threadnproclock);
	return p;
}

_Thread*
_threadcreate(Proc *p, void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	USED(stack);
	t = malloc(sizeof *t);
	if(t == nil)
		sysfatal("threadcreate malloc: %r");
	memset(t, 0, sizeof *t);
	t->id = incref(&threadidref);
	t->startfn = fn;
	t->startarg = arg;
	t->proc = p;
	if(p->nthread != 0)
		_threadpthreadstart(p, t);
	else
		t->mainthread = 1;
	p->nthread++;
	addthreadinproc(p, t);
	_threadready(t);
	return t;
}

int
threadcreate(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	t = _threadcreate(proc(), fn, arg, stack);
	_threaddebug(nil, "threadcreate %d", t->id);
	return t->id;
}

int
proccreate(void (*fn)(void*), void *arg, uint stack)
{
	int id;
	_Thread *t;
	Proc *p;

	p = procalloc();
	t = _threadcreate(p, fn, arg, stack);
	id = t->id;	/* t might be freed after _procstart */
	_threaddebug(t, "proccreate %p", p);
	_procstart(p, procmain);
	return id;
}

void
_threadswitch(void)
{
	Proc *p;

	needstack(0);
	p = proc();
/*print("threadswtch %p\n", p); */
	pthreadscheduler(p);
}

void
_threadready(_Thread *t)
{
	Proc *p;

	p = t->proc;
	lock(&p->lock);
	p->runrend.l = &p->lock;
	addthread(&p->runqueue, t);
/*print("%d wake for job %d->%d\n", time(0), getpid(), p->osprocid); */
	if(p != proc())
		_procwakeupandunlock(&p->runrend);
	else
		unlock(&p->lock);
}

int
threadidle(void)
{
	int n;
	Proc *p;

	p = proc();
	n = p->nswitch;
	lock(&p->lock);
	p->runrend.l = &p->lock;
	addthread(&p->idlequeue, p->thread);
	unlock(&p->lock);
	_threadswitch();
	return p->nswitch - n;
}

int
threadyield(void)
{
	int n;
	Proc *p;

	p = proc();
	n = p->nswitch;
	_threadready(p->thread);
	_threadswitch();
	return p->nswitch - n;
}

void
threadexits(char *msg)
{
	Proc *p;

	p = proc();
	if(msg == nil)
		msg = "";
	utfecpy(p->msg, p->msg+sizeof p->msg, msg);
	proc()->thread->exiting = 1;
	_threadswitch();
}

void
threadpin(void)
{
	Proc *p;

	p = proc();
	if(p->pinthread){
		fprint(2, "already pinning a thread - %p %p\n", p->pinthread, p->thread);
		assert(0);
	}
	p->pinthread = p->thread;
}

void
threadunpin(void)
{
	Proc *p;

	p = proc();
	if(p->pinthread != p->thread){
		fprint(2, "wrong pinthread - %p %p\n", p->pinthread, p->thread);
		assert(0);
	}
	p->pinthread = nil;
}

void
threadsysfatal(char *fmt, va_list arg)
{
	char buf[256];

	vseprint(buf, buf+sizeof(buf), fmt, arg);
	__fixargv0();
	fprint(2, "%s: %s\n", argv0 ? argv0 : "<prog>", buf);
	threadexitsall(buf);
}

static void
procmain(Proc *p)
{
	_Thread *t;

	_threadsetproc(p);

	/* take out first thread to run on system stack */
	t = p->runqueue.head;
	delthread(&p->runqueue, t);

	/* run it */
	p->thread = t;
	t->startfn(t->startarg);
	if(p->nthread != 0)
		threadexits(nil);
}

void
_threadpthreadmain(Proc *p, _Thread *t)
{
	_threadsetproc(p);
	lock(&p->lock);
	pthreadsleepschedlocked(p, t);
	unlock(&p->lock);
	_threaddebug(nil, "startfn");
	t->startfn(t->startarg);
	threadexits(nil);
}

static void
pthreadsleepschedlocked(Proc *p, _Thread *t)
{
	_threaddebug(t, "pthreadsleepsched %p %d", p, t->id);;
	t->schedrend.l = &p->lock;
	while(p->schedthread != t)
		_procsleep(&t->schedrend);
}

static void
pthreadwakeupschedlocked(Proc *p, _Thread *self, _Thread *t)
{
	_threaddebug(self, "pthreadwakeupschedlocked %p %d", p, t->id);;
	t->schedrend.l = &p->lock;
	p->schedthread = t;
	_procwakeup(&t->schedrend);
}

static void
pthreadscheduler(Proc *p)
{
	_Thread *self, *t;

	_threaddebug(nil, "scheduler");
	lock(&p->lock);
	self = p->thread;
	p->thread = nil;
	_threaddebug(self, "pausing");

	if(self->exiting) {
		_threaddebug(self, "exiting");
		delthreadinproc(p, self);
		p->nthread--;
	}

	t = procnext(p, self);
	if(t != nil) {
		pthreadwakeupschedlocked(p, self, t);
		if(!self->exiting) {
			pthreadsleepschedlocked(p, self);
			_threaddebug(nil, "resume %d", self->id);
			unlock(&p->lock);
			return;
		}
	}

	if(t == nil) {
		/* Tear down proc bookkeeping. Wait to free p. */
		delproc(p);
		lock(&threadnproclock);
		if(p->sysproc)
			--threadnsysproc;
		if(--threadnproc == threadnsysproc)
			threadexitsall(p->msg);
		unlock(&threadnproclock);
	}

	/* Tear down pthread. */
	if(self->mainthread && p->mainproc) {
		_threaddaemonize();
		_threaddebug(self, "sleeper");
		unlock(&p->lock);
		/*
		 * Avoid bugs with main pthread exiting.
		 * When all procs are gone, threadexitsall above will happen.
		 */
		for(;;)
			sleep(60*60*1000);
	}
	_threadsetproc(nil);
	free(self);
	unlock(&p->lock);
	if(t == nil)
		free(p);
	_threadpexit();
}

static _Thread*
procnext(Proc *p, _Thread *self)
{
	_Thread *t;

	if((t = p->pinthread) != nil){
		while(!onlist(&p->runqueue, t)){
			p->runrend.l = &p->lock;
			_threaddebug(self, "scheduler sleep (pin)");
			_procsleep(&p->runrend);
			_threaddebug(self, "scheduler wake (pin)");
		}
	} else
	while((t = p->runqueue.head) == nil){
		if(p->nthread == 0)
			return nil;
		if((t = p->idlequeue.head) != nil){
			/*
			 * Run all the idling threads once.
			 */
			while((t = p->idlequeue.head) != nil){
				delthread(&p->idlequeue, t);
				addthread(&p->runqueue, t);
			}
			continue;
		}
		p->runrend.l = &p->lock;
		_threaddebug(self, "scheduler sleep");
		_procsleep(&p->runrend);
		_threaddebug(self, "scheduler wake");
	}

	if(p->pinthread && p->pinthread != t)
		fprint(2, "p->pinthread %p t %p\n", p->pinthread, t);
	assert(p->pinthread == nil || p->pinthread == t);
	delthread(&p->runqueue, t);

	p->thread = t;
	p->nswitch++;
	return t;
}

void
_threadsetsysproc(void)
{
	lock(&threadnproclock);
	if(++threadnsysproc == threadnproc)
		threadexitsall(nil);
	unlock(&threadnproclock);
	proc()->sysproc = 1;
}

void**
procdata(void)
{
	return &proc()->udata;
}

void**
threaddata(void)
{
	return &proc()->thread->udata;
}

extern Jmp *(*_notejmpbuf)(void);
static Jmp*
threadnotejmp(void)
{
	return &proc()->sigjmp;
}

/*
 * debugging
 */
void
threadsetname(char *fmt, ...)
{
	va_list arg;
	_Thread *t;

	t = proc()->thread;
	va_start(arg, fmt);
	vsnprint(t->name, sizeof t->name, fmt, arg);
	va_end(arg);
}

char*
threadgetname(void)
{
	return proc()->thread->name;
}

void
threadsetstate(char *fmt, ...)
{
	va_list arg;
	_Thread *t;

	t = proc()->thread;
	va_start(arg, fmt);
	vsnprint(t->state, sizeof t->name, fmt, arg);
	va_end(arg);
}

int
threadid(void)
{
	_Thread *t;

	t = proc()->thread;
	return t->id;
}

void
needstack(int n)
{
	_Thread *t;

	t = proc()->thread;
	if(t->stk == nil)
		return;

	if((char*)&t <= (char*)t->stk
	|| (char*)&t - (char*)t->stk < 256+n){
		fprint(2, "thread stack overflow: &t=%p tstk=%p n=%d\n", &t, t->stk, 256+n);
		abort();
	}
}

static int
singlethreaded(void)
{
	return threadnproc == 1 && _threadprocs->nthread == 1;
}

/*
 * locking
 */
static int
threadqlock(QLock *l, int block, ulong pc)
{
/*print("threadqlock %p\n", l); */
	lock(&l->l);
	if(l->owner == nil){
		l->owner = (*threadnow)();
/*print("qlock %p @%#x by %p\n", l, pc, l->owner); */
		if(l->owner == nil) {
			fprint(2, "%s: qlock uncontended owner=nil oops\n", argv0);
			abort();
		}
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}

	if(singlethreaded()){
		fprint(2, "qlock deadlock\n");
		abort();
	}

/*print("qsleep %p @%#x by %p\n", l, pc, (*threadnow)()); */
	addthread(&l->waiting, (*threadnow)());
	unlock(&l->l);

	_threadswitch();

	if(l->owner != (*threadnow)()){
		fprint(2, "%s: qlock pc=0x%lux owner=%p self=%p oops\n",
			argv0, pc, l->owner, (*threadnow)());
		abort();
	}
	if(l->owner == nil) {
		fprint(2, "%s: qlock threadswitch owner=nil oops\n", argv0);
		abort();
	}

/*print("qlock wakeup %p @%#x by %p\n", l, pc, (*threadnow)()); */
	return 1;
}

static void
threadqunlock(QLock *l, ulong pc)
{
	_Thread *ready;

	lock(&l->l);
/*print("qlock unlock %p @%#x by %p (owner %p)\n", l, pc, (*threadnow)(), l->owner); */
	if(l->owner == 0){
		fprint(2, "%s: qunlock pc=0x%lux owner=%p self=%p oops\n",
			argv0, pc, l->owner, (*threadnow)());
		abort();
	}
	if((l->owner = ready = l->waiting.head) != nil)
		delthread(&l->waiting, l->owner);
	/*
	 * N.B. Cannot call _threadready() before unlocking l->l,
	 * because the thread we are readying might:
	 *	- be in another proc
	 *	- start running immediately
	 *	- and free l before we get a chance to run again
	 */
	unlock(&l->l);
	if(ready)
		_threadready(l->owner);
}

static int
threadrlock(RWLock *l, int block, ulong pc)
{
	USED(pc);

	lock(&l->l);
	if(l->writer == nil && l->wwaiting.head == nil){
		l->readers++;
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
	if(singlethreaded()){
		fprint(2, "rlock deadlock\n");
		abort();
	}
	addthread(&l->rwaiting, (*threadnow)());
	unlock(&l->l);
	_threadswitch();
	return 1;
}

static int
threadwlock(RWLock *l, int block, ulong pc)
{
	USED(pc);

	lock(&l->l);
	if(l->writer == nil && l->readers == 0){
		l->writer = (*threadnow)();
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
	if(singlethreaded()){
		fprint(2, "wlock deadlock\n");
		abort();
	}
	addthread(&l->wwaiting, (*threadnow)());
	unlock(&l->l);
	_threadswitch();
	return 1;
}

static void
threadrunlock(RWLock *l, ulong pc)
{
	_Thread *t;

	USED(pc);
	t = nil;
	lock(&l->l);
	--l->readers;
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
	}
	unlock(&l->l);
	if(t)
		_threadready(t);

}

static void
threadwunlock(RWLock *l, ulong pc)
{
	_Thread *t;

	USED(pc);
	lock(&l->l);
	l->writer = nil;
	assert(l->readers == 0);
	while((t = l->rwaiting.head) != nil){
		delthread(&l->rwaiting, t);
		l->readers++;
		_threadready(t);
	}
	t = nil;
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
	}
	unlock(&l->l);
	if(t)
		_threadready(t);
}

/*
 * sleep and wakeup
 */
static void
threadrsleep(Rendez *r, ulong pc)
{
	if(singlethreaded()){
		fprint(2, "rsleep deadlock\n");
		abort();
	}
	addthread(&r->waiting, proc()->thread);
	qunlock(r->l);
	_threadswitch();
	qlock(r->l);
}

static int
threadrwakeup(Rendez *r, int all, ulong pc)
{
	int i;
	_Thread *t;

	_threaddebug(nil, "rwakeup %p %d", r, all);
	for(i=0;; i++){
		if(i==1 && !all)
			break;
		if((t = r->waiting.head) == nil)
			break;
		_threaddebug(nil, "rwakeup %p %d -> wake %d", r, all, t->id);
		delthread(&r->waiting, t);
		_threadready(t);
		_threaddebug(nil, "rwakeup %p %d -> loop", r, all);
	}
	_threaddebug(nil, "rwakeup %p %d -> total %d", r, all, i);
	return i;
}

/*
 * startup
 */

static int threadargc;
static char **threadargv;
int mainstacksize;
extern int _p9usepwlibrary;	/* getgrgid etc. smash the stack - tell _p9dir just say no */
static void
threadmainstart(void *v)
{
	USED(v);

	/*
	 * N.B. This call to proc() is a program's first call (indirectly) to a
	 * pthreads function while executing on a non-pthreads-allocated
	 * stack.  If the pthreads implementation is using the stack pointer
	 * to locate the per-thread data, then this call will blow up.
	 * This means the pthread implementation is not suitable for
	 * running under libthread.  Time to write your own.  Sorry.
	 */
	_p9usepwlibrary = 0;
	threadmainproc = proc();
	threadmain(threadargc, threadargv);
}

extern void (*_sysfatal)(char*, va_list);

int
main(int argc, char **argv)
{
	Proc *p;
	_Thread *t;
	char *opts;

	argv0 = argv[0];

	opts = getenv("LIBTHREAD");
	if(opts == nil)
		opts = "";

	if(threadmaybackground() && strstr(opts, "nodaemon") == nil && getenv("NOLIBTHREADDAEMONIZE") == nil)
		_threadsetupdaemonize();

	threadargc = argc;
	threadargv = argv;

	/*
	 * Install locking routines into C library.
	 */
	_lock = _threadlock;
	_unlock = _threadunlock;
	_qlock = threadqlock;
	_qunlock = threadqunlock;
	_rlock = threadrlock;
	_runlock = threadrunlock;
	_wlock = threadwlock;
	_wunlock = threadwunlock;
	_rsleep = threadrsleep;
	_rwakeup = threadrwakeup;
	_notejmpbuf = threadnotejmp;
	_pin = threadpin;
	_unpin = threadunpin;
	_sysfatal = threadsysfatal;

	_pthreadinit();
	p = procalloc();
	p->mainproc = 1;
	_threadsetproc(p);
	if(mainstacksize == 0)
		mainstacksize = 256*1024;
	atnotify(threadinfo, 1);
	t = _threadcreate(p, threadmainstart, nil, mainstacksize);
	t->mainthread = 1;
	procmain(p);
	sysfatal("procmain returned in libthread");
	/* does not return */
	return 0;
}

/*
 * hooray for linked lists
 */
static void
addthread(_Threadlist *l, _Thread *t)
{
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = nil;
	}
	l->tail = t;
	t->next = nil;
}

static void
delthread(_Threadlist *l, _Thread *t)
{
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

/* inefficient but rarely used */
static int
onlist(_Threadlist *l, _Thread *t)
{
	_Thread *tt;

	for(tt = l->head; tt; tt=tt->next)
		if(tt == t)
			return 1;
	return 0;
}

static void
addthreadinproc(Proc *p, _Thread *t)
{
	_Threadlist *l;

	l = &p->allthreads;
	if(l->tail){
		l->tail->allnext = t;
		t->allprev = l->tail;
	}else{
		l->head = t;
		t->allprev = nil;
	}
	l->tail = t;
	t->allnext = nil;
}

static void
delthreadinproc(Proc *p, _Thread *t)
{
	_Threadlist *l;

	l = &p->allthreads;
	if(t->allprev)
		t->allprev->allnext = t->allnext;
	else
		l->head = t->allnext;
	if(t->allnext)
		t->allnext->allprev = t->allprev;
	else
		l->tail = t->allprev;
}

Proc *_threadprocs;
Lock _threadprocslock;
static Proc *_threadprocstail;

static void
addproc(Proc *p)
{
	lock(&_threadprocslock);
	if(_threadprocstail){
		_threadprocstail->next = p;
		p->prev = _threadprocstail;
	}else{
		_threadprocs = p;
		p->prev = nil;
	}
	_threadprocstail = p;
	p->next = nil;
	unlock(&_threadprocslock);
}

static void
delproc(Proc *p)
{
	lock(&_threadprocslock);
	if(p->prev)
		p->prev->next = p->next;
	else
		_threadprocs = p->next;
	if(p->next)
		p->next->prev = p->prev;
	else
		_threadprocstail = p->prev;
	unlock(&_threadprocslock);
}

/*
 * notify - for now just use the usual mechanisms
 */
void
threadnotify(int (*f)(void*, char*), int in)
{
	atnotify(f, in);
}

static int
onrunqueue(Proc *p, _Thread *t)
{
	_Thread *tt;

	for(tt=p->runqueue.head; tt; tt=tt->next)
		if(tt == t)
			return 1;
	return 0;
}

/*
 * print state - called from SIGINFO
 */
static int
threadinfo(void *v, char *s)
{
	Proc *p;
	_Thread *t;

	if(strcmp(s, "quit") != 0 && strcmp(s, "sys: status request") != 0)
		return 0;

	for(p=_threadprocs; p; p=p->next){
		fprint(2, "proc %p %s%s\n", (void*)p->osprocid, p->msg,
			p->sysproc ? " (sysproc)": "");
		for(t=p->allthreads.head; t; t=t->allnext){
			fprint(2, "\tthread %d %s: %s %s\n",
				t->id,
				t == p->thread ? "Running" :
				onrunqueue(p, t) ? "Ready" : "Sleeping",
				t->state, t->name);
		}
	}
	return 1;
}
