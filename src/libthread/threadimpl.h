#include <ucontext.h>

typedef struct Context Context;
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

struct _Thread
{
	_Thread	*next;
	_Thread	*prev;
	_Thread	*allnext;
	_Thread	*allprev;
	Context	context;
	uint	id;
	uchar	*stk;
	uint	stksize;
	int		exiting;
	void	(*startfn)(void*);
	void	*startarg;
	Proc	*proc;
	char	name[256];
	char	state[256];
};

struct _Procrendez
{
	Lock		*l;
	int		asleep;
	pthread_cond_t	cond;
};

extern	void	_procsleep(_Procrendez*);
extern	void	_procwakeup(_Procrendez*);

struct Proc
{
	pthread_t	tid;
	Lock		lock;
	_Thread		*thread;
	_Threadlist	runqueue;
	_Threadlist	allthreads;
	uint		nthread;
	uint		sysproc;
	_Procrendez	runrend;
	Context	schedcontext;
	void		*udata;
	Jmp		sigjmp;
};

extern Proc *xxx;
#define proc() _threadproc()
#define setproc(p) _threadsetproc(p)

extern void _procstart(Proc*, void (*fn)(void*));
extern _Thread *_threadcreate(Proc*, void(*fn)(void*), void*, uint);
extern void _threadexit(void);
extern Proc *_threadproc(void);
extern void _threadsetproc(Proc*);
extern int _threadlock(Lock*, int, ulong);
extern void _threadunlock(Lock*, ulong);
extern void _pthreadinit(void);
