#include <fcntl.h>
#include <unistd.h>
#include "threadimpl.h"

void
procexec(Channel *pidc, int fd[3], char *prog, char *args[])
{
	int n;
	Proc *p;
	Thread *t;

	_threaddebug(DBGEXEC, "procexec %s", prog);
	/* must be only thread in proc */
	p = _threadgetproc();
	t = p->thread;
	if(p->threads.head != t || p->threads.head->nextt != nil){
		werrstr("not only thread in proc");
	Bad:
		_threaddebug(DBGEXEC, "procexec bad %r");
		if(pidc)
			sendul(pidc, ~0);
		return;
	}

	/*
	 * We want procexec to behave like exec; if exec succeeds,
	 * never return, and if it fails, return with errstr set.
	 * Unfortunately, the exec happens in another proc since
	 * we have to wait for the exec'ed process to finish.
	 * To provide the semantics, we open a pipe with the 
	 * write end close-on-exec and hand it to the proc that
	 * is doing the exec.  If the exec succeeds, the pipe will
	 * close so that our read below fails.  If the exec fails,
	 * then the proc doing the exec sends the errstr down the
	 * pipe to us.
	 */
	if(pipe(p->exec.fd) < 0)
		goto Bad;
	if(fcntl(p->exec.fd[0], F_SETFD, 1) < 0)
		goto Bad;
	if(fcntl(p->exec.fd[1], F_SETFD, 1) < 0)
		goto Bad;

	/* exec in parallel via the scheduler */
	assert(p->needexec==0);
	p->exec.prog = prog;
	p->exec.args = args;
	p->exec.stdfd = fd;
	p->needexec = 1;
	_sched();

	close(p->exec.fd[1]);
	if((n = read(p->exec.fd[0], p->exitstr, ERRMAX-1)) > 0){	/* exec failed */
		p->exitstr[n] = '\0';
		errstr(p->exitstr, ERRMAX);
		close(p->exec.fd[0]);
		goto Bad;
	}
	close(p->exec.fd[0]);
	close(fd[0]);
	if(fd[1] != fd[0])
		close(fd[1]);
	if(fd[2] != fd[1] && fd[2] != fd[0])
		close(fd[2]);
	if(pidc)
		sendul(pidc, t->ret);

	_threaddebug(DBGEXEC, "procexec schedexecwait");
	/* wait for exec'ed program, then exit */
	_schedexecwait();
}

void
procexecl(Channel *pidc, int fd[3], char *f, ...)
{
	procexec(pidc, fd, f, &f+1);
}

void
_schedexecwait(void)
{
	int pid;
	Channel *c;
	Proc *p;
	Thread *t;
	Waitmsg *w;

	p = _threadgetproc();
	t = p->thread;
	pid = t->ret;
	_threaddebug(DBGEXEC, "_schedexecwait %d", t->ret);

	for(;;){
		w = wait();
		if(w == nil)
			break;
		if(w->pid == pid)
			break;
		free(w);
	}
	if(w != nil){
		if((c = _threadwaitchan) != nil)
			sendp(c, w);
		else
			free(w);
	}
	threadexits("procexec");
}

static void
efork(void *ve)
{
	char buf[ERRMAX];
	Execargs *e;
	int i;

	e = ve;
	_threaddebug(DBGEXEC, "_schedexec %s -- calling execv", e->prog);
	dup(e->stdfd[0], 0);
	dup(e->stdfd[1], 1);
	dup(e->stdfd[2], 2);
	for(i=3; i<40; i++)
		if(i != e->fd[1])
			close(i);
	rfork(RFNOTEG);
	execvp(e->prog, e->args);
	_threaddebug(DBGEXEC, "_schedexec failed: %r");
	rerrstr(buf, sizeof buf);
	if(buf[0]=='\0')
		strcpy(buf, "exec failed");
	write(e->fd[1], buf, strlen(buf));
	close(e->fd[1]);
	_exits(buf);
}

int
_schedexec(Execargs *e)
{
	int pid;

	pid = fork();
	if(pid == 0){
		efork(e);
		_exit(1);
	}
	return pid;
}
