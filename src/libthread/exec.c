#include "u.h"
#include <errno.h>
#include "libc.h"
#include "thread.h"
#include "threadimpl.h"

static Lock thewaitlock;
static Channel *thewaitchan;
Channel	*_dowaitchan;
Channel	*_threadexecchan;
Proc	*_threadexecproc;
QLock	_threadexeclock;

static void
execthread(void *v)
{
	Execjob *e;

	USED(v);
	while((e = recvp(_threadexecchan)) != nil){
print("%d doexec pid %d\n", time(0), getpid());
		sendul(e->c, _threadspawn(e->fd, e->cmd, e->argv));
	}
}

static void
waitproc(void *v)
{
	Channel *c;
	Waitmsg *w;

	_threadsetsysproc();
	_threadexecproc = proc();
	threadcreate(execthread, nil, 65536);
	for(;;){
		while((w = wait()) == nil)
			if(errno == ECHILD)
				recvul(_dowaitchan);
		if((c = thewaitchan) != nil)
			sendp(c, w);
		else
			free(w);
	}
}

Channel*
threadwaitchan(void)
{
	if(thewaitchan)
		return thewaitchan;
	lock(&thewaitlock);
	if(thewaitchan){
		unlock(&thewaitlock);
		return thewaitchan;
	}
	thewaitchan = chancreate(sizeof(Waitmsg*), 4);
	chansetname(thewaitchan, "threadwaitchan");
	unlock(&thewaitlock);
	return thewaitchan;
}

int
_threadspawn(int fd[3], char *cmd, char *argv[])
{
	int i, n, p[2], pid;
	char exitstr[100];

	if(pipe(p) < 0)
		return -1;
	if(fcntl(p[0], F_SETFD, 1) < 0 || fcntl(p[1], F_SETFD, 1) < 0){
		close(p[0]);
		close(p[1]);
		return -1;
	}
	switch(pid = fork()){
	case -1:
		close(p[0]);
		close(p[1]);
		return -1;
	case 0:
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		dup2(fd[2], 2);
		for(i=3; i<100; i++)
			if(i != p[1])
				close(i);
		execvp(cmd, argv);
		fprint(p[1], "%d", errno);
		close(p[1]);
		_exit(0);
	}

	close(p[1]);
	n = read(p[0], exitstr, sizeof exitstr-1);
	close(p[0]);
	if(n > 0){	/* exec failed */
		exitstr[n] = 0;
		errno = atoi(exitstr);
		return -1;
	}

	close(fd[0]);
	if(fd[1] != fd[0])
		close(fd[1]);
	if(fd[2] != fd[1] && fd[2] != fd[0])
		close(fd[2]);
	channbsendul(_dowaitchan, 1);
	return pid;
}

int
threadspawn(int fd[3], char *cmd, char *argv[])
{
	if(_dowaitchan == nil){
		lock(&thewaitlock);
		if(_dowaitchan == nil){
			_dowaitchan = chancreate(sizeof(ulong), 1);
			chansetname(_dowaitchan, "dowaitchan");
			_threadexecchan = chancreate(sizeof(void*), 1);
			chansetname(_threadexecchan, "execchan");
			proccreate(waitproc, nil, STACK);
		}
		unlock(&thewaitlock);
	}
	return _runthreadspawn(fd, cmd, argv);
}


int
_threadexec(Channel *cpid, int fd[3], char *cmd, char *argv[])
{
	int pid;

	pid = threadspawn(fd, cmd, argv);
	if(cpid){
		if(pid < 0)
			chansendul(cpid, ~0);
		else
			chansendul(cpid, pid);
	}
	return pid;
}

void
threadexec(Channel *cpid, int fd[3], char *cmd, char *argv[])
{
	if(_threadexec(cpid, fd, cmd, argv) >= 0)
		threadexits("threadexec");
}

void
threadexecl(Channel *cpid, int fd[3], char *cmd, ...)
{
	char **argv, *s;
	int n, pid;
	va_list arg;

	va_start(arg, cmd);
	for(n=0; va_arg(arg, char*) != nil; n++)
		;
	n++;
	va_end(arg);

	argv = malloc(n*sizeof(argv[0]));
	if(argv == nil){
		if(cpid)
			chansendul(cpid, ~0);
		return;
	}

	va_start(arg, cmd);
	for(n=0; (s=va_arg(arg, char*)) != nil; n++)
		argv[n] = s;
	argv[n] = 0;
	va_end(arg);

	pid = _threadexec(cpid, fd, cmd, argv);
	free(argv);

	if(pid >= 0)
		threadexits("threadexecl");
}

