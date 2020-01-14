#define NOPLAN9DEFINES
#include	"mk.h"
#include	<sys/wait.h>
#include	<signal.h>
#include	<sys/stat.h>
#include	<sys/time.h>

char	*shell = "/bin/sh";
char	*shellname = "sh";

extern char **environ;

static void
mkperror(char *s)
{
	fprint(2, "%s: %r\n", s);
}

void
readenv(void)
{
	char **p, *s;
	Word *w;

	for(p = environ; *p; p++){
/* rsc 5/5/2004 -- This misparses fn#cd={whatever}
		s = shname(*p);
		if(*s == '=') {
			*s = 0;
			w = newword(s+1);
		} else
			w = newword("");
*/
		s = strchr(*p, '=');
		if(s){
			*s = 0;
			w = newword(s+1);
		} else
			w = newword("");
		if (symlook(*p, S_INTERNAL, 0))
			continue;
		s = strdup(*p);
		setvar(s, (void *)w);
		symlook(s, S_EXPORTED, (void*)"")->u.ptr = "";
	}
}

/*
 *	done on child side of fork, so parent's env is not affected
 *	and we don't care about freeing memory because we're going
 *	to exec immediately after this.
 */
void
exportenv(Envy *e, Shell *sh)
{
	int w, n;
	char **p;
	Envy *e1;
	static char buf[16384];

	n = 0;
	for(e1 = e; e1->name; e1++)
		n++;
	p = Malloc((n+1)*sizeof(char*));
	w = 0;
	for(; e->name; e++) {
		if(sh == &rcshell && (e->values == 0 || e->values->s == 0 || e->values->s[0] == 0))
			continue; /* do not write empty string for empty list */
		if(e->values)
			snprint(buf, sizeof buf, "%s=%s", e->name,  wtos(e->values, sh->iws));
		else
			snprint(buf, sizeof buf, "%s=", e->name);
		p[w++] = strdup(buf);
	}
	p[w] = 0;
	environ = p;
}

int
waitfor(char *msg)
{
	int status;
	int pid;

	*msg = 0;
	pid = wait(&status);
	if(pid > 0) {
		if(status&0x7f) {
			if(status&0x80)
				snprint(msg, ERRMAX, "signal %d, core dumped", status&0x7f);
			else
				snprint(msg, ERRMAX, "signal %d", status&0x7f);
		} else if(status&0xff00)
			snprint(msg, ERRMAX, "exit(%d)", (status>>8)&0xff);
	}
	return pid;
}

void
expunge(int pid, char *msg)
{
	if(strcmp(msg, "interrupt"))
		kill(pid, SIGINT);
	else
		kill(pid, SIGHUP);
}

int mypid;

int
shargv(Word *cmd, int extra, char ***pargv)
{
	char **argv;
	int i, n;
	Word *w;

	n = 0;
	for(w=cmd; w; w=w->next)
		n++;

	argv = Malloc((n+extra+1)*sizeof(argv[0]));
	i = 0;
	for(w=cmd; w; w=w->next)
		argv[i++] = w->s;
	argv[n] = 0;
	*pargv = argv;
	return n;
}

int
execsh(char *args, char *cmd, Bufblock *buf, Envy *e, Shell *sh, Word *shellcmd)
{
	char *p, **argv;
	int tot, n, pid, in[2], out[2];

	if(buf && pipe(out) < 0){
		mkperror("pipe");
		Exit();
	}
	pid = fork();
	mypid = getpid();
	if(pid < 0){
		mkperror("mk fork");
		Exit();
	}
	if(pid == 0){
		if(buf)
			close(out[0]);
		if(pipe(in) < 0){
			mkperror("pipe");
			Exit();
		}
		pid = fork();
		if(pid < 0){
			mkperror("mk fork");
			Exit();
		}
		if(pid != 0){
			dup2(in[0], 0);
			if(buf){
				dup2(out[1], 1);
				close(out[1]);
			}
			close(in[0]);
			close(in[1]);
			if (e)
				exportenv(e, sh);
			n = shargv(shellcmd, 1, &argv);
			argv[n++] = args;
			argv[n] = 0;
			execvp(argv[0], argv);
			mkperror(shell);
			_exit(1);
		}
		close(out[1]);
		close(in[0]);
		if(DEBUG(D_EXEC))
			fprint(1, "starting: %s\n", cmd);
		p = cmd+strlen(cmd);
		while(cmd < p){
			n = write(in[1], cmd, p-cmd);
			if(n < 0)
				break;
			cmd += n;
		}
		close(in[1]);
		_exit(0);
	}
	if(buf){
		close(out[1]);
		tot = 0;
		for(;;){
			if (buf->current >= buf->end)
				growbuf(buf);
			n = read(out[0], buf->current, buf->end-buf->current);
			if(n <= 0)
				break;
			buf->current += n;
			tot += n;
		}
		if (tot && buf->current[-1] == '\n')
			buf->current--;
		close(out[0]);
	}
	return pid;
}

int
pipecmd(char *cmd, Envy *e, int *fd, Shell *sh, Word *shellcmd)
{
	int pid, pfd[2];
	int n;
	char **argv;

	if(DEBUG(D_EXEC))
		fprint(1, "pipecmd='%s'\n", cmd);/**/

	if(fd && pipe(pfd) < 0){
		mkperror("pipe");
		Exit();
	}
	pid = fork();
	if(pid < 0){
		mkperror("mk fork");
		Exit();
	}
	if(pid == 0){
		if(fd){
			close(pfd[0]);
			dup2(pfd[1], 1);
			close(pfd[1]);
		}
		if(e)
			exportenv(e, sh);
		n = shargv(shellcmd, 2, &argv);
		argv[n++] = "-c";
		argv[n++] = cmd;
		argv[n] = 0;
		execvp(argv[0], argv);
		mkperror(shell);
		_exit(1);
	}
	if(fd){
		close(pfd[1]);
		*fd = pfd[0];
	}
	return pid;
}

void
Exit(void)
{
	while(wait(0) >= 0)
		;
	exits("error");
}

static	struct
{
	int	sig;
	char	*msg;
}	sigmsgs[] =
{
	SIGALRM,	"alarm",
	SIGFPE,		"sys: fp: fptrap",
	SIGPIPE,	"sys: write on closed pipe",
	SIGILL,		"sys: trap: illegal instruction",
/*	SIGSEGV,	"sys: segmentation violation", */
	0,		0
};

static void
notifyf(int sig)
{
	int i;

	for(i = 0; sigmsgs[i].msg; i++)
		if(sigmsgs[i].sig == sig)
			killchildren(sigmsgs[i].msg);

	/* should never happen */
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

void
catchnotes(void)
{
	int i;

	for(i = 0; sigmsgs[i].msg; i++)
		signal(sigmsgs[i].sig, notifyf);
}

char*
maketmp(int *pfd)
{
	static char temp[] = "/tmp/mkargXXXXXX";
	static char buf[100];
	int fd;

	strcpy(buf, temp);
	fd = mkstemp(buf);
	if(fd < 0)
		return 0;
	*pfd = fd;
	return buf;
}

int
chgtime(char *name)
{
	if(access(name, 0) >= 0)
		return utimes(name, 0);
	return close(creat(name, 0666));
}

void
rcopy(char **to, Resub *match, int n)
{
	int c;
	char *p;

	*to = match->s.sp;		/* stem0 matches complete target */
	for(to++, match++; --n > 0; to++, match++){
		if(match->s.sp && match->e.ep){
			p = match->e.ep;
			c = *p;
			*p = 0;
			*to = strdup(match->s.sp);
			*p = c;
		}
		else
			*to = 0;
	}
}

unsigned long
mkmtime(char *name)
{
	struct stat st;

	if(stat(name, &st) < 0)
		return 0;

	return st.st_mtime;
}
