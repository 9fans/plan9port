/*
 * Lib9 is miscellany from the Plan 9 C library that doesn't
 * fit into libutf or into libfmt, but is still missing from traditional
 * Unix C libraries.
 */
#ifndef _LIB9H_
#define _LIB9H_ 1

#if defined(__cplusplus)
extern "C" {
#endif
                                                                                

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <setjmp.h>

#ifndef _FMTH_
#	include <fmt.h>
#endif

#define	nil	((void*)0)
#define	nelem(x)	(sizeof(x)/sizeof((x)[0]))

#define _NEEDUCHAR 1
#define _NEEDUSHORT 1
#define _NEEDUINT 1
#define _NEEDULONG 1

#if defined(__linux__)
#	include <sys/types.h>
#	if defined(__USE_MISC)
#		undef _NEEDUSHORT
#		undef _NEEDUINT
#		undef _NEEDULONG
#	endif
#endif
#if defined(__FreeBSD__)
#	include <sys/types.h>
#	if !defined(_POSIX_SOURCE)
#		undef _NEEDUSHORT
#		undef _NEEDUINT
#	endif
#endif
#if defined(__APPLE__)
#	include <sys/types.h>
#	undef _NEEDUSHORT
#	undef _NEEDUINT
#endif

typedef signed char schar;
typedef unsigned int u32int;
#ifdef _NEEDUCHAR
	typedef unsigned char uchar;
#endif
#ifdef _NEEDUSHORT
	typedef unsigned short ushort;
#endif
#ifdef _NEEDUINT
	typedef unsigned int uint;
#endif
#ifdef _NEEDULONG
	typedef unsigned long ulong;
#endif
typedef unsigned long long uvlong;
typedef long long vlong;

#define NAMELEN	28
#define CHDIR		0x80000000	/* mode bit for directories */
#define CHAPPEND	0x40000000	/* mode bit for append only files */
#define CHEXCL		0x20000000	/* mode bit for exclusive use files */
#define CHMOUNT		0x10000000	/* mode bit for mounted channel */
#define CHREAD		0x4		/* mode bit for read permission */
#define CHWRITE		0x2		/* mode bit for write permission */
#define CHEXEC		0x1		/* mode bit for execute permission */

/* rfork to create new process running fn(arg) */

#if defined(__FreeBSD__)
#undef RFFDG
#undef RFNOTEG
#undef RFPROC
#undef RFMEM
#undef RFNOWAIT
#undef RFCFDG
#endif

enum
{
/*	RFNAMEG		= (1<<0), */
/*	RFENVG		= (1<<1), */
	RFFDG		= (1<<2),
	RFNOTEG		= (1<<3),
	RFPROC		= (1<<4),
	RFMEM		= (1<<5),
	RFNOWAIT	= (1<<6),
/*	RFCNAMEG	= (1<<10), */
/*	RFCENVG		= (1<<11), */
	RFCFDG		= (1<<12),
/*	RFREND		= (1<<13), */
/*	RFNOMNT		= (1<<14) */
};
extern int		ffork(int, void(*)(void*), void*);

/* wait for processes */
#define wait _p9wait
typedef struct Waitmsg Waitmsg;
struct Waitmsg
{
	int pid;	/* of loved one */
	ulong time[3];	/* of loved one & descendants */
	char	*msg;
};
extern	int		await(char*, int);
extern	Waitmsg*	wait(void);

/* synchronization */
typedef struct Lock Lock;
struct Lock
{
	int val;
};

extern int		_tas(void*);
extern void	lock(Lock*);
extern void	unlock(Lock*);
extern int		canlock(Lock*);

typedef struct QLp QLp;
struct QLp
{
	int		inuse;
	QLp		*next;
	int		state;
};

typedef struct QLock QLock;
struct QLock
{
	Lock	lock;
	int		locked;
	QLp		*head;
	QLp		*tail;
};

extern void	qlock(QLock*);
extern void	qunlock(QLock*);
extern int		canqlock(QLock*);
extern void	_qlockinit(ulong (*)(ulong, ulong));

typedef struct RWLock RWLock;
struct RWLock
{
	Lock	lock;
	int		readers;
	int		writer;
	QLp		*head;
	QLp		*tail;
};

extern void	rlock(RWLock*);
extern void	runlock(RWLock*);
extern int		canrlock(RWLock*);
extern void	wlock(RWLock*);
extern void	wunlock(RWLock*);
extern int		canwlock(RWLock*);

typedef struct Rendez Rendez;
struct Rendez
{
	QLock	*l;
	QLp		*head;
	QLp		*tail;
};

extern void	rsleep(Rendez*);
extern int		rwakeup(Rendez*);
extern int		rwakeupall(Rendez*);

extern ulong	rendezvous(ulong, ulong);

typedef	struct	Qid	Qid;
typedef	struct	Dir	Dir;

struct	Qid
{
	ulong	path;
	ulong	vers;
};

struct Dir
{
	char	name[NAMELEN];
	char	uid[NAMELEN];
	char	gid[NAMELEN];
	Qid	qid;
	ulong	mode;
	long	atime;
	long	mtime;
	vlong	length;
	ushort	type;
	ushort	dev;
};

extern	int	dirstat(char*, Dir*);
extern	int	dirfstat(int, Dir*);
extern	int	dirwstat(char*, Dir*);
extern	int	dirfwstat(int, Dir*);


/* one of a kind */
extern void	sysfatal(char*, ...);
extern int	nrand(int);
extern long	lrand(void);
extern void	setmalloctag(void*, ulong);
extern void	setrealloctag(void*, ulong);
extern void	*mallocz(ulong, int);
extern long	readn(int, void*, long);
extern void	exits(char*);
extern void	_exits(char*);
extern ulong	getcallerpc(void*);
extern	char*	cleanname(char*);

/* string routines */
extern char*	strecpy(char*, char*, char*);
extern int		tokenize(char*, char**, int);
extern int		cistrncmp(char*, char*, int);
extern int		cistrcmp(char*, char*);
extern char*	cistrstr(char*, char*);
extern int		getfields(char*, char**, int, int, char*);
extern int		gettokens(char *, char **, int, char *);

/* formatting helpers */
extern int		dec64(uchar*, int, char*, int);
extern int		enc64(char*, int, uchar*, int);
extern int		dec32(uchar*, int, char*, int);
extern int		enc32(char*, int, uchar*, int);
extern int		dec16(uchar*, int, char*, int);
extern int		enc16(char*, int, uchar*, int);
extern int		encodefmt(Fmt*);

/* error string */
enum
{
	ERRMAX = 128
};
extern void	rerrstr(char*, uint);
extern void	werrstr(char*, ...);
extern int		errstr(char*, uint);

/* compiler directives on plan 9 */
#define	USED(x)	if(x){}else{}
#define	SET(x)	((x)=0)

/* command line */
extern char	*argv0;
extern void __fixargv0(void);
#define	ARGBEGIN	for((argv0||(argv0=(__fixargv0(),*argv))),argv++,argc--;\
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

#define OREAD O_RDONLY
#define OWRITE O_WRONLY
#define ORDWR	O_RDWR
#define AEXIST 0
#define AREAD 4
#define AWRITE 2
#define AEXEC 1
#define ORCLOSE 8
#define OCEXEC 16

#define dup dup2
#define exec execv
#define seek lseek
#define getwd getcwd

#if defined(__cplusplus)
}
#endif

#endif	/* _LIB9H_ */
