#include "u.h"
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>
#if !defined(__OpenBSD__)
#	if defined(__APPLE__)
#		define _XOPEN_SOURCE 	/* for Snow Leopard */
#	endif
#	include <ucontext.h>
#endif
#include <sys/utsname.h>
#include "libc.h"
#include "thread.h"

#if defined(__FreeBSD__) && __FreeBSD__ < 5
extern	int		getmcontext(mcontext_t*);
extern	void		setmcontext(mcontext_t*);
#define	setcontext(u)	setmcontext(&(u)->uc_mcontext)
#define	getcontext(u)	getmcontext(&(u)->uc_mcontext)
extern	int		swapcontext(ucontext_t*, ucontext_t*);
extern	void		makecontext(ucontext_t*, void(*)(), int, ...);
#endif

#if defined(__APPLE__)
	/*
	 * OS X before 10.5 (Leopard) does not provide
	 * swapcontext nor makecontext, so we have to use our own.
	 * In theory, Leopard does provide them, but when we use 
	 * them, they seg fault.  Maybe we're using them wrong.
	 * So just use our own versions, even on Leopard.
	 */
#	define mcontext libthread_mcontext
#	define mcontext_t libthread_mcontext_t
#	define ucontext libthread_ucontext
#	define ucontext_t libthread_ucontext_t
#	define swapcontext libthread_swapcontext
#	define makecontext libthread_makecontext
#	if defined(__i386__)
#		include "386-ucontext.h"
#	elif defined(__x86_64__)
#		include "x86_64-ucontext.h"
#	elif defined(__ppc__) || defined(__power__)
#		include "power-ucontext.h"
#	else
#		error "unknown architecture"
#	endif
#endif

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

/* THIS DOES NOT WORK!  Don't do this!
(At least, not on Solaris.  Maybe this is right for Linux,
in which case it should say if defined(__linux__) && defined(__sun__),
but surely the latter would be defined(__sparc__).

#if defined(__sun__)
#	define mcontext libthread_mcontext
#	define mcontext_t libthread_mcontext_t
#	define ucontext libthread_ucontext
#	define ucontext_t libthread_ucontext_t
#	include "sparc-ucontext.h"
#endif
*/

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
#ifdef __APPLE__
	/*
	 * On Snow Leopard, etc., the context routines exist,
	 * so we use them, but apparently they write past the
	 * end of the ucontext_t.  Sigh.  We put some extra
	 * scratch space here for them.
	 */
	uchar	buf[1024];
#endif
};

struct Execjob
{
	int *fd;
	char *cmd;
	char **argv;
	char *dir;
	Channel *c;
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
	uchar	*stk;
	uint	stksize;
	int		exiting;
	Proc	*proc;
	char	name[256];
	char	state[256];
	void *udata;
	Alt	*alt;
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
	_Thread		*thread;
	_Thread		*pinthread;
	_Threadlist	runqueue;
	_Threadlist	idlequeue;
	_Threadlist	allthreads;
	uint		nthread;
	uint		sysproc;
	_Procrendez	runrend;
	Context	schedcontext;
	void		*udata;
	Jmp		sigjmp;
	int		mainproc;
};

#define proc() _threadproc()
#define setproc(p) _threadsetproc(p)

extern Proc *_threadprocs;
extern Lock _threadprocslock;
extern Proc *_threadexecproc;
extern Channel *_threadexecchan;
extern QLock _threadexeclock;
extern Channel *_dowaitchan;

extern void _procstart(Proc*, void (*fn)(Proc*));
extern _Thread *_threadcreate(Proc*, void(*fn)(void*), void*, uint);
extern void _threadexit(void);
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
