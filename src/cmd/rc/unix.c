/*
 * Unix versions of system-specific functions
 *	By convention, exported routines herein have names beginning with an
 *	upper case letter.
 */
#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
#include "getflags.h"

#undef wait
#undef waitpid

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <sys/wait.h>

static void execfinit(void);

builtin Builtin[] = {
	"cd",		execcd,
	"whatis",	execwhatis,
	"eval",		execeval,
	"exec",		execexec,	/* but with popword first */
	"exit",		execexit,
	"shift",	execshift,
	"wait",		execwait,
	".",		execdot,
	"flag",		execflag,
	"finit",	execfinit,
	0
};

char Rcmain[PATH_MAX];
char Fdprefix[] = "/dev/fd/";

char *Signame[NSIG];

#define SEP '\1'
extern char **environ;
static char **envp;

static void
Xrdfn(void)
{
	char *s;
	int len;

	for(;*envp;envp++){
		for(s=*envp;*s && *s!='(' && *s!='=';s++);
		switch(*s){
		case '(':		/* Bourne again */
			if(strncmp(s, "()fn ", 5)!=0)
				continue;
			s=estrdup(s+2);
			len=strlen(s);
			s[len++]='\n';
			envp++;
			runq->pc--;	/* re-execute */
			execcmds(openiocore(s, len), estrdup("*environ*"), runq->local, runq->redir);
			runq->lex->qflag = 1;
			return;
		default:
			continue;
		}
	}
}

static void
execfinit(void)
{
	static union code rdfns[5];
	if(rdfns[0].i==0){
		rdfns[0].i = 1;
		rdfns[1].s = "*rdfns*";
		rdfns[2].f = Xrdfn;
		rdfns[3].f = Xreturn;
		rdfns[4].f = 0;
	}
	poplist();
	envp=environ;
	start(rdfns, 2, runq->local, runq->redir);
}

static int
cmpenv(const void *aa, const void *ab)
{
	return strcmp(*(char**)aa, *(char**)ab);
}

static char**
mkenv(void)
{
	char **env, **ep, *p, *q;
	struct var **h, *v;
	struct word *a;
	int nvar = 0, nchr = 0, sep;

	/*
	 * Slightly kludgy loops look at locals then globals.
	 * locals no longer exist - geoff
	 */
	for(h = gvar-1; h != &gvar[NVAR]; h++)
	for(v = h >= gvar? *h: runq->local; v ;v = v->next){
		if((v==vlook(v->name)) && v->val){
			nvar++;
			nchr+=strlen(v->name)+1;
			for(a = v->val;a;a = a->next)
				nchr+=strlen(a->word)+1;
		}
		if(v->fn){
			nvar++;
			nchr+=strlen(v->name)+strlen(v->fn[v->pc-1].s)+8;
		}
	}
	env = (char **)emalloc((nvar+1)*sizeof(char *)+nchr);
	ep = env;
	p = (char *)&env[nvar+1];
	for(h = gvar-1; h != &gvar[NVAR]; h++)
	for(v = h >= gvar? *h: runq->local;v;v = v->next){
		if((v==vlook(v->name)) && v->val){
			*ep++=p;
			q = v->name;
			while(*q) *p++=*q++;
			sep='=';
			for(a = v->val;a;a = a->next){
				*p++=sep;
				sep = SEP;
				q = a->word;
				while(*q) *p++=*q++;
			}
			*p++='\0';
		}
		if(v->fn){
			*ep++=p;
			*p++='#'; *p++='('; *p++=')';	/* to fool Bourne */
			*p++='f'; *p++='n'; *p++=' ';
			q = v->name;
			while(*q) *p++=*q++;
			*p++=' ';
			q = v->fn[v->pc-1].s;
			while(*q) *p++=*q++;
			*p++='\0';
		}
	}
	*ep = 0;
	qsort((void *)env, nvar, sizeof ep[0], cmpenv);
	return env;
}

static word*
envval(char *s)
{
	char *t, c;
	word *v;
	for(t=s;*t&&*t!=SEP;t++);
	c=*t;
	*t='\0';
	v=newword(s, c=='\0'?(word*)0:envval(t+1));
	*t=c;
	return v;
}

void
Vinit(void)
{
	char *s;

	for(envp=environ;*envp;envp++){
		for(s=*envp;*s && *s!='(' && *s!='=';s++);
		switch(*s){
		case '=':
			*s='\0';
			setvar(*envp, envval(s+1));
			*s='=';
			break;
		default: continue;
		}
	}
}

static void
sighandler(int sig)
{
	trap[sig]++;
	ntrap++;
}

void
Trapinit(void)
{
	int i;

	Signame[0] = "sigexit";

#ifdef SIGINT
	Signame[SIGINT] = "sigint";
#endif
#ifdef SIGTERM
	Signame[SIGTERM] = "sigterm";
#endif
#ifdef SIGHUP
	Signame[SIGHUP] = "sighup";
#endif
#ifdef SIGQUIT
	Signame[SIGQUIT] = "sigquit";
#endif
#ifdef SIGPIPE
	Signame[SIGPIPE] = "sigpipe";
#endif
#ifdef SIGUSR1
	Signame[SIGUSR1] = "sigusr1";
#endif
#ifdef SIGUSR2
	Signame[SIGUSR2] = "sigusr2";
#endif
#ifdef SIGBUS
	Signame[SIGBUS] = "sigbus";
#endif
#ifdef SIGWINCH
	Signame[SIGWINCH] = "sigwinch";
#endif

	for(i=1; i<NSIG; i++) if(Signame[i]){
#ifdef SA_RESTART
		struct sigaction a;

		sigaction(i, NULL, &a);
		a.sa_flags &= ~SA_RESTART;
		a.sa_handler = sighandler;
		sigaction(i, &a, NULL);
#else
		signal(i, sighandler);
#endif
	}
}

char*
Errstr(void)
{
	return strerror(errno);
}

int
Waitfor(int pid)
{
	thread *p;
	char num[12];
	int wpid, status;

	if(pid >= 0 && !havewaitpid(pid))
		return 0;
	while((wpid = wait(&status))!=-1){
		delwaitpid(wpid);
		inttoascii(num, WIFSIGNALED(status)?WTERMSIG(status)+1000:WEXITSTATUS(status));
		if(wpid==pid){
			setstatus(num);
			return 0;
		}
		for(p = runq->ret;p;p = p->ret)
			if(p->pid==wpid){
				p->pid=-1;
				p->status = estrdup(num);
				break;
			}
	}
	if(Eintr()) return -1;
	return 0;
}

static char **nextenv;

void
Updenv(void)
{
	if(nextenv){
		free(nextenv);
		nextenv = NULL;
	}
	if(err)
		flushio(err);
}

void
Exec(char **argv)
{
	if(nextenv==NULL) nextenv=mkenv();
	execve(argv[0], argv+1, nextenv);
}

int
Fork(void)
{
	Updenv();
	return fork();
}

void*
Opendir(char *name)
{
	return opendir(name);
}

char*
Readdir(void *arg, int onlydirs)
{
	DIR *rd = arg;
	struct dirent *ent = readdir(rd);
	if(ent == NULL)
		return 0;
	return ent->d_name;
}

void
Closedir(void *arg)
{
	DIR *rd = arg;
	closedir(rd);
}

long
Write(int fd, void *buf, long cnt)
{
	return write(fd, buf, cnt);
}

long
Read(int fd, void *buf, long cnt)
{
	return read(fd, buf, cnt);
}

long
Seek(int fd, long cnt, long whence)
{
	return lseek(fd, cnt, whence);
}

int
Executable(char *file)
{
	return access(file, 01)==0;
}

int
Open(char *file, int mode)
{
	static int tab[] = {O_RDONLY,O_WRONLY,O_RDWR,O_RDONLY};
	int fd = open(file, tab[mode&3]);
	if(fd >= 0 && mode == 3)
		unlink(file);
	return fd;
}

void
Close(int fd)
{
	close(fd);
}

int
Creat(char *file)
{
	return creat(file, 0666L);
}

int
Dup(int a, int b)
{
	return dup2(a, b);
}

int
Dup1(int a)
{
	return dup(a, 0);
}

void
Exit(void)
{
	Updenv();
	exit(truestatus()?0:1);
}

int
Eintr(void)
{
	return errno==EINTR;
}

void
Noerror(void)
{
	errno=0;
}

int
Isatty(int fd)
{
	return isatty(fd);
}

void
Abort(void)
{
	abort();
}

int
Chdir(char *dir)
{
	return chdir(dir);
}

void
Prompt(char *s)
{
	pstr(err, s);
	flushio(err);
}
