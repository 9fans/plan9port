#ifndef _THREAD_H_
#define _THREAD_H_ 1
#if defined(__cplusplus)
extern "C" {
#endif

AUTOLIB(thread)

/*
 * basic procs and threads
 */
int		proccreate(void (*f)(void *arg), void *arg, unsigned int stacksize);
int		threadcreate(void (*f)(void *arg), void *arg, unsigned int stacksize);
void		threadexits(char *);
void		threadexitsall(char *);
void		threadsetname(char*, ...);
void		threadsetstate(char*, ...);
void		threadneedbackground(void);
char		*threadgetname(void);
int			threadyield(void);
int			threadidle(void);
void		_threadready(_Thread*);
void		_threadswitch(void);
void		_threadsetsysproc(void);
void		_threadsleep(Rendez*);
_Thread	*_threadwakeup(Rendez*);
#define	yield		threadyield
int		threadid(void);
void		_threadpin(void);
void		_threadunpin(void);

/*
 * I am tired of making this mistake.
 */
#define	exits	do_not_call_exits_in_threaded_programs
#define	_exits	do_not_call__exits_in_threaded_programs



/*
 * signals
 */
void		threadnotify(int(*f)(void*,char*), int);

/*
 * daemonize
 *
void	threaddaemonize(void);
 */

/*
 * per proc and thread data
 */
void		**procdata(void);
void		**threaddata(void);

/*
 * supplied by user instead of main.
 * mainstacksize is size of stack allocated to run threadmain
 */
void		threadmain(int argc, char *argv[]);
extern	int	mainstacksize;

int	threadmaybackground(void);

/*
 * channel communication
 */
typedef struct Alt Alt;
typedef struct _Altarray _Altarray;
typedef struct Channel Channel;

enum
{
	CHANEND,
	CHANSND,
	CHANRCV,
	CHANNOP,
	CHANNOBLK
};

struct Alt
{
	Channel		*c;
	void		*v;
	uint		op;
	_Thread		*thread;
};

struct _Altarray
{
	Alt			**a;
	uint		n;
	uint		m;
};

struct Channel
{
	uint			bufsize;
	uint			elemsize;
	uchar		*buf;
	uint			nbuf;
	uint			off;
	_Altarray	asend;
	_Altarray	arecv;
	char			*name;
};

/* [Edit .+1,./^$/ |cfn -h $PLAN9/src/libthread/channel.c] */
int		chanalt(Alt *alts);
Channel*	chancreate(int elemsize, int elemcnt);
void		chanfree(Channel *c);
int		channbrecv(Channel *c, void *v);
void*		channbrecvp(Channel *c);
ulong		channbrecvul(Channel *c);
int		channbsend(Channel *c, void *v);
int		channbsendp(Channel *c, void *v);
int		channbsendul(Channel *c, ulong v);
int		chanrecv(Channel *c, void *v);
void*		chanrecvp(Channel *c);
ulong		chanrecvul(Channel *c);
int		chansend(Channel *c, void *v);
int		chansendp(Channel *c, void *v);
int		chansendul(Channel *c, ulong v);
void		chansetname(Channel *c, char *fmt, ...);

#define	alt		chanalt
#define	nbrecv	channbrecv
#define	nbrecvp	channbrecvp
#define	nbrecvul	channbrecvul
#define	nbsend	channbsend
#define	nbsendp	channbsendp
#define	nbsendul	channbsendul
#define	recv		chanrecv
#define	recvp	chanrecvp
#define	recvul	chanrecvul
#define	send		chansend
#define	sendp	chansendp
#define	sendul	chansendul

/*
 * reference counts
 */
typedef struct Ref	Ref;

struct Ref {
	Lock lock;
	long ref;
};

long		decref(Ref *r);
long		incref(Ref *r);

/*
 * slave i/o processes
 */
typedef struct Ioproc Ioproc;

/* [Edit .+1,/^$/ |cfn -h $PLAN9/src/libthread/io*.c] */
void		closeioproc(Ioproc *io);
long		iocall(Ioproc *io, long (*op)(va_list*), ...);
int		ioclose(Ioproc *io, int fd);
int		iodial(Ioproc *io, char *addr, char *local, char *dir, int *cdfp);
void		iointerrupt(Ioproc *io);
int		ioopen(Ioproc *io, char *path, int mode);
Ioproc*		ioproc(void);
long		ioread(Ioproc *io, int fd, void *a, long n);
int		ioread9pmsg(Ioproc*, int, void*, int);
long		ioreadn(Ioproc *io, int fd, void *a, long n);
int		iorecvfd(Ioproc *, int);
int		iosendfd(Ioproc*, int, int);
int		iosleep(Ioproc *io, long n);
long		iowrite(Ioproc *io, int fd, void *a, long n);

/*
 * exec external programs
 */
void		threadexec(Channel*, int[3], char*, char *[]);
void		threadexecl(Channel*, int[3], char*, ...);
int		threadspawn(int[3], char*, char*[]);
int		threadspawnd(int[3], char*, char*[], char*);
int		threadspawnl(int[3], char*, ...);
Channel*	threadwaitchan(void);

/*
 * alternate interface to threadwaitchan - don't use both!
 */
Waitmsg*	procwait(int pid);

#if defined(__cplusplus)
}
#endif
#endif	/* _THREADH_ */
