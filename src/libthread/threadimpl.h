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

enum{
STKSIZE = 16384,
STKMAGIC = 0xCAFEBEEF
};

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
	Thread	*rendhash;	/* Trgrp linked list */
	ulong	rendtag;		/* rendezvous tag */
	ulong	rendval;		/* rendezvous value */
	int		rendbreak;	/* rendezvous has been taken */

	Chanstate	chan;		/* which channel operation is current */
	Alt		*alt;			/* pointer to current alt structure (debugging) */

	void*	udata[NPRIV];	/* User per-thread data pointer */
};

struct Execargs
{
	char		*prog;
	char		**args;
	int		fd[2];
};

struct Proc
{
	Lock		lock;
	Label	sched;		/* for context switches */
	Proc		*link;		/* in proctab */
	int		pid;			/* process id */
	int		splhi;		/* delay notes */
	Thread	*thread;		/* running thread */

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

	void		*arg;			/* passed between shared and unshared stk */
	char		str[ERRMAX];	/* used by threadexits to avoid malloc */
	char		errbuf[ERRMAX];	/* errstr */

	void*	udata;		/* User per-proc data pointer */
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

void		_gotolabel(Label*);
int		_setlabel(Label*);
void		_freeproc(Proc*);
Proc*	_newproc(void(*)(void*), void*, uint, char*, int, int);
int		_procsplhi(void);
void		_procsplx(int);
void		_sched(void);
int		_schedexec(Execargs*);
void		_schedexecwait(void);
void		_schedexit(Proc*);
int		_schedfork(Proc*);
void		_schedinit(void*);
void		_systhreadinit(void);
void		_threadassert(char*);
void		_threadbreakrendez(void);
void		__threaddebug(ulong, char*, ...);
#define _threaddebug if(!_threaddebuglevel){}else __threaddebug
void		_threadexitsall(char*);
void		_threadflagrendez(Thread*);
Proc*	_threadgetproc(void);
Proc*	_threaddelproc(void);
void		_threadsetproc(Proc*);
void		_threadinitstack(Thread*, void(*)(void*), void*);
void*	_threadmalloc(long, int);
void		_threadnote(void*, char*);
void		_threadready(Thread*);
ulong	_threadrendezvous(ulong, ulong);
void		_threadsignal(void);
void		_threadsysfatal(char*, va_list);
long		_xdec(long*);
void		_xinc(long*);
void		_threadremove(Proc*, Thread*);

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

