/*
 * Thread library.  
 */

#include "threadimpl.h"

typedef struct Mainarg Mainarg;
struct Mainarg
{
	int argc;
	char **argv;
};

int _threadmainpid;
int mainstacksize;
int _callsthreaddaemonize;
static int passtomainpid;

extern void (*_sysfatal)(char*, va_list);

static void
mainlauncher(void *arg)
{
	Mainarg *a;

	a = arg;
	_threadmaininit();
	threadmain(a->argc, a->argv);
	threadexits("threadmain");
}

static void
passer(void *x, char *msg)
{
	USED(x);
	Waitmsg *w;

	if(strcmp(msg, "sys: usr2") == 0)
		_exit(0);	/* daemonize */
	else if(strcmp(msg, "sys: child") == 0){
		w = wait();
		if(w == nil)
			_exit(1);
		_exit(atoi(w->msg));
	}else
		postnote(PNPROC, passtomainpid, msg);
}

int
main(int argc, char **argv)
{
	int pid;
	Mainarg a;
	Proc *p;
	sigset_t mask;

	/*
	 * Do daemonize hack here.
	 */
	if(_callsthreaddaemonize){
		passtomainpid = getpid();
		switch(pid = fork()){
		case -1:
			sysfatal("fork: %r");

		case 0:
			/* continue executing */
			_threadmainpid = getppid();
			break;

		default:
			/* wait for signal USR2 */
			notify(passer);
			for(;;)
				pause();
			_exit(0);
		}
	}

	/*
	 * Instruct QLock et al. to use our scheduling functions
	 * so that they can operate at the thread level.
	 */
	_qlockinit(_threadsleep, _threadwakeup);

	/*
	 * Install our own _threadsysfatal which takes down
	 * the whole conglomeration of procs.
	 */
	_sysfatal = _threadsysfatal;

	/*
	 * XXX Install our own jump handler.
	 */

	/*
	 * Install our own signal handlers.
	 */
	notify(_threadnote);

	/*
	 * Construct the initial proc running mainlauncher(&a).
	 */
	if(mainstacksize == 0)
		mainstacksize = 32*1024;
	a.argc = argc;
	a.argv = argv;
	p = _newproc();
	_newthread(p, mainlauncher, &a, mainstacksize, "threadmain", 0);
	_threadscheduler(p);
	abort();	/* not reached */
	return 0;
}

/*
 * No-op function here so that sched.o drags in main.o.
 */
void
_threadlinkmain(void)
{
}
