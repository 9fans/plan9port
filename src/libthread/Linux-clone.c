#include <u.h>
#include <errno.h>
#include <sched.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include "threadimpl.h"

#define procid()		getpid()
#define procexited(id)	(kill((id), 0) < 0 && errno==ESRCH)

static int multi;
static Proc *theproc;

/*
 * Run all execs forked from a special exec proc.
 */
#include "execproc.ch"

/*
 * Use _exits to exit one proc, and signals to tear everyone else down.
 */
#include "exit-getpid.ch"

/*
 * Use table for _threadmultiproc, _threadsetproc, _threadgetproc.
 */
#include "proctab.ch"

/*
 * Use per-process stack allocation code.
 */
#include "procstack.ch"

/*
 * Implement _threadstartproc using clone.
 * 
 * Cannot use this on newer kernels (2.6+) because
 * on those kernels clone allows you to set up a per-thread
 * segment using %gs, and the C library and compilers
 * assume that you've done this.  I don't want to learn
 * how to do this (the code below is scary enough),
 * so on those more recent kernels we use nptl (the
 * pthreads implementation), which is finally good enough.
 */

/*
 * Trampoline to run f(arg).
 */
static int
tramp(void *v)
{
	void (*fn)(void*), *arg;
	void **v2;
	void *p;

	v2 = v;
	fn = v2[0];
	arg = v2[1];
	p = v2[2];
	free(v2);
	fn(arg);
	abort();	/* not reached! */
	return 0;
}

/*
 * Trampnowait runs in the child, and starts a granchild to run f(arg).
 * When trampnowait proc exits, the connection between the
 * grandchild running f(arg) and the parent/grandparent is lost, so the
 * grandparent need not worry about cleaning up a zombie
 * when the grandchild finally exits.
 */
static int
trampnowait(void *v)
{
	int pid;
	int cloneflag;
	void **v2;
	int *pidp;
	void *p;

	v2 = v;
	cloneflag = (int)v2[4];
	pidp = v2[3];
	p = v2[2];
	pid = clone(tramp, p+fforkstacksize-512, cloneflag, v);
	*pidp = pid;
	_exit(0);
	return 0;
}

static int
ffork(int flags, void (*fn)(void*), void *arg)
{
	void **v;
	char *p;
	int cloneflag, pid, thepid, status, nowait;

	p = mallocstack();
	v = malloc(sizeof(void*)*5);
	if(p==nil || v==nil){
		freestack(p);
		free(v);
		return -1;
	}
	cloneflag = 0;
	flags &= ~RFPROC;
	if(flags&RFMEM){
		cloneflag |= CLONE_VM;
		flags &= ~RFMEM;
	}
	if(!(flags&RFFDG))
		cloneflag |= CLONE_FILES;
	else
		flags &= ~RFFDG;
	nowait = flags&RFNOWAIT;
//	if(!(flags&RFNOWAIT))
//		cloneflag |= SIGCHLD;
//	else
		flags &= ~RFNOWAIT;
	if(flags){
		fprint(2, "unknown rfork flags %x\n", flags);
		freestack(p);
		free(v);
		return -1;
	}
	v[0] = fn;
	v[1] = arg;
	v[2] = p;
	v[3] = &thepid;
	v[4] = (void*)cloneflag;
	thepid = -1;
	pid = clone(nowait ? trampnowait : tramp, p+fforkstacksize-16, cloneflag, v);
	if(pid > 0 && nowait){
		if(wait4(pid, &status, __WALL, 0) < 0)
			fprint(2, "ffork wait4: %r\n");
	}else
		thepid = pid;
	if(thepid == -1)
		freestack(p);
	else{
		((Stack*)p)->pid = thepid;
	}
	return thepid;
}

/*
 * Called to start a new proc.
 */
void
_threadstartproc(Proc *p)
{
	int pid;
	Proc *np;

	np = p->newproc;
	pid = ffork(RFPROC|RFMEM|RFNOWAIT, _threadscheduler, np);
	if(pid == -1)
		sysfatal("starting new proc: %r");
	np->pid = pid;
}

/*
 * Called to associate p with the current pthread.
 */
void
_threadinitproc(Proc *p)
{
	sigset_t mask;

	p->pid = getpid();
	sigemptyset(&mask);
	sigaddset(&mask, WAITSIG);
	sigprocmask(SIG_BLOCK, &mask, nil);
	_threadsetproc(p);
}

/*
 * Called from mainlauncher before threadmain.
 */
void
_threadmaininit(void)
{
}

