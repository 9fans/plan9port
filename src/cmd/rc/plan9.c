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

static void execrfork(void);
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
	"rfork",	execrfork,
	0
};

char Rcmain[]="/rc/lib/rcmain";
char Fdprefix[]="/fd/";

char *Signame[] = {
	"sigexit",	"sighup",	"sigint",	"sigquit",
	"sigalrm",	"sigkill",	"sigfpe",	"sigterm",
	0
};
static char *syssigname[] = {
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

/*
 * finit could be removed but is kept for
 * backwards compatibility, see: rcmain.plan9
 */
static void
execfinit(void)
{
	char *cmds = estrdup("for(i in '/env/fn#'*){. -bq $i}\n");
	int line = runq->line;
	poplist();
	execcmds(openiocore(cmds, strlen(cmds)), estrdup(srcfile(runq)), runq->local, runq->redir);
	runq->lex->line = line;
	runq->lex->qflag = 1;
}

static void
execrfork(void)
{
	int arg;
	char *s;

	switch(count(runq->argv->words)){
	case 1:
		arg = RFENVG|RFNAMEG|RFNOTEG;
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
		case 'm':
			arg|=RFNOMNT;  break;
		case 'e':
			arg|=RFENVG;   break;
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
		pfmt(err, "Usage: %s [fnesFNEm]\n", runq->argv->words->word);
		setstatus("rfork usage");
		poplist();
		return;
	}
	if(rfork(arg)==-1){
		pfmt(err, "%s: %s failed\n", argv0, runq->argv->words->word);
		setstatus("rfork failed");
	} else {
		if(arg & RFCFDG){
			redir *rp;
			for(rp = runq->redir; rp; rp = rp->next)
				rp->type = 0;
		}
		setstatus("");
	}
	poplist();
}

char*
Env(char *name, int fn)
{
	static char buf[128];

	strcpy(buf, "/env/");
	if(fn) strcat(buf, "fn#");
	return strncat(buf, name, sizeof(buf)-1);
}

void
Vinit(void)
{
	int dir, fd, i, n;
	Dir *ent;

	dir = Open(Env("", 0), 0);
	if(dir<0){
		pfmt(err, "%s: can't open: %s\n", argv0, Errstr());
		return;
	}
	for(;;){
		ent = 0;
		n = dirread(dir, &ent);
		if(n <= 0)
			break;
		for(i = 0; i<n; i++){
			if(ent[i].length<=0 || strncmp(ent[i].name, "fn#", 3)==0)
				continue;
			if((fd = Open(Env(ent[i].name, 0), 0))>=0){
				io *f = openiofd(fd);
				word *w = 0, **wp = &w;
				char *s;
				while((s = rstr(f, "")) != 0){
					*wp = Newword(s, (word*)0);
					wp = &(*wp)->next;
				}
				closeio(f);
				setvar(ent[i].name, w);
				vlook(ent[i].name)->changed = 0;
			}
		}
		free(ent);
	}
	Close(dir);
}

char*
Errstr(void)
{
	static char err[ERRMAX];
	rerrstr(err, sizeof err);
	return err;
}

int
Waitfor(int pid)
{
	thread *p;
	Waitmsg *w;

	if(pid >= 0 && !havewaitpid(pid))
		return 0;

	while((w = wait()) != nil){
		delwaitpid(w->pid);
		if(w->pid==pid){
			setstatus(w->msg);
			free(w);
			return 0;
		}
		for(p = runq->ret;p;p = p->ret)
			if(p->pid==w->pid){
				p->pid=-1;
				p->status = estrdup(w->msg);
				break;
			}
		free(w);
	}

	if(strcmp(Errstr(), "interrupted")==0) return -1;
	return 0;
}

static void
addenv(var *v)
{
	word *w;
	int fd;
	io *f;

	if(v->changed){
		v->changed = 0;
		if((fd = Creat(Env(v->name, 0)))<0)
			pfmt(err, "%s: can't open: %s\n", argv0, Errstr());
		else{
			f = openiofd(fd);
			for(w = v->val;w;w = w->next){
				pstr(f, w->word);
				pchr(f, '\0');
			}
			flushio(f);
			closeio(f);
		}
	}
	if(v->fnchanged){
		v->fnchanged = 0;
		if((fd = Creat(Env(v->name, 1)))<0)
			pfmt(err, "%s: can't open: %s\n", argv0, Errstr());
		else{
			f = openiofd(fd);
			if(v->fn)
				pfmt(f, "fn %q %s\n", v->name, v->fn[v->pc-1].s);
			flushio(f);
			closeio(f);
		}
	}
}

static void
updenvlocal(var *v)
{
	if(v){
		updenvlocal(v->next);
		addenv(v);
	}
}

void
Updenv(void)
{
	var *v, **h;
	for(h = gvar;h!=&gvar[NVAR];h++)
		for(v=*h;v;v = v->next)
			addenv(v);
	if(runq)
		updenvlocal(runq->local);
	if(err)
		flushio(err);
}

void
Exec(char **argv)
{
	exec(argv[0], argv+1);
}

int
Fork(void)
{
	Updenv();
	return rfork(RFPROC|RFFDG|RFREND);
}


typedef struct readdir readdir;
struct readdir {
	Dir	*dbuf;
	int	i, n;
	int	fd;
};

void*
Opendir(char *name)
{
	readdir *rd;
	int fd;
	if((fd = Open(name, 0))<0)
		return 0;
	rd = new(readdir);
	rd->dbuf = 0;
	rd->i = 0;
	rd->n = 0;
	rd->fd = fd;
	return rd;
}

static int
trimdirs(Dir *d, int nd)
{
	int r, w;

	for(r=w=0; r<nd; r++)
		if(d[r].mode&DMDIR)
			d[w++] = d[r];
	return w;
}

char*
Readdir(void *arg, int onlydirs)
{
	readdir *rd = arg;
	int n;
Again:
	if(rd->i>=rd->n){	/* read */
		free(rd->dbuf);
		rd->dbuf = 0;
		n = dirread(rd->fd, &rd->dbuf);
		if(n>0){
			if(onlydirs){
				n = trimdirs(rd->dbuf, n);
				if(n == 0)
					goto Again;
			}	
			rd->n = n;
		}else
			rd->n = 0;
		rd->i = 0;
	}
	if(rd->i>=rd->n)
		return 0;
	return rd->dbuf[rd->i++].name;
}

void
Closedir(void *arg)
{
	readdir *rd = arg;
	Close(rd->fd);
	free(rd->dbuf);
	free(rd);
}

static int interrupted = 0;

static void
notifyf(void*, char *s)
{
	int i;

	for(i = 0;syssigname[i];i++) if(strncmp(s, syssigname[i], strlen(syssigname[i]))==0){
		if(strncmp(s, "sys: ", 5)!=0) interrupted = 1;
		goto Out;
	}
	noted(NDFLT);
	return;
Out:
	if(strcmp(s, "interrupt")!=0 || trap[i]==0){
		trap[i]++;
		ntrap++;
	}
	noted(NCONT);
}

void
Trapinit(void)
{
	notify(notifyf);
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
	return seek(fd, cnt, whence);
}

int
Executable(char *file)
{
	Dir *statbuf;
	int ret;

	statbuf = dirstat(file);
	if(statbuf == nil)
		return 0;
	ret = ((statbuf->mode&0111)!=0 && (statbuf->mode&DMDIR)==0);
	free(statbuf);
	return ret;
}

int
Open(char *file, int mode)
{
	static int tab[] = {OREAD,OWRITE,ORDWR,OREAD|ORCLOSE};
	return open(file, tab[mode&3]);
}

void
Close(int fd)
{
	close(fd);
}

int
Creat(char *file)
{
	return create(file, OWRITE, 0666L);
}

int
Dup(int a, int b)
{
	return dup(a, b);
}

int
Dup1(int a)
{
	return dup(a, -1);
}

void
Exit(void)
{
	Updenv();
	exits(truestatus()?"":getstatus());
}

int
Eintr(void)
{
	return interrupted;
}

void
Noerror(void)
{
	interrupted = 0;
}

int
Isatty(int fd)
{
	char buf[64];

	if(fd2path(fd, buf, sizeof buf) != 0)
		return 0;
	/* might be /mnt/term/dev/cons */
	return strlen(buf) >= 9 && strcmp(buf+strlen(buf)-9, "/dev/cons") == 0;
}

void
Abort(void)
{
	abort();
}

static int newwdir;

int
Chdir(char *dir)
{
	newwdir = 1;
	return chdir(dir);
}

void
Prompt(char *s)
{
	pstr(err, s);
	flushio(err);

	if(newwdir){
		char dir[4096];
		int fd;
		if((fd=Creat("/dev/wdir"))>=0){
			getwd(dir, sizeof(dir));
			Write(fd, dir, strlen(dir));
			Close(fd);
		}
		newwdir = 0;
	}
}
