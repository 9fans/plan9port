#include "ffork-pthread.c"

#ifdef OLD
/*
 * Is nothing simple?
 *
 * We can't free the stack until we've finished executing,
 * but once we've finished executing, we can't do anything
 * at all, including call free.  So instead we keep a linked list
 * of all stacks for all processes, and every few times we try
 * to allocate a new stack we scan the current stack list for
 * dead processes and reclaim those stacks.
 */

#include <u.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <libc.h>
#include "9proc.h"

int fforkstacksize = 16384;

typedef struct Stack Stack;
struct Stack
{
	Stack *next;
	Stack *fnext;
	int pid;
};

static Lock stacklock;
static Stack *freestacks;
static Stack *allstacks;
static int stackmallocs;
static void gc(void);

static void*
mallocstack(void)
{
	Stack *p;

	lock(&stacklock);
top:
	p = freestacks;
	if(p)
		freestacks = p->fnext;
	else{
		if(stackmallocs++%1 == 0)
			gc();
		if(freestacks)
			goto top;
		p = malloc(fforkstacksize);
		p->next = allstacks;
		allstacks = p;
	}
	if(p)
		p->pid = 1;
	unlock(&stacklock);
	return p;
}

static void
gc(void)
{
	Stack *p;

	for(p=allstacks; p; p=p->next){
		if(p->pid > 1)
		if(kill(p->pid, 0) < 0 && errno == ESRCH){
			if(0) fprint(2, "reclaim stack from %d\n", p->pid);
			p->pid = 0;
		}
		if(p->pid == 0){
			p->fnext = freestacks;
			freestacks = p;
		}
	}
}

static void
freestack(void *v)
{
	Stack *p;

	p = v;
	if(p == nil)
		return;
	lock(&stacklock);
	p->fnext = freestacks;
	p->pid = 0;
	freestacks = p;
	unlock(&stacklock);
	return;
}

static int
tramp(void *v)
{
	void (*fn)(void*), *arg;
	void **v2;
	void *p;

	_p9uproc(0);
	v2 = v;
	fn = v2[0];
	arg = v2[1];
	p = v2[2];
	free(v2);
	fn(arg);
	_exit(0);
	return 0;
}
	
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

int
ffork(int flags, void (*fn)(void*), void *arg)
{
	void **v;
	char *p;
	int cloneflag, pid, thepid, status, nowait;

	_p9uproc(0);
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
	if(!(flags&RFNOWAIT))
		cloneflag |= SIGCHLD;
	else
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
	else
		((Stack*)p)->pid = thepid;
	return thepid;
}

int
getfforkid(void)
{
	return getpid();
}

#endif
