#include <u.h>
#include <errno.h>
#include "threadimpl.h"

/*
 * Basic kernel thread management.
 */
static pthread_key_t key;

void
_kthreadinit(void)
{
	pthread_key_create(&key, 0);
}

void
_kthreadsetproc(Proc *p)
{
	sigset_t all;

	p->pthreadid = pthread_self();
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, nil);
	pthread_setspecific(key, p);
}

Proc*
_kthreadgetproc(void)
{
	return pthread_getspecific(key);
}

void
_kthreadstartproc(Proc *p)
{
	Proc *np;
	pthread_t tid;
	sigset_t all;

	np = p->newproc;
	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, nil);
	if(pthread_create(&tid, nil, (void*(*)(void*))_threadscheduler, 
			np) < 0)
		sysfatal("pthread_create: %r");
	np->pthreadid = tid;
}

void
_kthreadexitproc(char *exitstr)
{
	_threaddebug(DBGSCHED, "_pthreadexit");
	pthread_exit(nil);
}

void
_kthreadexitallproc(char *exitstr)
{
	_threaddebug(DBGSCHED, "_threadexitallproc");
	exits(exitstr);
}

/*
 * Exec.  Pthreads does the hard work of making it possible
 * for any thread to do the waiting, so this is pretty easy.
 * We create a separate proc whose job is to wait for children
 * and deliver wait messages.
 */
static Channel *_threadexecwaitchan;

static void
_threadwaitproc(void *v)
{
	Channel *c;
	Waitmsg *w;

	_threadinternalproc();

	USED(v);
	
	for(;;){
		w = wait();
		if(w == nil){
			if(errno == ECHILD)	/* wait for more */
				recvul(_threadexecwaitchan);
			continue;
		}
		if((c = _threadwaitchan) != nil)
			sendp(c, w);
		else
			free(w);
	}
	fprint(2, "_threadwaitproc exits\n");	/* not reached */
}


/* 
 * Call _threadexec in the right conditions.
 */
int
_kthreadexec(Channel *c, int fd[3], char *prog, char *args[], int freeargs)
{
	static Lock lk;
	int rv;

	if(!_threadexecwaitchan){
		lock(&lk);
		if(!_threadexecwaitchan){
			_threadexecwaitchan = chancreate(sizeof(ulong), 1);
			proccreate(_threadwaitproc, nil, 32*1024);
		}
		unlock(&lk);
	}
	rv = _threadexec(c, fd, prog, args, freeargs);
	nbsendul(_threadexecwaitchan, 1);
	return rv;
}

/*
 * Some threaded applications want to run in the background.
 * Calling fork() and exiting in the parent will result in a child
 * with a single pthread (if we are using pthreads), and will screw
 * up our internal process info if we are using clone/rfork.
 * Instead, apps should call threadbackground(), which takes
 * care of this.
 * 
 * _threadbackgroundinit is called from main.
 */

static int mainpid, passerpid;

static void
passer(void *x, char *msg)
{
	Waitmsg *w;

	USED(x);
	if(strcmp(msg, "sys: usr2") == 0)
		_exit(0);	/* daemonize */
	else if(strcmp(msg, "sys: child") == 0){
		/* child exited => so should we */
		w = wait();
		if(w == nil)
			_exit(1);
		_exit(atoi(w->msg));
	}else
		postnote(PNGROUP, mainpid, msg);
}

void
_threadbackgroundinit(void)
{
	int pid;
	sigset_t mask;

	sigfillset(&mask);
	pthread_sigmask(SIG_BLOCK, &mask, 0);

return;

	passerpid = getpid();
	switch(pid = fork()){
	case -1:
		sysfatal("fork: %r");

	case 0:
		rfork(RFNOTEG);
		return;

	default:
		break;
	}

	mainpid = pid;
	notify(passer);
	notifyon("sys: child");
	notifyon("sys: usr2");	/* should already be on */
	for(;;)
		pause();
	_exit(0);
}

void
threadbackground(void)
{
	if(passerpid <= 1)
		return;
	postnote(PNPROC, passerpid, "sys: usr2");
}

/*
 * Notes.
 */
Channel *_threadnotechan;
static ulong sigs;
static Lock _threadnotelk;
static void _threadnoteproc(void*);
extern int _p9strsig(char*);
extern char *_p9sigstr(int);

Channel*
threadnotechan(void)
{
	if(_threadnotechan == nil){
		lock(&_threadnotelk);
		if(_threadnotechan == nil){
			_threadnotechan = chancreate(sizeof(char*), 1);
			proccreate(_threadnoteproc, nil, 32*1024);
		}
		unlock(&_threadnotelk);
	}
	return _threadnotechan;
}

void
_threadnote(void *x, char *msg)
{
	USED(x);

	if(_threadexitsallstatus)
		_kthreadexitproc(_threadexitsallstatus);

	if(strcmp(msg, "sys: usr2") == 0)
		noted(NCONT);

	if(_threadnotechan == nil)
		noted(NDFLT);

	sigs |= 1<<_p9strsig(msg);
	noted(NCONT);
}

void
_threadnoteproc(void *x)
{
	int i;
	sigset_t none;
	Channel *c;

	_threadinternalproc();
	sigemptyset(&none);
	pthread_sigmask(SIG_SETMASK, &none, 0);

	c = _threadnotechan;
	for(;;){
		if(sigs == 0)
			pause();
		for(i=0; i<32; i++){
			if((sigs&(1<<i)) == 0)
				continue;
			sigs &= ~(1<<i);
			if(i == 0)
				continue;
			sendp(c, _p9sigstr(i));
		}
	}
}

void
_threadschednote(void)
{
}

void
_kmaininit(void)
{
	sigset_t all;

	sigfillset(&all);
	pthread_sigmask(SIG_SETMASK, &all, 0);
}
