#include "threadimpl.h"

#define PIPEMNT	"/mnt/temp"

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
	if(bind("#|", PIPEMNT, MREPL) < 0)
		goto Bad;
	if((p->exec.fd[0] = open(PIPEMNT "/data", OREAD)) < 0){
		unmount(nil, PIPEMNT);
		goto Bad;
	}
	if((p->exec.fd[1] = open(PIPEMNT "/data1", OWRITE|OCEXEC)) < 0){
		close(p->exec.fd[0]);
		unmount(nil, PIPEMNT);
		goto Bad;
	}
	unmount(nil, PIPEMNT);

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

	/* wait for exec'ed program, then exit */
	_schedexecwait();
}

void
procexecl(Channel *pidc, int fd[3], char *f, ...)
{
	procexec(pidc, fd, f, &f+1);
}

