#include "threadimpl.h"

int	_threaddebuglevel;

static	uint		threadnproc;
static	uint		threadnsysproc;
static	Lock		threadnproclock;
static	Ref		threadidref;
static	Proc		*threadmainproc;

static	void		addproc(Proc*);
static	void		delproc(Proc*);
static	void		addthread(_Threadlist*, _Thread*);
static	void		delthread(_Threadlist*, _Thread*);
static	void		addthreadinproc(Proc*, _Thread*);
static	void		delthreadinproc(Proc*, _Thread*);
static	void		contextswitch(Context *from, Context *to);
static	void		scheduler(Proc*);

static void
_threaddebug(char *fmt, ...)
{
	va_list arg;
	char buf[128];
	_Thread *t;
	char *p;
	static int fd = -1;

return;
	va_start(arg, fmt);
	vfprint(1, fmt, arg);
	va_end(arg);
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
	}

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	t = proc()->thread;
	if(t)
		fprint(fd, "%d.%d: %s\n", getpid(), t->id, buf);
	else
		fprint(fd, "%d._: %s\n", getpid(), buf);
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

static void
threadstart(uint y, uint x)
{
	_Thread *t;
	ulong z;

	z = x<<16;	/* hide undefined 32-bit shift from 32-bit compilers */
	z <<= 16;
	z |= y;
	t = (_Thread*)z;

//print("threadstart %p\n", v);
	t->startfn(t->startarg);
//print("threadexits %p\n", v);
	threadexits(nil);
//print("not reacehd\n");
}

static _Thread*
threadalloc(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;
	sigset_t zero;
	uint x, y;
	ulong z;

	/* allocate the task and stack together */
	t = malloc(sizeof *t+stack);
	if(t == nil)
		sysfatal("threadalloc malloc: %r");
	memset(t, 0, sizeof *t);
	t->stk = (uchar*)(t+1);
	t->stksize = stack;
	t->id = incref(&threadidref);
	t->startfn = fn;
	t->startarg = arg;

	/* do a reasonable initialization */
	memset(&t->context.uc, 0, sizeof t->context.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

	/* must initialize with current context */
	if(getcontext(&t->context.uc) < 0)
		sysfatal("threadalloc getcontext: %r");

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	t->context.uc.uc_stack.ss_sp = t->stk+8;
	t->context.uc.uc_stack.ss_size = t->stksize-64;
#ifdef __sun__		/* sigh */
	/* can avoid this with __MAKECONTEXT_V2_SOURCE but only on SunOS 5.9 */
	t->context.uc.uc_stack.ss_sp = 
		(char*)t->context.uc.uc_stack.ss_sp
		+t->context.uc.uc_stack.ss_size;
#endif
	/*
	 * All this magic is because you have to pass makecontext a
	 * function that takes some number of word-sized variables,
	 * and on 64-bit machines pointers are bigger than words.
	 */
	z = (ulong)t;
	y = z;
	z >>= 16;	/* hide undefined 32-bit shift from 32-bit compilers */
	x = z>>16;
	makecontext(&t->context.uc, (void(*)())threadstart, 2, y, x);

	return t;
}

_Thread*
_threadcreate(Proc *p, void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	t = threadalloc(fn, arg, stack);
	t->proc = p;
	addthreadinproc(p, t);
	p->nthread++;
	_threadready(t);
	return t;
}

int
threadcreate(void (*fn)(void*), void *arg, uint stack)
{
	_Thread *t;

	t = _threadcreate(proc(), fn, arg, stack);
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
	_procstart(p, scheduler);
	return id;
}

void
_threadswitch(void)
{
	Proc *p;

	needstack(0);
	p = proc();
//print("threadswtch %p\n", p);
	contextswitch(&p->thread->context, &p->schedcontext);
}

void
_threadready(_Thread *t)
{
	Proc *p;

	p = t->proc;
	lock(&p->lock);
	p->runrend.l = &p->lock;
	addthread(&p->runqueue, t);
//print("%d wake for job %d->%d\n", time(0), getpid(), p->osprocid);
	if(p != proc())
		_procwakeupandunlock(&p->runrend);
	else
		unlock(&p->lock);
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

static void
contextswitch(Context *from, Context *to)
{
	if(swapcontext(&from->uc, &to->uc) < 0){
		fprint(2, "swapcontext failed: %r\n");
		assert(0);
	}
}

static void
scheduler(Proc *p)
{
	_Thread *t;

	setproc(p);
	_threaddebug("scheduler enter");
//	print("s %p\n", p);
	lock(&p->lock);
	for(;;){
		while((t = p->runqueue.head) == nil){
			if(p->nthread == 0)
				goto Out;
			p->runrend.l = &p->lock;
			_threaddebug("scheduler sleep");
			_procsleep(&p->runrend);
			_threaddebug("scheduler wake");
		}
		delthread(&p->runqueue, t);
		unlock(&p->lock);
		p->thread = t;
		p->nswitch++;
		_threaddebug("run %d (%s)", t->id, t->name);
		contextswitch(&p->schedcontext, &t->context);
//print("back in scheduler\n");
		p->thread = nil;
		lock(&p->lock);
		if(t->exiting){
			delthreadinproc(p, t);
			p->nthread--;
//print("ntrhead %d\n", p->nthread);
			free(t);
		}
	}

Out:
	_threaddebug("scheduler exit");
	delproc(p);
	lock(&threadnproclock);
	if(p->sysproc)
		--threadnsysproc;
	if(--threadnproc == threadnsysproc)
		threadexitsall(p->msg);
	unlock(&threadnproclock);
	unlock(&p->lock);
	free(p);
	setproc(0);
}

void
_threadsetsysproc(void)
{
	lock(&threadnproclock);
	if(++threadnsysproc == threadnproc)
		exit(0);
	unlock(&threadnproclock);
	proc()->sysproc = 1;
}

void**
procdata(void)
{
	return &proc()->udata;
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

void
needstack(int n)
{
	_Thread *t;

	t = proc()->thread;

	if((char*)&t <= (char*)t->stk
	|| (char*)&t - (char*)t->stk < 256+n){
		fprint(2, "thread stack overflow\n");
		abort();
	}
}

/*
 * locking
 */
static int
threadqlock(QLock *l, int block, ulong pc)
{
//print("threadqlock %p\n", l);
	lock(&l->l);
	if(l->owner == nil){
		l->owner = (*threadnow)();
//print("qlock %p @%#x by %p\n", l, pc, l->owner);
		unlock(&l->l);
		return 1;
	}
	if(!block){
		unlock(&l->l);
		return 0;
	}
//print("qsleep %p @%#x by %p\n", l, pc, (*threadnow)());
	addthread(&l->waiting, (*threadnow)());
	unlock(&l->l);

	_threadswitch();

	if(l->owner != (*threadnow)()){
		fprint(2, "%s: qlock pc=0x%lux owner=%p self=%p oops\n",
			argv0, pc, l->owner, (*threadnow)());
		abort();
	}
//print("qlock wakeup %p @%#x by %p\n", l, pc, (*threadnow)());
	return 1;
}

static void
threadqunlock(QLock *l, ulong pc)
{
	lock(&l->l);
//print("qlock unlock %p @%#x by %p (owner %p)\n", l, pc, (*threadnow)(), l->owner);
	if(l->owner == 0){
		fprint(2, "%s: qunlock pc=0x%lux owner=%p self=%p oops\n",
			argv0, pc, l->owner, (*threadnow)());
		abort();
	}
	if((l->owner = l->waiting.head) != nil){
		delthread(&l->waiting, l->owner);
		_threadready(l->owner);
	}
	unlock(&l->l);
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
	lock(&l->l);
	--l->readers;
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
		_threadready(t);
	}
	unlock(&l->l);
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
	if(l->readers == 0 && (t = l->wwaiting.head) != nil){
		delthread(&l->wwaiting, t);
		l->writer = t;
		_threadready(t);
	}
	unlock(&l->l);
}

/*
 * sleep and wakeup
 */
static void
threadrsleep(Rendez *r, ulong pc)
{
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

	for(i=0;; i++){
		if(i==1 && !all)
			break;
		if((t = r->waiting.head) == nil)
			break;
		delthread(&r->waiting, t);
		_threadready(t);	
	}
	return i;
}

/*
 * startup
 */

static int threadargc;
static char **threadargv;
int mainstacksize;

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
	threadmainproc = proc();
	threadmain(threadargc, threadargv);
}

int
main(int argc, char **argv)
{
	Proc *p;

	argv0 = argv[0];

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

	_pthreadinit();
	p = procalloc();
	_threadsetproc(p);
	if(mainstacksize == 0)
		mainstacksize = 65536;
	_threadcreate(p, threadmainstart, nil, mainstacksize);
	scheduler(p);
	_threaddaemonize();
	_threadpexit();
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
