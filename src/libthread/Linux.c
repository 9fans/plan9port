#include "threadimpl.h"

#undef exits
#undef _exits

static int
timefmt(Fmt *fmt)
{
	static char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	vlong ns;
	Tm tm;
	ns = nsec();
	tm = *localtime(time(0));
	return fmtprint(fmt, "%s %2d %02d:%02d:%02d.%03d", 
		mon[tm.mon], tm.mday, tm.hour, tm.min, tm.sec,
		(int)(ns%1000000000)/1000000);
}

/*
 * spin locks
 */
extern int _tas(int*);

void
_threadunlock(Lock *l, ulong pc)
{
	USED(pc);

	l->held = 0;
}

int
_threadlock(Lock *l, int block, ulong pc)
{
	int i;
static int first=1;
if(first) {first=0; fmtinstall('\001', timefmt);}

	USED(pc);

	/* once fast */
	if(!_tas(&l->held))
		return 1;
	if(!block)
		return 0;

	/* a thousand times pretty fast */
	for(i=0; i<1000; i++){
		if(!_tas(&l->held))
			return 1;
		sched_yield();
	}
	/* now increasingly slow */
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(1);
	}
fprint(2, "%\001 %s: lock loop1 %p from %lux\n", argv0, l, pc);
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(10);
	}
fprint(2, "%\001 %s: lock loop2 %p from %lux\n", argv0, l, pc);
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(100);
	}
fprint(2, "%\001 %s: lock loop3 %p from %lux\n", argv0, l, pc);
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(1000);
	}
fprint(2, "%\001 %s: lock loop4 %p from %lux\n", argv0, l, pc);
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(10*1000);
	}
fprint(2, "%\001 %s: lock loop5 %p from %lux\n", argv0, l, pc);
	for(i=0; i<1000; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(100*1000);
	}
fprint(2, "%\001 %s: lock loop6 %p from %lux\n", argv0, l, pc);
	/* take your time */
	while(_tas(&l->held))
		usleep(1000*1000);
	return 1;
}

/*
 * sleep and wakeup
 */
static void
ign(int x)
{
	USED(x);
}

static void /*__attribute__((constructor))*/
ignusr1(int restart)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = ign;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGUSR1);
	if(restart)
		sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, nil);
}

void
_procsleep(_Procrendez *r)
{
	sigset_t mask;

	/*
	 * Go to sleep.
	 *
	 * Block USR1, set the handler to interrupt system calls,
	 * unlock the vouslock so our waker can wake us,
	 * and then suspend.
	 */
again:
	r->asleep = 1;
	r->pid = getpid();

	sigprocmask(SIG_SETMASK, nil, &mask);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_SETMASK, &mask, nil);
	ignusr1(0);
	unlock(r->l);
	sigdelset(&mask, SIGUSR1);
	sigsuspend(&mask);

	/*
	 * We're awake.  Make USR1 not interrupt system calls.
	 */
	lock(r->l);
	ignusr1(1);
	if(r->asleep && r->pid == getpid()){
		/* Didn't really wake up - signal from something else */
		goto again;
	}
}

void
_procwakeupandunlock(_Procrendez *r)
{
	int pid;

	pid = 0;
	if(r->asleep){
		r->asleep = 0;
		assert(r->pid >= 1);
		pid = r->pid;
	}
	assert(r->l);
	unlock(r->l);
	if(pid)
		kill(pid, SIGUSR1);
}

/*
 * process creation and exit
 */
typedef struct Stackfree Stackfree;
struct Stackfree
{
	Stackfree	*next;
	int	pid;
	int	pid1;
};
static Lock stacklock;
static Stackfree *stackfree;

static void
delayfreestack(uchar *stk, int pid, int pid1)
{
	Stackfree *sf;

	sf = (Stackfree*)stk;
	sf->pid = pid;
	sf->pid1 = pid1;
	lock(&stacklock);
	sf->next = stackfree;
	stackfree = sf;
	unlock(&stacklock);
}

static void
dofreestacks(void)
{
	Stackfree *sf, *last, *next;

	if(stackfree==nil || !canlock(&stacklock))
		return;

	for(last=nil,sf=stackfree; sf; last=sf,sf=next){
		next = sf->next;
		if(sf->pid >= 1 && kill(sf->pid, 0) < 0 && errno == ESRCH)
		if(sf->pid1 >= 1 && kill(sf->pid1, 0) < 0 && errno == ESRCH){
			free(sf);
			if(last)
				last->next = next;
			else
				stackfree = next;
			sf = last;
		}
	}
	unlock(&stacklock);
}

static int
startprocfn(void *v)
{
	void **a;
	uchar *stk;
	void (*fn)(void*);
	Proc *p;
	int pid0, pid1;

	a = (void**)v;
	fn = a[0];
	p = a[1];
	stk = a[2];
	pid0 = (int)a[4];
	pid1 = getpid();
	free(a);
	p->osprocid = pid1;

	(*fn)(p);

	delayfreestack(stk, pid0, pid1);
	_exit(0);
	return 0;
}

/*
 * indirect through here so that parent need not wait for child zombie
 * 
 * slight race - if child exits and then another process starts before we 
 * manage to exit, we'll be running on a freed stack.
 */
static int
trampnowait(void *v)
{
	void **a;
	int *kidpid;

	a = (void*)v;
	kidpid = a[3];
	a[4] = (void*)getpid();
	*kidpid = clone(startprocfn, a[2]+65536-512, CLONE_VM|CLONE_FILES, a);
	_exit(0);
	return 0;
}

void
_procstart(Proc *p, void (*fn)(Proc*))
{
	void **a;
	uchar *stk;
	int pid, kidpid, status;

	dofreestacks();
	a = malloc(5*sizeof a[0]);
	if(a == nil)
		sysfatal("_procstart malloc: %r");
	stk = malloc(65536);
	if(stk == nil)
		sysfatal("_procstart malloc stack: %r");

	a[0] = fn;
	a[1] = p;
	a[2] = stk;
	a[3] = &kidpid;
	kidpid = -1;

	pid = clone(trampnowait, stk+65536-16, CLONE_VM|CLONE_FILES, a);
	if(pid > 0)
		if(wait4(pid, &status, __WALL, 0) < 0)
			fprint(2, "ffork wait4: %r\n");
	if(pid < 0 || kidpid < 0){
		fprint(2, "_procstart clone: %r\n");
		abort();
	}
}

static char *threadexitsmsg;
void
sigusr2handler(int s)
{
/*	fprint(2, "%d usr2 %d\n", time(0), getpid()); */
	if(threadexitsmsg)
		_exits(threadexitsmsg);
}

void
threadexitsall(char *msg)
{
	static int pid[1024];
	int i, npid, mypid;
	Proc *p;

	if(msg == nil)
		msg = "";

	/* 
	 * Only one guy, ever, gets to run this.
	 * If two guys do it, inevitably they end up
	 * tripping over each other in the underlying
	 * C library exit() implementation, which is
	 * trying to run the atexit handlers and apparently
	 * not thread safe.  This has been observed on
	 * both Linux and OpenBSD.  Sigh.
	 */
	{
		static Lock onelock;
		if(!canlock(&onelock))
			_exits(threadexitsmsg);
		threadexitsmsg = msg;
	}

	mypid = getpid();
	lock(&_threadprocslock);
	npid = 0;
	for(p=_threadprocs; p; p=p->next)
		if(p->osprocid != mypid && p->osprocid >= 1)
			pid[npid++] = p->osprocid;
	for(i=0; i<npid; i++)
		kill(pid[i], SIGUSR2);
	unlock(&_threadprocslock);
	exits(msg);
}

/*
 * per-process data, indexed by pid
 *
 * could use modify_ldt and a segment register
 * to avoid the many calls to getpid(), but i don't
 * care -- this is compatibility code.  linux 2.6 with
 * nptl is a good enough pthreads to avoid this whole file.
 */
typedef struct Perproc Perproc;
struct Perproc
{
	int		pid;
	Proc	*proc;
};

static Lock perlock;
static Perproc perproc[1024];
#define P ((Proc*)-1)

static Perproc*
myperproc(void)
{
	int i, pid, h;
	Perproc *p;

	pid = getpid();
	h = pid%nelem(perproc);
	for(i=0; i<nelem(perproc); i++){
		p = &perproc[(i+h)%nelem(perproc)];
		if(p->pid == pid)
			return p;
		if(p->pid == 0){
			print("found 0 at %d (h=%d)\n", (i+h)%nelem(perproc), h);
			break;
		}
	}
	fprint(2, "myperproc %d (%s): cannot find self\n", pid, argv0);
	abort();
	return nil;
}

static Perproc*
newperproc(void)
{
	int i, pid, h;
	Perproc *p;

	lock(&perlock);
	pid = getpid();
	h = pid%nelem(perproc);
	for(i=0; i<nelem(perproc); i++){
		p = &perproc[(i+h)%nelem(perproc)];
		if(p->pid == pid || p->pid == -1 || p->pid == 0){
			p->pid = pid;
			unlock(&perlock);
			return p;
		}
	}
	fprint(2, "newperproc %d: out of procs\n", pid);
	abort();
	return nil;
}

Proc*
_threadproc(void)
{
	return myperproc()->proc;
}

void
_threadsetproc(Proc *p)
{
	Perproc *pp;

	if(p)
		p->osprocid = getpid();
	pp = newperproc();
	pp->proc = p;
	if(p == nil)
		pp->pid = -1;
}

void
_pthreadinit(void)
{
	signal(SIGUSR2, sigusr2handler);
}

void
_threadpexit(void)
{
	_exit(0);
}

#ifdef __arm__
void
makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	int i, *sp;
	va_list arg;
	
	sp = (int*)uc->uc_stack.ss_sp+uc->uc_stack.ss_size/4;
	va_start(arg, argc);
	for(i=0; i<4 && i<argc; i++)
		uc->uc_mcontext.gregs[i] = va_arg(arg, uint);
	va_end(arg);
	uc->uc_mcontext.gregs[13] = (uint)sp;
	uc->uc_mcontext.gregs[14] = (uint)fn;
}

int
swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}
#endif

