#include <u.h>
#include <fcntl.h>
#include <unistd.h>
#include "threadimpl.h"

static void efork(int[3], int[2], char*, char**);
int
_threadexec(Channel *pidc, int fd[3], char *prog, char *args[], int freeargs)
{
	int pfd[2];
	int n, pid;
	char exitstr[ERRMAX];
	static int firstexec = 1;
	static Lock lk;

	_threaddebug(DBGEXEC, "threadexec %s", prog);

	if(firstexec){
		lock(&lk);
		if(firstexec){
			firstexec = 0;
			_threadfirstexec();
		}
		unlock(&lk);
	}

	/*
	 * We want threadexec to behave like exec; if exec succeeds,
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
	if(pipe(pfd) < 0)
		goto Bad;
	if(fcntl(pfd[0], F_SETFD, 1) < 0)
		goto Bad;
	if(fcntl(pfd[1], F_SETFD, 1) < 0)
		goto Bad;

	switch(pid = fork()){
	case -1:
		close(pfd[0]);
		close(pfd[1]);
		goto Bad;
	case 0:
		efork(fd, pfd, prog, args);
		_threaddebug(DBGSCHED, "exit after efork");
		_exit(0);
	default:
		_threadafterexec();
		if(freeargs)
			free(args);
		break;
	}

	close(pfd[1]);
	if((n = read(pfd[0], exitstr, ERRMAX-1)) > 0){	/* exec failed */
		exitstr[n] = '\0';
		errstr(exitstr, ERRMAX);
		close(pfd[0]);
		goto Bad;
	}
	close(pfd[0]);
	close(fd[0]);
	if(fd[1] != fd[0])
		close(fd[1]);
	if(fd[2] != fd[1] && fd[2] != fd[0])
		close(fd[2]);
	if(pidc)
		sendul(pidc, pid);

	_threaddebug(DBGEXEC, "threadexec schedexecwait");
	return pid;

Bad:
	_threaddebug(DBGEXEC, "threadexec bad %r");
	if(pidc)
		sendul(pidc, ~0);
	return -1;
}

void
threadexec(Channel *pidc, int fd[3], char *prog, char *args[])
{
	if(_callthreadexec(pidc, fd, prog, args, 0) >= 0)
		threadexits(nil);
}

int
threadspawn(int fd[3], char *prog, char *args[])
{
	return _callthreadexec(nil, fd, prog, args, 0);
}

/*
 * The &f+1 trick doesn't work on SunOS, so we might 
 * as well bite the bullet and do this correctly.
 */
void
threadexecl(Channel *pidc, int fd[3], char *f, ...)
{
	char **args, *s;
	int n;
	va_list arg;

	va_start(arg, f);
	for(n=0; va_arg(arg, char*) != 0; n++)
		;
	n++;
	va_end(arg);

	args = malloc(n*sizeof(args[0]));
	if(args == nil){
		if(pidc)
			sendul(pidc, ~0);
		return;
	}

	va_start(arg, f);
	for(n=0; (s=va_arg(arg, char*)) != 0; n++)
		args[n] = s;
	args[n] = 0;
	va_end(arg);

	if(_callthreadexec(pidc, fd, f, args, 1) >= 0)
		threadexits(nil);
}

static void
efork(int stdfd[3], int fd[2], char *prog, char **args)
{
	char buf[ERRMAX];
	int i;

	_threaddebug(DBGEXEC, "_schedexec %s -- calling execv", prog);
	dup(stdfd[0], 0);
	dup(stdfd[1], 1);
	dup(stdfd[2], 2);
	for(i=3; i<40; i++)
		if(i != fd[1])
			close(i);
	rfork(RFNOTEG);
	execvp(prog, args);
	_threaddebug(DBGEXEC, "_schedexec failed: %r");
	rerrstr(buf, sizeof buf);
	if(buf[0]=='\0')
		strcpy(buf, "exec failed");
	write(fd[1], buf, strlen(buf));
	close(fd[1]);
	_threaddebug(DBGSCHED, "_exits in exec-unix");
	_exits(buf);
}

