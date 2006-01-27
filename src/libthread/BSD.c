#undef exits
#undef _exits

extern int __isthreaded;

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
		sleep(0);
	}
	/* increasingly slow */
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(1);
	}
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(10);
	}
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(100);
	}
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(1000);
	}
	for(i=0; i<10; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(10*1000);
	}
	/* now nice and slow */
	for(i=0; i<1000; i++){
		if(!_tas(&l->held))
			return 1;
		usleep(100*1000);
	}
	/* take your time */
	while(_tas(&l->held))
		usleep(1000*1000);
	return 1;
}

/*
 * For libc.
 */

typedef struct {
	volatile long	access_lock;
	volatile long	lock_owner;
	volatile char	*fname;
	volatile int	lineno;
} spinlock_t;

void
_spinlock(spinlock_t *lk)
{
	lock((Lock*)&lk->access_lock);
}

void
_spinunlock(spinlock_t *lk)
{
	unlock((Lock*)&lk->access_lock);
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
_procwakeup(_Procrendez *r)
{
	if(r->asleep){
		r->asleep = 0;
		assert(r->pid >= 1);
		kill(r->pid, SIGUSR1);
	}
}

void
_procwakeupandunlock(_Procrendez *r)
{
	_procwakeup(r);
	unlock(r->l);
}


/*
 * process creation and exit
 */
typedef struct Stackfree Stackfree;
struct Stackfree
{
	Stackfree	*next;
	int	pid;
};
static Lock stacklock;
static Stackfree *stackfree;

static void
delayfreestack(uchar *stk)
{
	Stackfree *sf;

	sf = (Stackfree*)stk;
	sf->pid = getpid();
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
		if(sf->pid >= 1 && kill(sf->pid, 0) < 0 && errno == ESRCH){
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

	a = (void**)v;
	fn = a[0];
	p = a[1];
	stk = a[2];
	free(a);
	p->osprocid = getpid();

	(*fn)(p);

	delayfreestack(stk);
	_exit(0);
	return 0;
}

void
_procstart(Proc *p, void (*fn)(Proc*))
{
	void **a;
	uchar *stk;
	int pid;

	dofreestacks();
	a = malloc(3*sizeof a[0]);
	if(a == nil)
		sysfatal("_procstart malloc: %r");
	stk = malloc(65536);
	if(stk == nil)
		sysfatal("_procstart malloc stack: %r");

	a[0] = fn;
	a[1] = p;
	a[2] = stk;

	pid = rfork_thread(RFPROC|RFMEM|RFNOWAIT, stk+65536-64, startprocfn, a);
	if(pid < 0){
		fprint(2, "_procstart rfork_thread: %r\n");
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

	if(msg == nil)
		msg = "";
	mypid = getpid();
	lock(&_threadprocslock);
	threadexitsmsg = msg;
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
	fprint(2, "myperproc %d: cannot find self\n", pid);
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
_threadpexit(void)
{
	_exit(0);
}

