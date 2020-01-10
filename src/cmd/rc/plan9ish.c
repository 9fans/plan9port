/*
 * Plan 9 versions of system-specific functions
 *	By convention, exported routines herein have names beginning with an
 *	upper case letter.
 */
#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
#include "getflags.h"
char *Signame[]={
	"sigexit",	"sighup",	"sigint",	"sigquit",
	"sigalrm",	"sigkill",	"sigfpe",	"sigterm",
	0
};
char *syssigname[]={
	"exit",		/* can't happen */
	"hangup",
	"interrupt",
	"quit",		/* can't happen */
	"alarm",
	"kill",
	"sys: fp: ",
	"term",
	0
};
char*
Rcmain(void)
{
	return unsharp("#9/rcmain");
}

char Fdprefix[]="/dev/fd/";
long readnb(int, char *, long);
void execfinit(void);
void execbind(void);
void execmount(void);
void execulimit(void);
void execumask(void);
void execrfork(void);
builtin Builtin[]={
	"cd",		execcd,
	"whatis",	execwhatis,
	"eval",		execeval,
	"exec",		execexec,	/* but with popword first */
	"exit",		execexit,
	"shift",	execshift,
	"wait",		execwait,
	".",		execdot,
	"finit",	execfinit,
	"flag",		execflag,
	"ulimit",	execulimit,
	"umask",	execumask,
	"rfork",	execrfork,
	0
};

void
execrfork(void)
{
	int arg;
	char *s;

	switch(count(runq->argv->words)){
	case 1:
		arg = RFENVG|RFNOTEG|RFNAMEG;
		break;
	case 2:
		arg = 0;
		for(s = runq->argv->words->next->word;*s;s++) switch(*s){
		default:
			goto Usage;
		case 'n':
			arg|=RFNAMEG;  break;
		case 'N':
			arg|=RFCNAMEG;
			break;
		case 'e':
			/* arg|=RFENVG; */  break;
		case 'E':
			arg|=RFCENVG;  break;
		case 's':
			arg|=RFNOTEG;  break;
		case 'f':
			arg|=RFFDG;    break;
		case 'F':
			arg|=RFCFDG;   break;
		}
		break;
	default:
	Usage:
		pfmt(err, "Usage: %s [nNeEsfF]\n", runq->argv->words->word);
		setstatus("rfork usage");
		poplist();
		return;
	}
	if(rfork(arg)==-1){
		pfmt(err, "rc: %s failed\n", runq->argv->words->word);
		setstatus("rfork failed");
	}
	else
		setstatus("");
	poplist();
}



#define	SEP	'\1'
char **environp;
struct word *enval(s)
register char *s;
{
	register char *t, c;
	register struct word *v;
	for(t=s;*t && *t!=SEP;t++);
	c=*t;
	*t='\0';
	v=newword(s, c=='\0'?(struct word *)0:enval(t+1));
	*t=c;
	return v;
}
void Vinit(void){
	extern char **environ;
	register char *s;
	register char **env=environ;
	environp=env;
	for(;*env;env++){
		for(s=*env;*s && *s!='(' && *s!='=';s++);
		switch(*s){
		case '\0':
		/*	pfmt(err, "rc: odd environment %q?\n", *env); */
			break;
		case '=':
			*s='\0';
			setvar(*env, enval(s+1));
			*s='=';
			break;
		case '(':	/* ignore functions for now */
			break;
		}
	}
}
char **envp;
void Xrdfn(void){
	char *p;
	register char *s;
	register int len;
	for(;*envp;envp++){
		s = *envp;
		if(strncmp(s, "fn#", 3) == 0){
			p = strchr(s, '=');
			if(p == nil)
				continue;
			*p = ' ';
			s[2] = ' ';
			len = strlen(s);
			execcmds(opencore(s, len));
			s[len] = '\0';
			return;
		}
#if 0
		for(s=*envp;*s && *s!='(' && *s!='=';s++);
		switch(*s){
		case '\0':
			pfmt(err, "environment %q?\n", *envp);
			break;
		case '=':	/* ignore variables */
			break;
		case '(':		/* Bourne again */
			s=*envp+3;
			envp++;
			len=strlen(s);
			s[len]='\n';
			execcmds(opencore(s, len+1));
			s[len]='\0';
			return;
		}
#endif
	}
	Xreturn();
}
union code rdfns[4];
void execfinit(void){
	static int first=1;
	if(first){
		rdfns[0].i=1;
		rdfns[1].f=Xrdfn;
		rdfns[2].f=Xjump;
		rdfns[3].i=1;
		first=0;
	}
	Xpopm();
	envp=environp;
	start(rdfns, 1, runq->local);
}
extern int mapfd(int);
int Waitfor(int pid, int unused0){
	thread *p;
	Waitmsg *w;
	char errbuf[ERRMAX];

	if(pid >= 0 && !havewaitpid(pid))
		return 0;
	while((w = wait()) != nil){
		delwaitpid(w->pid);
		if(w->pid==pid){
			if(strncmp(w->msg, "signal: ", 8) == 0)
				fprint(mapfd(2), "%d: %s\n", w->pid, w->msg);
			setstatus(w->msg);
			free(w);
			return 0;
		}
		if(runq->iflag && strncmp(w->msg, "signal: ", 8) == 0)
			fprint(2, "%d: %s\n", w->pid, w->msg);
		for(p=runq->ret;p;p=p->ret)
			if(p->pid==w->pid){
				p->pid=-1;
				strcpy(p->status, w->msg);
			}
		free(w);
	}

	rerrstr(errbuf, sizeof errbuf);
	if(strcmp(errbuf, "interrupted")==0) return -1;
	return 0;
}
char **mkargv(word *a)
{
	char **argv=(char **)emalloc((count(a)+2)*sizeof(char *));
	char **argp=argv+1;	/* leave one at front for runcoms */
	for(;a;a=a->next) *argp++=a->word;
	*argp=0;
	return argv;
}
/*
void addenv(var *v)
{
	char envname[256];
	word *w;
	int f;
	io *fd;
	if(v->changed){
		v->changed=0;
		snprint(envname, sizeof envname, "/env/%s", v->name);
		if((f=Creat(envname))<0)
			pfmt(err, "rc: can't open %s: %r\n", envname);
		else{
			for(w=v->val;w;w=w->next)
				write(f, w->word, strlen(w->word)+1L);
			close(f);
		}
	}
	if(v->fnchanged){
		v->fnchanged=0;
		snprint(envname, sizeof envname, "/env/fn#%s", v->name);
		if((f=Creat(envname))<0)
			pfmt(err, "rc: can't open %s: %r\n", envname);
		else{
			if(v->fn){
				fd=openfd(f);
				pfmt(fd, "fn %s %s\n", v->name, v->fn[v->pc-1].s);
				closeio(fd);
			}
			close(f);
		}
	}
}
void updenvlocal(var *v)
{
	if(v){
		updenvlocal(v->next);
		addenv(v);
	}
}
void Updenv(void){
	var *v, **h;
	for(h=gvar;h!=&gvar[NVAR];h++)
		for(v=*h;v;v=v->next)
			addenv(v);
	if(runq) updenvlocal(runq->local);
}
*/
int
cmpenv(const void *a, const void *b)
{
	return strcmp(*(char**)a, *(char**)b);
}
char **mkenv(){
	register char **env, **ep, *p, *q;
	register struct var **h, *v;
	register struct word *a;
	register int nvar=0, nchr=0, sep;
	/*
	 * Slightly kludgy loops look at locals then globals
	 */
	for(h=gvar-1;h!=&gvar[NVAR];h++) for(v=h>=gvar?*h:runq->local;v;v=v->next){
		if((v==vlook(v->name)) && v->val){
			nvar++;
			nchr+=strlen(v->name)+1;
			for(a=v->val;a;a=a->next)
				nchr+=strlen(a->word)+1;
		}
		if(v->fn){
			nvar++;
			nchr+=strlen(v->name)+strlen(v->fn[v->pc-1].s)+8;
		}
	}
	env=(char **)emalloc((nvar+1)*sizeof(char *)+nchr);
	ep=env;
	p=(char *)&env[nvar+1];
	for(h=gvar-1;h!=&gvar[NVAR];h++) for(v=h>=gvar?*h:runq->local;v;v=v->next){
		if((v==vlook(v->name)) && v->val){
			*ep++=p;
			q=v->name;
			while(*q) *p++=*q++;
			sep='=';
			for(a=v->val;a;a=a->next){
				*p++=sep;
				sep=SEP;
				q=a->word;
				while(*q) *p++=*q++;
			}
			*p++='\0';
		}
		if(v->fn){
			*ep++=p;
#if 0
			*p++='#'; *p++='('; *p++=')';	/* to fool Bourne */
			*p++='f'; *p++='n'; *p++=' ';
			q=v->name;
			while(*q) *p++=*q++;
			*p++=' ';
#endif
			*p++='f'; *p++='n'; *p++='#';
			q=v->name;
			while(*q) *p++=*q++;
			*p++='=';
			q=v->fn[v->pc-1].s;
			while(*q) *p++=*q++;
			*p++='\n';
			*p++='\0';
		}
	}
	*ep=0;
	qsort((char *)env, nvar, sizeof ep[0], cmpenv);
	return env;
}
void Updenv(void){}
void Execute(word *args, word *path)
{
	char **argv=mkargv(args);
	char **env=mkenv();
	char file[1024];
	int nc;
	Updenv();
	for(;path;path=path->next){
		nc=strlen(path->word);
		if(nc<1024){
			strcpy(file, path->word);
			if(file[0]){
				strcat(file, "/");
				nc++;
			}
			if(nc+strlen(argv[1])<1024){
				strcat(file, argv[1]);
				execve(file, argv+1, env);
			}
			else werrstr("command name too long");
		}
	}
	rerrstr(file, sizeof file);
	pfmt(err, "%s: %s\n", argv[1], file);
	efree((char *)argv);
}
#define	NDIR	256		/* shoud be a better way */
int Globsize(char *p)
{
	ulong isglob=0, globlen=NDIR+1;
	for(;*p;p++){
		if(*p==GLOB){
			p++;
			if(*p!=GLOB) isglob++;
			globlen+=*p=='*'?NDIR:1;
		}
		else
			globlen++;
	}
	return isglob?globlen:0;
}
#define	NFD	50
#define	NDBUF	32
struct{
	Dir	*dbuf;
	int	i;
	int	n;
}dir[NFD];
int Opendir(char *name)
{
	Dir *db;
	int f;
	f=open(name, 0);
	if(f==-1)
		return f;
	db = dirfstat(f);
	if(db!=nil && (db->mode&DMDIR)){
		if(f<NFD){
			dir[f].i=0;
			dir[f].n=0;
		}
		free(db);
		return f;
	}
	free(db);
	close(f);
	return -1;
}
int Readdir(int f, char *p, int onlydirs)
{
	int n;
	USED(onlydirs);	/* only advisory */

	if(f<0 || f>=NFD)
		return 0;
	if(dir[f].i==dir[f].n){	/* read */
		free(dir[f].dbuf);
		dir[f].dbuf=0;
		n=dirread(f, &dir[f].dbuf);
		if(n>=0)
			dir[f].n=n;
		else
			dir[f].n=0;
		dir[f].i=0;
	}
	if(dir[f].i==dir[f].n)
		return 0;
	strcpy(p, dir[f].dbuf[dir[f].i].name);
	dir[f].i++;
	return 1;
}
void Closedir(int f){
	if(f>=0 && f<NFD){
		free(dir[f].dbuf);
		dir[f].i=0;
		dir[f].n=0;
		dir[f].dbuf=0;
	}
	close(f);
}
int interrupted = 0;
void
notifyf(void *unused0, char *s)
{
	int i;
	for(i=0;syssigname[i];i++)
		if(strncmp(s, syssigname[i], strlen(syssigname[i]))==0){
			if(strncmp(s, "sys: ", 5)!=0){
				if(kidpid && !interrupted){
					interrupted=1;
					postnote(PNGROUP, kidpid, s);
				}
				interrupted = 1;
			}
			goto Out;
		}
	if(strcmp(s, "sys: window size change") != 0)
	if(strcmp(s, "sys: write on closed pipe") != 0)
	if(strcmp(s, "sys: child") != 0)
		pfmt(err, "rc: note: %s\n", s);
	noted(NDFLT);
	return;
Out:
	if(strcmp(s, "interrupt")!=0 || trap[i]==0){
		trap[i]++;
		ntrap++;
	}
	if(ntrap>=32){	/* rc is probably in a trap loop */
		pfmt(err, "rc: Too many traps (trap %s), aborting\n", s);
		abort();
	}
	noted(NCONT);
}
void Trapinit(void){
	notify(notifyf);
}
void Unlink(char *name)
{
	remove(name);
}
long Write(int fd, char *buf, long cnt)
{
	return write(fd, buf, (long)cnt);
}
long Read(int fd, char *buf, long cnt)
{
	int i;

	i = readnb(fd, buf, cnt);
	if(ntrap) dotrap();
	return i;
}
long Seek(int fd, long cnt, long whence)
{
	return seek(fd, cnt, whence);
}
int Executable(char *file)
{
	Dir *statbuf;
	int ret;

	statbuf = dirstat(file);
	if(statbuf == nil) return 0;
	ret = ((statbuf->mode&0111)!=0 && (statbuf->mode&DMDIR)==0);
	free(statbuf);
	return ret;
}
int Creat(char *file)
{
	return create(file, 1, 0666L);
}
int Dup(int a, int b){
	return dup(a, b);
}
int Dup1(int a){
	return dup(a, -1);
}
void Exit(char *stat)
{
	Updenv();
	setstatus(stat);
	exits(truestatus()?"":getstatus());
}
int Eintr(void){
	return interrupted;
}
void Noerror(void){
	interrupted=0;
}
int
Isatty(int fd){
	return isatty(fd);
}
void Abort(void){
	pfmt(err, "aborting\n");
	flush(err);
	Exit("aborting");
}
void Memcpy(char *a, char *b, long n)
{
	memmove(a, b, (long)n);
}
void *Malloc(ulong n){
	return malloc(n);
}

int
exitcode(char *msg)
{
	int n;

	n = atoi(msg);
	if(n == 0)
		n = 1;
	return n;
}

int *waitpids;
int nwaitpids;

void
addwaitpid(int pid)
{
	waitpids = realloc(waitpids, (nwaitpids+1)*sizeof waitpids[0]);
	if(waitpids == 0)
		panic("Can't realloc %d waitpids", nwaitpids+1);
	waitpids[nwaitpids++] = pid;
}

void
delwaitpid(int pid)
{
	int r, w;

	for(r=w=0; r<nwaitpids; r++)
		if(waitpids[r] != pid)
			waitpids[w++] = waitpids[r];
	nwaitpids = w;
}

void
clearwaitpids(void)
{
	nwaitpids = 0;
}

int
havewaitpid(int pid)
{
	int i;

	for(i=0; i<nwaitpids; i++)
		if(waitpids[i] == pid)
			return 1;
	return 0;
}
