#include "u.h"
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#if !defined(__OpenBSD__)
#	if defined(__APPLE__)
#		define _XOPEN_SOURCE    /* for Snow Leopard */
#	endif
#	include <ucontext.h>
#endif
#include <sys/utsname.h>
#include "libc.h"
#include "thread.h"

#if defined(__OpenBSD__)
#	define mcontext libthread_mcontext
#	define mcontext_t libthread_mcontext_t
#	define ucontext libthread_ucontext
#	define ucontext_t libthread_ucontext_t
#	if defined __i386__
#		include "386-ucontext.h"
#	elif defined __amd64__
#		include "x86_64-ucontext.h"
#	else
#		include "power-ucontext.h"
#	endif
extern pid_t rfork_thread(int, void*, int(*)(void*), void*);
#endif

#if defined(__arm__)
int mygetmcontext(ulong*);
void mysetmcontext(const ulong*);
#define	setcontext(u)	mysetmcontext(&(u)->uc_mcontext.arm_r0)
#define	getcontext(u)	mygetmcontext(&(u)->uc_mcontext.arm_r0)
#endif


typedef struct Context Context;
typedef struct Execjob Execjob;
typedef struct Proc Proc;
typedef struct _Procrendez _Procrendez;

typedef struct Jmp Jmp;
struct Jmp
{
	p9jmp_buf b;
};

enum
{
	STACK = 8192
};

struct Context
{
	ucontext_t	uc;
};

struct Execjob
{
	int *fd;
	char *cmd;
	char **argv;
	char *dir;
	Channel *c;
};

struct _Procrendez
{
	Lock		*l;
	int		asleep;
#ifdef PLAN9PORT_USING_PTHREADS
	pthread_cond_t	cond;
#else
	int		pid;
#endif
};

struct _Thread
{
	_Thread	*next;
	_Thread	*prev;
	_Thread	*allnext;
	_Thread	*allprev;
	Context	context;
	void	(*startfn)(void*);
	void	*startarg;
	uint	id;
#ifdef PLAN9PORT_USING_PTHREADS
	pthread_t	osprocid;
#else
	int		osprocid;
#endif
	uchar	*stk;
	uint	stksize;
	int		exiting;
	Proc	*proc;
	char	name[256];
	char	state[256];
	void *udata;
	Alt	*alt;
	_Procrendez schedrend;
};

extern	void	_procsleep(_Procrendez*);
extern	void	_procwakeup(_Procrendez*);
extern	void	_procwakeupandunlock(_Procrendez*);

struct Proc
{
	Proc		*next;
	Proc		*prev;
	char		msg[128];
#ifdef PLAN9PORT_USING_PTHREADS
	pthread_t	osprocid;
#else
	int		osprocid;
#endif
	Lock		lock;
	int			nswitch;
	_Thread		*thread0;
	_Thread		*thread;
	_Thread		*pinthread;
	_Threadlist	runqueue;
	_Threadlist	idlequeue;
	_Threadlist	allthreads;
	uint		nthread;
	uint		sysproc;
	_Procrendez	runrend;
	Lock		schedlock;
	_Thread	*schedthread;
	Context	schedcontext;
	void		*udata;
	Jmp		sigjmp;
	int		mainproc;
};

#define proc() _threadproc()

extern Proc *_threadprocs;
extern Lock _threadprocslock;
extern Proc *_threadexecproc;
extern Channel *_threadexecchan;
extern QLock _threadexeclock;
extern Channel *_dowaitchan;

extern void _procstart(Proc*, void (*fn)(Proc*));
extern _Thread *_threadcreate(Proc*, void(*fn)(void*), void*, uint);
extern void _procexit(void);
extern Proc *_threadproc(void);
extern void _threadsetproc(Proc*);
extern int _threadlock(Lock*, int, ulong);
extern void _threadunlock(Lock*, ulong);
extern void _pthreadinit(void);
extern int _threadspawn(int*, char*, char**, char*);
extern int _runthreadspawn(int*, char*, char**, char*);
extern void _threadsetupdaemonize(void);
extern void _threaddodaemonize(char*);
extern void _threadpexit(void);
extern void _threaddaemonize(void);
extern void *_threadstkalloc(int);
extern void _threadstkfree(void*, int);
extern void _threadpthreadmain(Proc*, _Thread*);
extern void _threadpthreadstart(Proc*, _Thread*);

#define USPALIGN(ucp, align) \
	(void*)((((uintptr)(ucp)->uc_stack.ss_sp+(ucp)->uc_stack.ss_size)-(align))&~((align)-1))
