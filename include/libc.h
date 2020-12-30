// This file originated as Plan 9's /sys/include/libc.h.
// The plan9port-specific changes may be distributed
// using the license in ../src/lib9/LICENSE.

/*
 * Lib9 is miscellany from the Plan 9 C library that doesn't
 * fit into libutf or into libfmt, but is still missing from traditional
 * Unix C libraries.
 */
#ifndef _LIBC_H_
#define _LIBC_H_ 1
#if defined(__cplusplus)
extern "C" {
#endif

#include <utf.h>
#include <fmt.h>

/*
 * Begin usual libc.h
 */

#ifndef nil
#define	nil	((void*)0)
#endif
#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))

#ifndef offsetof
#define offsetof(s, m)	(ulong)(&(((s*)0)->m))
#endif

/*
 * mem routines (provided by system <string.h>)
 *
extern	void*	memccpy(void*, void*, int, ulong);
extern	void*	memset(void*, int, ulong);
extern	int	memcmp(void*, void*, ulong);
extern	void*	memcpy(void*, void*, ulong);
extern	void*	memmove(void*, void*, ulong);
extern	void*	memchr(void*, int, ulong);
 */

/*
 * string routines (provided by system <string.h>)
 *
extern	char*	strcat(char*, char*);
extern	char*	strchr(char*, int);
extern	int	strcmp(char*, char*);
extern	char*	strcpy(char*, char*);
 */
extern	char*	strecpy(char*, char*, char*);
extern	char*	p9strdup(char*);
/*
extern	char*	strncat(char*, char*, long);
extern	char*	strncpy(char*, char*, long);
extern	int	strncmp(char*, char*, long);
extern	char*	strpbrk(char*, char*);
extern	char*	strrchr(char*, int);
extern	char*	strtok(char*, char*);
extern	long	strlen(char*);
extern	long	strspn(char*, char*);
extern	long	strcspn(char*, char*);
extern	char*	strstr(char*, char*);
 */
extern	int	cistrncmp(char*, char*, int);
extern	int	cistrcmp(char*, char*);
extern	char*	cistrstr(char*, char*);
extern	int	tokenize(char*, char**, int);

/*
enum
{
	UTFmax		= 4,
	Runesync	= 0x80,
	Runeself	= 0x80,
	Runeerror	= 0xFFFD,
	Runemax	= 0x10FFFF,
};
*/

/*
 * rune routines (provided by <utf.h>
 *
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(long);
extern	int	runenlen(Rune*, int);
extern	int	fullrune(char*, int);
extern	int	utflen(char*);
extern	int	utfnlen(char*, long);
extern	char*	utfrune(char*, long);
extern	char*	utfrrune(char*, long);
extern	char*	utfutf(char*, char*);
extern	char*	utfecpy(char*, char*, char*);

extern	Rune*	runestrcat(Rune*, Rune*);
extern	Rune*	runestrchr(Rune*, Rune);
extern	int	runestrcmp(Rune*, Rune*);
extern	Rune*	runestrcpy(Rune*, Rune*);
extern	Rune*	runestrncpy(Rune*, Rune*, long);
extern	Rune*	runestrecpy(Rune*, Rune*, Rune*);
extern	Rune*	runestrdup(Rune*);
extern	Rune*	runestrncat(Rune*, Rune*, long);
extern	int	runestrncmp(Rune*, Rune*, long);
extern	Rune*	runestrrchr(Rune*, Rune);
extern	long	runestrlen(Rune*);
extern	Rune*	runestrstr(Rune*, Rune*);

extern	Rune	tolowerrune(Rune);
extern	Rune	totitlerune(Rune);
extern	Rune	toupperrune(Rune);
extern	int	isalpharune(Rune);
extern	int	islowerrune(Rune);
extern	int	isspacerune(Rune);
extern	int	istitlerune(Rune);
extern	int	isupperrune(Rune);
 */

/*
 * malloc (provied by system <stdlib.h>)
 *
extern	void*	malloc(ulong);
 */
extern	void*	p9malloc(ulong);
extern	void*	mallocz(ulong, int);
extern	void	p9free(void*);
extern	void*	p9calloc(ulong, ulong);
extern	void*	p9realloc(void*, ulong);
extern	void		setmalloctag(void*, ulong);
extern	void		setrealloctag(void*, ulong);
extern	ulong	getmalloctag(void*);
extern	ulong	getrealloctag(void*);
/*
extern	void*	malloctopoolblock(void*);
*/
#ifndef NOPLAN9DEFINES
#define	malloc	p9malloc
#define	realloc	p9realloc
#define	calloc	p9calloc
#define	free	p9free
#undef strdup
#define	strdup	p9strdup
#endif

/*
 * print routines (provided by <fmt.h>)
 *
typedef struct Fmt	Fmt;
struct Fmt{
	uchar	runes;
	void	*start;
	void	*to;
	void	*stop;
	int	(*flush)(Fmt *);
	void	*farg;
	int	nfmt;
	va_list	args;
	int	r;
	int	width;
	int	prec;
	ulong	flags;
};

enum{
	FmtWidth	= 1,
	FmtLeft		= FmtWidth << 1,
	FmtPrec		= FmtLeft << 1,
	FmtSharp	= FmtPrec << 1,
	FmtSpace	= FmtSharp << 1,
	FmtSign		= FmtSpace << 1,
	FmtZero		= FmtSign << 1,
	FmtUnsigned	= FmtZero << 1,
	FmtShort	= FmtUnsigned << 1,
	FmtLong		= FmtShort << 1,
	FmtVLong	= FmtLong << 1,
	FmtComma	= FmtVLong << 1,
	FmtByte	= FmtComma << 1,

	FmtFlag		= FmtByte << 1
};

extern	int	print(char*, ...);
extern	char*	seprint(char*, char*, char*, ...);
extern	char*	vseprint(char*, char*, char*, va_list);
extern	int	snprint(char*, int, char*, ...);
extern	int	vsnprint(char*, int, char*, va_list);
extern	char*	smprint(char*, ...);
extern	char*	vsmprint(char*, va_list);
extern	int	sprint(char*, char*, ...);
extern	int	fprint(int, char*, ...);
extern	int	vfprint(int, char*, va_list);

extern	int	runesprint(Rune*, char*, ...);
extern	int	runesnprint(Rune*, int, char*, ...);
extern	int	runevsnprint(Rune*, int, char*, va_list);
extern	Rune*	runeseprint(Rune*, Rune*, char*, ...);
extern	Rune*	runevseprint(Rune*, Rune*, char*, va_list);
extern	Rune*	runesmprint(char*, ...);
extern	Rune*	runevsmprint(char*, va_list);

extern	int	fmtfdinit(Fmt*, int, char*, int);
extern	int	fmtfdflush(Fmt*);
extern	int	fmtstrinit(Fmt*);
extern	char*	fmtstrflush(Fmt*);
extern	int	runefmtstrinit(Fmt*);
extern	Rune*	runefmtstrflush(Fmt*);

extern	int	fmtinstall(int, int (*)(Fmt*));
extern	int	dofmt(Fmt*, char*);
extern	int	dorfmt(Fmt*, Rune*);
extern	int	fmtprint(Fmt*, char*, ...);
extern	int	fmtvprint(Fmt*, char*, va_list);
extern	int	fmtrune(Fmt*, int);
extern	int	fmtstrcpy(Fmt*, char*);
extern	int	fmtrunestrcpy(Fmt*, Rune*);
 */

/*
 * error string for %r
 * supplied on per os basis, not part of fmt library
 *
 * (provided by lib9, but declared in fmt.h)
 *
extern	int	errfmt(Fmt *f);
 */

/*
 * quoted strings
 */
extern	char	*unquotestrdup(char*);
extern	Rune	*unquoterunestrdup(Rune*);
extern	char	*quotestrdup(char*);
extern	Rune	*quoterunestrdup(Rune*);
/*
 * in fmt.h
 *
extern	void	quotefmtinstall(void);
extern	int	quotestrfmt(Fmt*);
extern	int	quoterunestrfmt(Fmt*);
 */
#ifndef NOPLAN9DEFINES
#define doquote fmtdoquote
#endif
extern	int	needsrcquote(int);

/*
 * random number
 */
extern	void	p9srand(long);
extern	int	p9rand(void);

extern	int	p9nrand(int);
extern	long	p9lrand(void);
extern	long	p9lnrand(long);
extern	double	p9frand(void);
extern	ulong	truerand(void);			/* uses /dev/random */
extern	ulong	ntruerand(ulong);		/* uses /dev/random */

#ifndef NOPLAN9DEFINES
#define	srand	p9srand
#define	rand	p9rand
#define	nrand	p9nrand
#define	lrand	p9lrand
#define	lnrand	p9lnrand
#define	frand	p9frand
#endif

/*
 * math
 */
extern	ulong	getfcr(void);
extern	void	setfsr(ulong);
extern	ulong	getfsr(void);
extern	void	setfcr(ulong);
extern	double	NaN(void);
extern	double	Inf(int);
extern	int	isNaN(double);
extern	int	isInf(double, int);
extern	ulong	umuldiv(ulong, ulong, ulong);
extern	long	muldiv(long, long, long);

/*
 * provided by math.h
 *
extern	double	pow(double, double);
extern	double	atan2(double, double);
extern	double	fabs(double);
extern	double	atan(double);
extern	double	log(double);
extern	double	log10(double);
extern	double	exp(double);
extern	double	floor(double);
extern	double	ceil(double);
extern	double	hypot(double, double);
extern	double	sin(double);
extern	double	cos(double);
extern	double	tan(double);
extern	double	asin(double);
extern	double	acos(double);
extern	double	sinh(double);
extern	double	cosh(double);
extern	double	tanh(double);
extern	double	sqrt(double);
extern	double	fmod(double, double);
#define	HUGE	3.4028234e38
#define	PIO2	1.570796326794896619231e0
#define	PI	(PIO2+PIO2)
 */
#define PI	M_PI
#define	PIO2	M_PI_2

/*
 * Time-of-day
 */

typedef
struct Tm
{
	int	sec;
	int	min;
	int	hour;
	int	mday;
	int	mon;
	int	year;
	int	wday;
	int	yday;
	char	zone[4];
	int	tzoff;
} Tm;

extern	Tm*	p9gmtime(long);
extern	Tm*	p9localtime(long);
extern	char*	p9asctime(Tm*);
extern	char*	p9ctime(long);
extern	double	p9cputime(void);
extern	long	p9times(long*);
extern	long	p9tm2sec(Tm*);
extern	vlong	p9nsec(void);

#ifndef NOPLAN9DEFINES
#define	gmtime		p9gmtime
#define	localtime	p9localtime
#define	asctime		p9asctime
#define	ctime		p9ctime
#define	cputime		p9cputime
#define	times		p9times
#define	tm2sec		p9tm2sec
#define	nsec		p9nsec
#endif

/*
 * one-of-a-kind
 */
enum
{
	PNPROC		= 1,
	PNGROUP		= 2
};

/* extern	int	abs(int); <stdlib.h> */
extern	int	p9atexit(void(*)(void));
extern	void	p9atexitdont(void(*)(void));
extern	int	atnotify(int(*)(void*, char*), int);
/*
 * <stdlib.h>
extern	double	atof(char*); <stdlib.h>
 */
extern	int	p9atoi(char*);
extern	long	p9atol(char*);
extern	vlong	p9atoll(char*);
extern	double	fmtcharstod(int(*)(void*), void*);
extern	char*	cleanname(char*);
extern	int	p9decrypt(void*, void*, int);
extern	int	p9encrypt(void*, void*, int);
extern	int	netcrypt(void*, void*);
extern	int	dec64(uchar*, int, char*, int);
extern	int	enc64(char*, int, uchar*, int);
extern	int	dec32(uchar*, int, char*, int);
extern	int	enc32(char*, int, uchar*, int);
extern	int	dec16(uchar*, int, char*, int);
extern	int	enc16(char*, int, uchar*, int);
extern	int	encodefmt(Fmt*);
extern	int	dirmodefmt(Fmt*);
extern	int	exitcode(char*);
extern	void	exits(char*);
extern	double	p9frexp(double, int*);
extern	ulong	getcallerpc(void*);
#if defined(__GNUC__) || defined(__clang__) || defined(__IBMC__)
#define getcallerpc(x) ((ulong)__builtin_return_address(0))
#endif
extern	char*	p9getenv(char*);
extern	int	p9putenv(char*, char*);
extern	int	getfields(char*, char**, int, int, char*);
extern	int	gettokens(char *, char **, int, char *);
extern	char*	getuser(void);
extern	char*	p9getwd(char*, int);
extern	int	iounit(int);
/* extern	long	labs(long); <math.h> */
/* extern	double	ldexp(double, int); <math.h> */
extern	void	p9longjmp(p9jmp_buf, int);
extern	char*	mktemp(char*);
extern	int		opentemp(char*, int);
/* extern	double	modf(double, double*); <math.h> */
extern	void	p9notejmp(void*, p9jmp_buf, int);
extern	void	perror(const char*);
extern	int	postnote(int, int, char *);
extern	double	p9pow10(int);
/* extern	int	putenv(char*, char*); <stdlib.h. */
/* extern	void	qsort(void*, long, long, int (*)(void*, void*)); <stdlib.h> */
extern	char*	searchpath(char*);
/* extern	int	p9setjmp(p9jmp_buf); */
#define p9setjmp(b)	sigsetjmp((void*)(b), 1)
/*
 * <stdlib.h>
extern	long	strtol(char*, char**, int);
extern	ulong	strtoul(char*, char**, int);
extern	vlong	strtoll(char*, char**, int);
extern	uvlong	strtoull(char*, char**, int);
 */
extern	void	sysfatal(char*, ...);
extern	void	p9syslog(int, char*, char*, ...);
extern	long	p9time(long*);
/* extern	int	tolower(int); <ctype.h> */
/* extern	int	toupper(int); <ctype.h> */
extern	void	needstack(int);
extern	char*	readcons(char*, char*, int);

extern	void	(*_pin)(void);
extern	void	(*_unpin)(void);

#ifndef NOPLAN9DEFINES
#define atexit		p9atexit
#define atexitdont	p9atexitdont
#define atoi		p9atoi
#define atol		p9atol
#define atoll		p9atoll
#define encrypt		p9encrypt
#define decrypt		p9decrypt
#undef frexp
#define frexp		p9frexp
#define getenv		p9getenv
#define	getwd		p9getwd
#define	longjmp		p9longjmp
#undef  setjmp
#define setjmp		p9setjmp
#define putenv		p9putenv
#define notejmp		p9notejmp
#define jmp_buf		p9jmp_buf
#define time		p9time
#define pow10		p9pow10
#define strtod		fmtstrtod
#define charstod	fmtcharstod
#define syslog		p9syslog
#endif

/*
 *  just enough information so that libc can be
 *  properly locked without dragging in all of libthread
 */
typedef struct _Thread _Thread;
typedef struct _Threadlist _Threadlist;
struct _Threadlist
{
	_Thread	*head;
	_Thread	*tail;
};

extern	_Thread	*(*threadnow)(void);

/*
 *  synchronization
 */
typedef struct Lock Lock;
struct Lock
{
	int init;
	pthread_mutex_t mutex;
	int held;
};

extern	void	lock(Lock*);
extern	void	unlock(Lock*);
extern	int	canlock(Lock*);
extern	int	(*_lock)(Lock*, int, ulong);
extern	void	(*_unlock)(Lock*, ulong);

typedef struct QLock QLock;
struct QLock
{
	Lock		l;
	_Thread	*owner;
	_Threadlist	waiting;
};

extern	void	qlock(QLock*);
extern	void	qunlock(QLock*);
extern	int	canqlock(QLock*);
extern	int	(*_qlock)(QLock*, int, ulong);	/* do not use */
extern	void	(*_qunlock)(QLock*, ulong);

typedef struct Rendez Rendez;
struct Rendez
{
	QLock	*l;
	_Threadlist	waiting;
};

extern	void	rsleep(Rendez*);	/* unlocks r->l, sleeps, locks r->l again */
extern	int	rwakeup(Rendez*);
extern	int	rwakeupall(Rendez*);
extern	void	(*_rsleep)(Rendez*, ulong);	/* do not use */
extern	int	(*_rwakeup)(Rendez*, int, ulong);

typedef struct RWLock RWLock;
struct RWLock
{
	Lock		l;
	int	readers;
	_Thread	*writer;
	_Threadlist	rwaiting;
	_Threadlist	wwaiting;
};

extern	void	rlock(RWLock*);
extern	void	runlock(RWLock*);
extern	int		canrlock(RWLock*);
extern	void	wlock(RWLock*);
extern	void	wunlock(RWLock*);
extern	int		canwlock(RWLock*);
extern	int	(*_rlock)(RWLock*, int, ulong);	/* do not use */
extern	int	(*_wlock)(RWLock*, int, ulong);
extern	void	(*_runlock)(RWLock*, ulong);
extern	void	(*_wunlock)(RWLock*, ulong);

/*
 * per-process private data
 */
extern	void**	privalloc(void);
extern	void	privfree(void**);

/*
 *  network dialing
 */
#define NETPATHLEN 40
extern	int	p9accept(int, char*);
extern	int	p9announce(char*, char*);
extern	int	p9dial(char*, char*, char*, int*);
extern	int	p9dialparse(char *ds, char **net, char **unixa, void *ip, int *port);
extern	void	p9setnetmtpt(char*, int, char*);
extern	int	p9listen(char*, char*);
extern	char*	p9netmkaddr(char*, char*, char*);
extern	int	p9reject(int, char*, char*);

#ifndef NOPLAN9DEFINES
#define	accept		p9accept
#define	announce	p9announce
#define	dial		p9dial
#define	setnetmtpt	p9setnetmtpt
#define	listen		p9listen
#define	netmkaddr	p9netmkaddr
#define	reject		p9reject
#endif

/*
 *  encryption
 */
extern	int	pushssl(int, char*, char*, char*, int*);
extern	int	pushtls(int, char*, char*, int, char*, char*);

/*
 *  network services
 */
typedef struct NetConnInfo NetConnInfo;
struct NetConnInfo
{
	char	*dir;		/* connection directory */
	char	*root;		/* network root */
	char	*spec;		/* binding spec */
	char	*lsys;		/* local system */
	char	*lserv;		/* local service */
	char	*rsys;		/* remote system */
	char	*rserv;		/* remote service */
	char *laddr;
	char *raddr;
};
extern	NetConnInfo*	getnetconninfo(char*, int);
extern	void		freenetconninfo(NetConnInfo*);

/*
 * system calls
 *
 */
#define	STATMAX	65535U	/* max length of machine-independent stat structure */
#define	DIRMAX	(sizeof(Dir)+STATMAX)	/* max length of Dir structure */
#define	ERRMAX	128	/* max length of error string */

#define	MORDER	0x0003	/* mask for bits defining order of mounting */
#define	MREPL	0x0000	/* mount replaces object */
#define	MBEFORE	0x0001	/* mount goes before others in union directory */
#define	MAFTER	0x0002	/* mount goes after others in union directory */
#define	MCREATE	0x0004	/* permit creation in mounted directory */
#define	MCACHE	0x0010	/* cache some data */
#define	MMASK	0x0017	/* all bits on */

#define	OREAD	0	/* open for read */
#define	OWRITE	1	/* write */
#define	ORDWR	2	/* read and write */
#define	OEXEC	3	/* execute, == read but check execute permission */
#define	OTRUNC	16	/* or'ed in (except for exec), truncate file first */
#define	OCEXEC	32	/* or'ed in, close on exec */
#define	ORCLOSE	64	/* or'ed in, remove on close */
#define	ODIRECT	128	/* or'ed in, direct access */
#define	ONONBLOCK 256	/* or'ed in, non-blocking call */
#define	OEXCL	0x1000	/* or'ed in, exclusive use (create only) */
#define	OLOCK	0x2000	/* or'ed in, lock after opening */
#define	OAPPEND	0x4000	/* or'ed in, append only */

#define	AEXIST	0	/* accessible: exists */
#define	AEXEC	1	/* execute access */
#define	AWRITE	2	/* write access */
#define	AREAD	4	/* read access */

/* Segattch */
#define	SG_RONLY	0040	/* read only */
#define	SG_CEXEC	0100	/* detach on exec */

#define	NCONT	0	/* continue after note */
#define	NDFLT	1	/* terminate after note */
#define	NSAVE	2	/* clear note but hold state */
#define	NRSTR	3	/* restore saved state */

/* bits in Qid.type */
#define QTDIR		0x80		/* type bit for directories */
#define QTAPPEND	0x40		/* type bit for append only files */
#define QTEXCL		0x20		/* type bit for exclusive use files */
#define QTMOUNT		0x10		/* type bit for mounted channel */
#define QTAUTH		0x08		/* type bit for authentication file */
#define QTTMP		0x04		/* type bit for non-backed-up file */
#define QTSYMLINK	0x02		/* type bit for symbolic link */
#define QTFILE		0x00		/* type bits for plain file */

/* bits in Dir.mode */
#define DMDIR		0x80000000	/* mode bit for directories */
#define DMAPPEND	0x40000000	/* mode bit for append only files */
#define DMEXCL		0x20000000	/* mode bit for exclusive use files */
#define DMMOUNT		0x10000000	/* mode bit for mounted channel */
#define DMAUTH		0x08000000	/* mode bit for authentication file */
#define DMTMP		0x04000000	/* mode bit for non-backed-up file */
#define DMSYMLINK	0x02000000	/* mode bit for symbolic link (Unix, 9P2000.u) */
#define DMDEVICE	0x00800000	/* mode bit for device file (Unix, 9P2000.u) */
#define DMNAMEDPIPE	0x00200000	/* mode bit for named pipe (Unix, 9P2000.u) */
#define DMSOCKET	0x00100000	/* mode bit for socket (Unix, 9P2000.u) */
#define DMSETUID	0x00080000	/* mode bit for setuid (Unix, 9P2000.u) */
#define DMSETGID	0x00040000	/* mode bit for setgid (Unix, 9P2000.u) */

#define DMREAD		0x4		/* mode bit for read permission */
#define DMWRITE		0x2		/* mode bit for write permission */
#define DMEXEC		0x1		/* mode bit for execute permission */

#ifdef RFMEM	/* FreeBSD, OpenBSD */
#undef RFFDG
#undef RFNOTEG
#undef RFPROC
#undef RFMEM
#undef RFNOWAIT
#undef RFCFDG
#undef RFNAMEG
#undef RFENVG
#undef RFCENVG
#undef RFCFDG
#undef RFCNAMEG
#endif

enum
{
	RFNAMEG		= (1<<0),
	RFENVG		= (1<<1),
	RFFDG		= (1<<2),
	RFNOTEG		= (1<<3),
	RFPROC		= (1<<4),
	RFMEM		= (1<<5),
	RFNOWAIT	= (1<<6),
	RFCNAMEG	= (1<<10),
	RFCENVG		= (1<<11),
	RFCFDG		= (1<<12)
/*	RFREND		= (1<<13), */
/*	RFNOMNT		= (1<<14) */
};

typedef
struct Qid
{
	uvlong	path;
	ulong	vers;
	uchar	type;
} Qid;

typedef
struct Dir {
	/* system-modified data */
	ushort	type;	/* server type */
	uint	dev;	/* server subtype */
	/* file data */
	Qid	qid;	/* unique id from server */
	ulong	mode;	/* permissions */
	ulong	atime;	/* last read time */
	ulong	mtime;	/* last write time */
	vlong	length;	/* file length */
	char	*name;	/* last element of path */
	char	*uid;	/* owner name */
	char	*gid;	/* group name */
	char	*muid;	/* last modifier name */

	/* 9P2000.u extensions */
	uint	uidnum;		/* numeric uid */
	uint	gidnum;		/* numeric gid */
	uint	muidnum;	/* numeric muid */
	char	*ext;		/* extended info */
} Dir;

/* keep /sys/src/ape/lib/ap/plan9/sys9.h in sync with this -rsc */
typedef
struct Waitmsg
{
	int pid;	/* of loved one */
	ulong time[3];	/* of loved one & descendants */
	char	*msg;
} Waitmsg;

typedef
struct IOchunk
{
	void	*addr;
	ulong	len;
} IOchunk;

extern	void	_exits(char*);

extern	void	abort(void);
/* extern	int	access(char*, int); */
extern	long	p9alarm(ulong);
extern	int	await(char*, int);
extern	int	awaitfor(int, char*, int);
extern	int	awaitnohang(char*, int);
/* extern	int	bind(char*, char*, int); give up */
/* extern	int	brk(void*); <unistd.h> */
extern	int	p9chdir(char*);
extern	int	p9close(int);
extern	int	p9create(char*, int, ulong);
extern	int	p9dup(int, int);
extern	int	errstr(char*, uint);
extern	int	p9exec(char*, char*[]);
extern	int	p9execl(char*, ...);
/* extern	int	p9fork(void); */
extern	int	p9rfork(int);
/* not implemented
extern	int	fauth(int, char*);
extern	int	fstat(int, uchar*, int);
extern	int	fwstat(int, uchar*, int);
extern	int	fversion(int, int, char*, int);
extern	int	mount(int, int, char*, int, char*);
extern	int	unmount(char*, char*);
*/
extern	int	noted(int);
extern	int	notify(void(*)(void*, char*));
extern	int	noteenable(char*);
extern	int	notedisable(char*);
extern	int	notifyon(char*);
extern	int	notifyoff(char*);
extern	int	p9open(char*, int);
extern	int	fd2path(int, char*, int);
extern	int	p9pipe(int*);
/*
 * use defs from <unistd.h>
extern	long	pread(int, void*, long, vlong);
extern	long	preadv(int, IOchunk*, int, vlong);
extern	long	pwrite(int, void*, long, vlong);
extern	long	pwritev(int, IOchunk*, int, vlong);
extern	long	read(int, void*, long);
 */
extern	long	readn(int, void*, long);
/* extern	long	readv(int, IOchunk*, int); <unistd.h> */
extern	int	remove(const char*);
/* extern	void*	sbrk(ulong); <unistd.h> */
/* extern	long	oseek(int, long, int); */
extern	vlong	p9seek(int, vlong, int);
/* give up
extern	long	segattach(int, char*, void*, ulong);
extern	int	segbrk(void*, void*);
extern	int	segdetach(void*);
extern	int	segflush(void*, ulong);
extern	int	segfree(void*, ulong);
*/
extern	int	p9sleep(long);
/* extern	int	stat(char*, uchar*, int); give up */
extern	Waitmsg*	p9wait(void);
extern	Waitmsg*	p9waitfor(int);
extern	Waitmsg*	waitnohang(void);
extern	int	p9waitpid(void);
/* <unistd.h>
extern	long	write(int, void*, long);
extern	long	writev(int, IOchunk*, int);
*/
extern	long	p9write(int, void*, long);
/* extern	int	wstat(char*, uchar*, int); give up */
extern	ulong	rendezvous(ulong, ulong);

#ifndef NOPLAN9DEFINES
#define alarm		p9alarm
#define	dup		p9dup
#define	exec		p9exec
#define	execl	p9execl
#define	seek		p9seek
#define sleep		p9sleep
#define wait		p9wait
#define waitpid		p9waitpid
/* #define fork		p9fork */
#define rfork		p9rfork
/* #define access		p9access */
#define create		p9create
#undef open
#define open		p9open
#undef close
#define close		p9close
#define pipe		p9pipe
#define	waitfor		p9waitfor
#define write		p9write
#endif

extern	Dir*	dirstat(char*);
extern	Dir*	dirfstat(int);
extern	int	dirwstat(char*, Dir*);
extern	int	dirfwstat(int, Dir*);
extern	long	dirread(int, Dir**);
extern	void	nulldir(Dir*);
extern	long	dirreadall(int, Dir**);
/* extern	int	getpid(void); <unistd.h> */
/* extern	int	getppid(void); */
extern	void	rerrstr(char*, uint);
extern	char*	sysname(void);
extern	void	werrstr(char*, ...);
extern	char*	getns(void);
extern	char*	get9root(void);
extern	char*	unsharp(char*);
extern	int	sendfd(int, int);
extern	int	recvfd(int);
extern	int	post9pservice(int, char*, char*);
extern	int	chattyfuse;

/* external names that we don't want to step on */
#ifndef NOPLAN9DEFINES
#define main	p9main
#endif

#ifdef VARARGCK
#pragma	varargck	type	"lld"	vlong
#pragma	varargck	type	"llx"	vlong
#pragma	varargck	type	"lld"	uvlong
#pragma	varargck	type	"llx"	uvlong
#pragma	varargck	type	"ld"	long
#pragma	varargck	type	"lx"	long
#pragma	varargck	type	"ld"	ulong
#pragma	varargck	type	"lx"	ulong
#pragma	varargck	type	"d"	int
#pragma	varargck	type	"x"	int
#pragma	varargck	type	"c"	int
#pragma	varargck	type	"C"	int
#pragma	varargck	type	"d"	uint
#pragma	varargck	type	"x"	uint
#pragma	varargck	type	"c"	uint
#pragma	varargck	type	"C"	uint
#pragma	varargck	type	"f"	double
#pragma	varargck	type	"e"	double
#pragma	varargck	type	"g"	double
#pragma	varargck	type	"lf"	long double
#pragma	varargck	type	"le"	long double
#pragma	varargck	type	"lg"	long double
#pragma	varargck	type	"s"	char*
#pragma	varargck	type	"q"	char*
#pragma	varargck	type	"S"	Rune*
#pragma	varargck	type	"Q"	Rune*
#pragma	varargck	type	"r"	void
#pragma	varargck	type	"%"	void
#pragma	varargck	type	"n"	int*
#pragma	varargck	type	"p"	void*
#pragma	varargck	type	"<"	void*
#pragma	varargck	type	"["	void*
#pragma	varargck	type	"H"	void*
#pragma	varargck	type	"lH"	void*

#pragma	varargck	flag	' '
#pragma	varargck	flag	'#'
#pragma	varargck	flag	'+'
#pragma	varargck	flag	','
#pragma	varargck	flag	'-'
#pragma	varargck	flag	'u'

#pragma	varargck	argpos	fmtprint	2
#pragma	varargck	argpos	fprint	2
#pragma	varargck	argpos	print	1
#pragma	varargck	argpos	runeseprint	3
#pragma	varargck	argpos	runesmprint	1
#pragma	varargck	argpos	runesnprint	3
#pragma	varargck	argpos	runesprint	2
#pragma	varargck	argpos	seprint	3
#pragma	varargck	argpos	smprint	1
#pragma	varargck	argpos	snprint	3
#pragma	varargck	argpos	sprint	2
#pragma	varargck	argpos	sysfatal	1
#pragma	varargck	argpos	p9syslog	3
#pragma	varargck	argpos	werrstr	1
#endif

/* compiler directives on plan 9 */
#define	SET(x)	((x)=0)
#define	USED(x)	if(x){}else{}
#ifdef __GNUC__
#	if __GNUC__ >= 3
#		undef USED
#		define USED(x) ((void)(x))
#	endif
#endif

/* command line */
extern char	*argv0;
extern void __fixargv0(void);
#define	ARGBEGIN	for((argv0?0:(argv0=(__fixargv0(),*argv))),argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt;\
				Rune _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while(*_args && (_args += chartorune(&_argc, _args)))\
				switch(_argc)
#define	ARGEND		SET(_argt);USED(_argt);USED(_argc);USED(_args);}USED(argv);USED(argc);
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	EARGF(x)	(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): ((x), abort(), (char*)0)))

#define	ARGC()		_argc

#if defined(__cplusplus)
}
#endif
#endif	/* _LIB9_H_ */
