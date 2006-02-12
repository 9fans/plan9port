/*
 * System dependent header files for research
 */

#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <bio.h>
#include <libString.h>

/*
 *  for the lock routines in libsys.c
 */
typedef struct Mlock	Mlock;
struct Mlock {
	int fd;
	int pid;
	String *name;
};

/*
 *  from config.c - call upasconfig() before using
 */
extern char *_MAILROOT;	/* root of mail system */
extern char *_UPASLOG;	/* log directory */
extern char *_UPASLIB;	/* upas library directory */
extern char *_UPASBIN;	/* upas binary directory */
extern char *_UPASTMP;	/* temporary directory */
extern char *_SHELL;	/* path name of shell */
extern char *_POST;	/* path name of post server addresses */
extern int MBOXMODE;	/* default mailbox protection mode */
extern void upasconfig(void);

/* forgive me */
#define	MAILROOT	(upasconfig(), _MAILROOT)
#define	UPASLOG	(upasconfig(), _UPASLOG)
#define	UPASLIB	(upasconfig(), _UPASLIB)
#define	UPASBIN	(upasconfig(), _UPASBIN)
#define	UPASTMP	(upasconfig(), _UPASTMP)
#define	SHELL	(upasconfig(), _SHELL)
#define	POST	(upasconfig(), _POST)

/*
 *  files in libsys.c
 */
extern char	*sysname_read(void);
extern char	*alt_sysname_read(void);
extern char	*domainname_read(void);
extern char	**sysnames_read(void);
extern char	*getlog(void);
extern char	*thedate(void);
extern Biobuf	*sysopen(char*, char*, ulong);
extern int	sysopentty(void);
extern int	sysclose(Biobuf*);
extern int	sysmkdir(char*, ulong);
extern int	syschgrp(char*, char*);
extern Mlock	*syslock(char *);
extern void	sysunlock(Mlock *);
extern void	syslockrefresh(Mlock *);
extern int	e_nonexistent(void);
extern int	e_locked(void);
extern long	sysfilelen(Biobuf*);
extern int	sysremove(char*);
extern int	sysrename(char*, char*);
extern int	sysexist(char*);
extern int	sysisdir(char*);
extern int	syskill(int);
extern int	syskillpg(int);
extern int	syscreate(char*, int, ulong);
extern Mlock	*trylock(char *);
extern void	pipesig(int*);
extern void	pipesigoff(void);
extern int	holdon(void);
extern void	holdoff(int);
extern int	syscreatelocked(char*, int, int);
extern int	sysopenlocked(char*, int);
extern int	sysunlockfile(int);
extern int	sysfiles(void);
extern int 	become(char**, char*);
extern int	sysdetach(void);
extern int	sysdirreadall(int, Dir**);
extern String	*username(String*);
extern char*	remoteaddr(int, char*);
extern int	creatembox(char*, char*);

extern String	*readlock(String*);
extern char	*homedir(char*);
extern String	*mboxname(char*, String*);
extern String	*deadletter(String*);

/*
 *  maximum size for a file path
 */
#define MAXPATHLEN 128
