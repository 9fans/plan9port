/* 
 * Some notes on locking:
 *
 *	All the locking woes come from implementing
 *	threadinterrupt (and threadkill).
 *
 *	_threadgetproc()->thread is always a live pointer.
 *	p->threads, p->ready, and _threadrgrp also contain
 * 	live thread pointers.  These may only be consulted
 *	while holding p->lock or _threadrgrp.lock; in procs
 *	other than p, the pointers are only guaranteed to be live
 *	while the lock is still being held.
 *
 *	Thread structures can only be freed by the proc
 *	they belong to.  Threads marked with t->inrendez
 * 	need to be extracted from the _threadrgrp before
 *	being freed.
 *
 *	_threadrgrp.lock cannot be acquired while holding p->lock.
 */

#include <assert.h>
#include <lib9.h>
#include <thread.h>
#include "label.h"

typedef struct Thread	Thread;
typedef struct Proc	Proc;
typedef struct Tqueue	Tqueue;
typedef struct Pqueue	Pqueue;
typedef struct Rgrp		Rgrp;
typedef struct Execargs	Execargs;

/* must match list in sched.c */
typedef enum
{
	Dead,
	Running,
	Ready,
	Rendezvous,
} State;
	
typedef enum
{
	Channone,
	Chanalt,
	Chansend,
	Chanrecv,
} Chanstate;

enum
{
	RENDHASH = 10009,
	Printsize = 2048,
	NPRIV = 8,
};

struct Rgrp
{
	Lock		lock;
	Thread	*hash[RENDHASH];
};

struct Tqueue		/* Thread queue */
{
	int		asleep;
	Thread	*head;
	Thread	*tail;
};

struct Thread
{
	Lock		lock;			/* protects thread data structure */
	Label	sched;		/* for context switches */
	int		id;			/* thread id */
	int 		grp;			/* thread group */
	int		moribund;	/* thread needs to die */
	State		state;		/* run state */
	State		nextstate;		/* next run state */
	uchar	*stk;			/* top of stack (lowest address of stack) */
	uint		stksize;		/* stack size */
	Thread	*next;		/* next on ready queue */

	Proc		*proc;		/* proc of this thread */
	Thread	*nextt;		/* next on list of threads in this proc */
	Thread	*prevt;		/* prev on list of threads in this proc */
	int		ret;			/* return value for Exec, Fork */

	char		*cmdname;	/* ptr to name of thread */

	int		inrendez;	
	Channel	*altc;
	_Procrend	altrend;

	Chanstate	chan;		/* which channel operation is current */
	Alt		*alt;			/* pointer to current alt structure (debugging) */
	ulong		userpc;
	Channel	*c;
	pthread_cond_t cond;

	void*	udata[NPRIV];	/* User per-thread data pointer */
	int		lastfd;
};

struct Execargs
{
	char		*prog;
	char		**args;
	int		fd[2];
	int		*stdfd;
};

struct Proc
{
	Lock		lock;
	Label	sched;		/* for context switches */
	Proc		*link;		/* in proctab */
	int		pid;			/* process id */
	int		splhi;		/* delay notes */
	Thread	*thread;		/* running thread */
	Thread	*idle;			/* idle thread */

	int		needexec;
	Execargs	exec;		/* exec argument */
	Proc		*newproc;	/* fork argument */
	char		exitstr[ERRMAX];	/* exit status */

	int		rforkflag;
	int		nthreads;
	Tqueue	threads;		/* All threads of this proc */
	Tqueue	ready;		/* Runnable threads */
	Lock		readylock;

	char		printbuf[Printsize];
	int		blocked;		/* In a rendezvous */
	int		pending;		/* delayed note pending */
	int		nonotes;		/*  delay notes */
	uint		nextID;		/* ID of most recently created thread */
	Proc		*next;		/* linked list of Procs */

	_Procrend	rend;	/* sleep here for more ready threads */

	void		*arg;			/* passed between shared and unshared stk */
	char		str[ERRMAX];	/* used by threadexits to avoid malloc */
	char		errbuf[ERRMAX];	/* errstr */
	Waitmsg		*waitmsg;

	void*	udata;		/* User per-proc data pointer */
	int		nsched;
};

struct Pqueue {		/* Proc queue */
	Lock		lock;
	Proc		*head;
	Proc		**tail;
};

struct Ioproc
{
	int tid;
	Channel *c, *creply;
	int inuse;
	long (*op)(va_list*);
	va_list arg;
	long ret;
	char err[ERRMAX];
	Ioproc *next;
};

void		_swaplabel(Label*, Label*);
void		_freeproc(Proc*);
Proc*	_newproc(void(*)(void*), void*, uint, char*, int, int);
int		_procsplhi(void);
void		_procsplx(int);
int		_sched(void);
int		_schedexec(Execargs*);
void		_schedexecwait(void);
void		_schedexit(Proc*);
int		_schedfork(Proc*);
void		_scheduler(void*);
void		_systhreadinit(void);
void		_threadassert(char*);
void		__threaddebug(ulong, char*, ...);
#define _threaddebug if(!_threaddebuglevel){}else __threaddebug
void		_threadexitsall(char*);
Proc*	_threadgetproc(void);
extern void	_threadmultiproc(void);
Proc*	_threaddelproc(void);
void		_threadsetproc(Proc*);
void		_threadinitstack(Thread*, void(*)(void*), void*);
void*	_threadmalloc(long, int);
void		_threadnote(void*, char*);
void		_threadready(Thread*);
void		_threadidle(void);
void		_threadsleep(_Procrend*);
void		_threadwakeup(_Procrend*);
void		_threadsignal(void);
void		_threadsysfatal(char*, va_list);
long		_xdec(long*);
void		_xinc(long*);
void		_threadremove(Proc*, Thread*);
void		threadstatus(void);

extern int			_threaddebuglevel;
extern char*		_threadexitsallstatus;
extern Pqueue		_threadpq;
extern Channel*	_threadwaitchan;
extern Rgrp		_threadrgrp;
extern void _stackfree(void*);

#define DBGAPPL	(1 << 0)
#define DBGSCHED	(1 << 16)
#define DBGCHAN	(1 << 17)
#define DBGREND	(1 << 18)
/* #define DBGKILL	(1 << 19) */
#define DBGNOTE	(1 << 20)
#define DBGEXEC	(1 << 21)

#define ioproc_arg(io, type)	(va_arg((io)->arg, type))
extern int _threadgetpid(void);
extern void _threadmemset(void*, int, int);
extern void _threaddebugmemset(void*, int, int);
extern int _threadprocs;
extern void _threadstacklimit(void*, void*);
