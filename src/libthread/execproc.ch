/*
 * Set up a dedicated proc to handle calls to exec.
 * The proc also waits for child messages.  
 * This way, each proc scheduler need not worry
 * about calling wait in its main loop.
 * 
 * To be included from other files (e.g., Linux-clone.c).
 */

typedef struct Xarg Xarg;
struct Xarg
{
	Channel *pidc;
	int fd[3];
	char *prog;
	char **args;
	int freeargs;
	Channel *ret;
	int errn;
	char errstr[ERRMAX];
};

static Proc *_threadexecproc;
static Channel *_threadexecchan;
static Lock threadexeclock;

/*
 * Called to poll for any kids of this pthread.
 * We have a separate proc responsible for exec,
 * so this is a no-op.
 */
void
_threadwaitkids(Proc *p)
{
}

#define WAITSIG SIGCHLD

/*
 * Separate process to wait for child messages.
 * Also runs signal handlers and runs all execs.
 */
static void
nop(int sig)
{
	USED(sig);
}

static void
_threadwaitproc(void *v)
{
	Channel *c;
	Waitmsg *w;
	sigset_t mask;
	int ret, nkids;
	Xarg *xa;

	nkids = 0;

	sigemptyset(&mask);
	siginterrupt(WAITSIG, 1);
	signal(WAITSIG, nop);
	sigaddset(&mask, WAITSIG);
	sigprocmask(SIG_BLOCK, &mask, nil);
	USED(v);
	for(;;){
		while((nkids > 0 ? nbrecv : recv)(_threadexecchan, &xa) == 1){
			ret = _threadexec(xa->pidc, xa->fd, xa->prog, xa->args, xa->freeargs);
			if(ret > 0)
				nkids++;
			else{
				rerrstr(xa->errstr, sizeof xa->errstr);
				xa->errn = errno;
			}
			sendul(xa->ret, ret);
		}
		if(nkids > 0){
			sigprocmask(SIG_UNBLOCK, &mask, nil);
			w = wait();
			sigprocmask(SIG_BLOCK, &mask, nil);
			if(w == nil && errno == ECHILD){
				fprint(2, "wait returned ECHILD but nkids=%d; reset\n", nkids);
				nkids = 0;
			}
			if(w){
				nkids--;
				if((c = _threadwaitchan) != nil)
					sendp(c, w);
				else
					free(w);
			}
		}
	}
}

static void _kickexecproc(void);

int
_callthreadexec(Channel *pidc, int fd[3], char *prog, char *args[], int freeargs)
{
	int ret;
	Xarg xa;

	if(_threadexecchan == nil){
		lock(&threadexeclock);
		if(_threadexecchan == nil)
			_threadfirstexec();
		unlock(&threadexeclock);
	}

	xa.pidc = pidc;
	xa.fd[0] = fd[0];
	xa.fd[1] = fd[1];
	xa.fd[2] = fd[2];
	xa.prog = prog;
	xa.args = args;
	xa.freeargs = freeargs;
	xa.ret = chancreate(sizeof(ulong), 1);
	sendp(_threadexecchan, &xa);
	_kickexecproc();
	ret = recvul(xa.ret);
	if(ret < 0){
		werrstr("%s", xa.errstr);
		errno = xa.errn;
	}
	chanfree(xa.ret);
	return ret;
}

/* 
 * Called before the first exec.
 */
void
_threadfirstexec(void)
{
	int id;
	Proc *p;

	_threadexecchan = chancreate(sizeof(Xarg*), 1);
	id = proccreate(_threadwaitproc, nil, 32*1024);

	/*
	 * Sleazy: decrement threadnprocs so that 
	 * the existence of the _threadwaitproc proc
	 * doesn't keep us from exiting.
	 */
	lock(&_threadpq.lock);
	--_threadnprocs;
	for(p=_threadpq.head; p; p=p->next)
		if(p->threads.head && p->threads.head->id == id)
			break;
	if(p == nil)
		sysfatal("cannot find exec proc");
	unlock(&_threadpq.lock);
	_threadexecproc = p;
}

/*
 * Called after the thread t has been rescheduled.
 * Kick the exec proc in case it is in the middle of a wait.
 */
static void
_kickexecproc(void)
{
	kill(_threadexecproc->pid, WAITSIG);
}

/*
 * Called before exec.
 */
void
_threadbeforeexec(void)
{
}

/*
 * Called after exec.
 */
void
_threadafterexec(void)
{
}

