#include <u.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"

#undef pipe
#undef wait

static int sigpid;
static int threadpassfd;

static void
child(void)
{
	int status;
	if(wait(&status) == sigpid && WIFEXITED(status))
		 _exit(WEXITSTATUS(status));
}

static void
sigpass(int sig)
{
	if(sig == SIGCHLD)
		child();
	else
		kill(sigpid, sig);
}

void
_threadsetupdaemonize(void)
{
	int i, n, pid;
	int p[2];
	char buf[20];

	sigpid = 1;

	threadlinklibrary();

	if(pipe(p) < 0)
		sysfatal("passer pipe: %r");

	/* hide these somewhere they won't cause harm */
	if(dup(p[0], 98) < 0 || dup(p[1], 99) < 0)
		sysfatal("passer pipe dup: %r");
	close(p[0]);
	close(p[1]);
	p[0] = 98;
	p[1] = 99;

	/* close on exec */
	if(fcntl(p[0], F_SETFD, 1) < 0 || fcntl(p[1], F_SETFD, 1) < 0)
		sysfatal("passer pipe pipe fcntl: %r");

	switch(pid = fork()){
	case -1:
		sysfatal("passer fork: %r");
	default:
		close(p[1]);
		break;
	case 0:
		rfork(RFNOTEG);
		close(p[0]);
		threadpassfd = p[1];
		return;
	}

	sigpid = pid;
	for(i=0; i<NSIG; i++){
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_handler = sigpass;
		sa.sa_flags |= SA_RESTART;
		sigaction(i, &sa, nil);
	}

	for(;;){
		n = read(p[0], buf, sizeof buf-1);
		if(n == 0)	/* program exited */
			child();
		if(n > 0)
			break;
		sysfatal("passer pipe read: %r");
	}
	buf[n] = 0;
	_exit(atoi(buf));
}

void
threaddaemonize(void)
{
	write(threadpassfd, "0", 1);
	close(threadpassfd);
	threadpassfd = -1;
}
