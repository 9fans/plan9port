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

int mainstacksize;

extern void (*_sysfatal)(char*, va_list);
extern Jmp *(*_notejmpbuf)(void);

static void
mainlauncher(void *arg)
{
	Mainarg *a;

	a = arg;
	_kmaininit();
	threadmain(a->argc, a->argv);
	threadexits("threadmain");
}

int
main(int argc, char **argv)
{
	Mainarg a;
	Proc *p;

	/*
	 * In pthreads, threadmain is actually run in a subprocess,
	 * so that the main process can exit (if threaddaemonize is called).
	 * The main process relays notes to the subprocess.
	 * _Threadbackgroundsetup will return only in the subprocess.
	 */
	_threadbackgroundinit();

	/*
	 * Instruct QLock et al. to use our scheduling functions
	 * so that they can operate at the thread level.
	 */
	_qlockinit(_threadsleep, _threadwakeup);

	/*
	 * Install our own _threadsysfatal which takes down
	 * the whole confederation of procs.
	 */
	_sysfatal = _threadsysfatal;

	/*
	 * Install our own jump handler.
	 */
	_notejmpbuf = _threadgetjmp;

	/*
	 * Install our own signal handler.
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

Jmp*
_threadgetjmp(void)
{
	static Jmp j;
	Proc *p;

	p = _threadgetproc();
	if(p == nil)
		return &j;
	return &p->sigjmp;
}
