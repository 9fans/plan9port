/* 
 * Some notes on locking:
 *
 *	All the locking woes come from implementing
 *	threadinterrupt (and threadkill).
 *
 *	_threadgetproc()->thread is always a live pointer.
 *	p->threads, p->ready, and _threadrgrp also contain
 * 	live thread pointers.  These may only be consulted
 *	while holding p->lock; in procs other than p, the
 *	pointers are only guaranteed to be live while the lock
 *	is still being held.
 *
 *	Thread structures can only be freed by the proc
 *	they belong to.  Threads marked with t->inrendez
 * 	need to be extracted from the _threadrgrp before
 *	being freed.
 */

#include <u.h>
#include <assert.h>
#include <libc.h>
#include <thread.h>
#include "label.h"

typedef struct Thread	Thread;
typedef struct Proc		Proc;
typedef struct Tqueue	Tqueue;
typedef struct Pqueue	Pqueue;
typedef struct Execargs	Execargs;

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
	NPRIV = 8,
};

struct Tqueue		/* Thread queue */
{
	int		asleep;
	Thread	*head;
	Thread	*tail;
};

struct Pqueue {		/* Proc queue */
	Lock		lock;
	Proc		*head;
	Proc		**tail;
};

struct Thread
{
	Lock		lock;			/* protects thread data structure */

	int		asleep;		/* thread is in _threadsleep */
	Label	context;		/* for context switches */
	int 		grp;			/* thread group */
	int		id;			/* thread id */
	int		moribund;	/* thread needs to die */
	char		*name;		/* name of thread */
	Thread	*next;		/* next on ready queue */
	Thread	*nextt;		/* next on list of threads in this proc */
	State		nextstate;		/* next run state */
	Proc		*proc;		/* proc of this thread */
	Thread	*prevt;		/* prev on list of threads in this proc */
	int		ret;			/* return value for Exec, Fork */
	State		state;		/* run state */
	uchar	*stk;			/* top of stack (lowest address of stack) */
	uint		stksize;		/* stack size */
	void*	udata[NPRIV];	/* User per-thread data pointer */

	/*
	 * for debugging only
	 * (could go away without impacting correct behavior):
	 */

	Channel	*altc;
	_Procrend	altrend;

	Chanstate	chan;		/* which channel operation is current */
	Alt		*alt;			/* pointer to current alt structure (debugging) */
	ulong		userpc;
	Channel	*c;

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

	Label	context;		/* for context switches */
	Proc		*link;		/* in ptab */
	int		splhi;		/* delay notes */
	Thread	*thread;		/* running thread */
	Thread	*idle;			/* idle thread */
	int		id;

	int		needexec;
	Execargs	exec;		/* exec argument */
	Proc		*newproc;	/* fork argument */
	char		exitstr[ERRMAX];	/* exit status */

	int		rforkflag;
	int		nthreads;
	Tqueue	threads;		/* All threads of this proc */
	Tqueue	ready;		/* Runnable threads */
	Lock		readylock;

	int		blocked;		/* In a rendezvous */
	int		pending;		/* delayed note pending */
	int		nonotes;		/*  delay notes */
	uint		nextID;		/* ID of most recently created thread */
	Proc		*next;		/* linked list of Procs */


	void		(*schedfn)(Proc*);	/* function to call in scheduler */

	_Procrend	rend;	/* sleep here for more ready threads */

	void		*arg;			/* passed between shared and unshared stk */
	char		str[ERRMAX];	/* used by threadexits to avoid malloc */
	char		errbuf[ERRMAX];	/* errstr */
	Waitmsg		*waitmsg;

	void*	udata;		/* User per-proc data pointer */
	int		nsched;

	/*
	 * for debugging only
	 */
	int		pid;			/* process id */
	int		pthreadid;		/* pthread id */
};

void		_swaplabel(Label*, Label*);
Proc*	_newproc(void);
int		_newthread(Proc*, void(*)(void*), void*, uint, char*, int);
int		_procsplhi(void);
void		_procsplx(int);
int		_sched(void);
int		_schedexec(Execargs*);
void		_schedexecwait(void);
void		_schedexit(Proc*);
int		_schedfork(Proc*);
void		_threadfree(Thread*);
void		_threadscheduler(void*);
void		_systhreadinit(void);
void		_threadassert(char*);
void		__threaddebug(ulong, char*, ...);
#define _threaddebug if(!_threaddebuglevel){}else __threaddebug
void		_threadexitsall(char*);
Proc*	_threadgetproc(void);
extern void	_threadmultiproc(void);
Proc*	_threaddelproc(void);
void		_threadinitproc(Proc*);
void		_threadwaitkids(Proc*);
void		_threadsetproc(Proc*);
void		_threadinitstack(Thread*, void(*)(void*), void*);
void		_threadlinkmain(void);
void*	_threadmalloc(long, int);
void		_threadnote(void*, char*);
void		_threadready(Thread*);
void		_threadsetidle(int);
void		_threadsleep(_Procrend*);
void		_threadwakeup(_Procrend*);
void		_threadsignal(void);
void		_threadsysfatal(char*, va_list);
long		_xdec(long*);
void		_xinc(long*);
void		_threadremove(Proc*, Thread*);
void		threadstatus(void);
void		_threadstartproc(Proc*);
void		_threadexitproc(char*);
void		_threadexitallproc(char*);

extern int			_threadnprocs;
extern int			_threaddebuglevel;
extern char*		_threadexitsallstatus;
extern Pqueue		_threadpq;
extern Channel*	_threadwaitchan;

#define DBGAPPL	(1 << 0)
#define DBGSCHED	(1 << 16)
#define DBGCHAN	(1 << 17)
#define DBGREND	(1 << 18)
/* #define DBGKILL	(1 << 19) */
#define DBGNOTE	(1 << 20)
#define DBGEXEC	(1 << 21)

extern void _threadmemset(void*, int, int);
extern void _threaddebugmemset(void*, int, int);
extern int _threadprocs;
extern void _threadstacklimit(void*, void*);
