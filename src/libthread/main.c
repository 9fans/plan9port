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

static void
mainlauncher(void *arg)
{
	Mainarg *a;

	a = arg;
	_threadmaininit();
	threadmain(a->argc, a->argv);
	threadexits("threadmain");
}

int
main(int argc, char **argv)
{
	Mainarg a;
	Proc *p;

	/*
	 * XXX Do daemonize hack here.
	 */

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
