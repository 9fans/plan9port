#include "threadimpl.h"

#undef waitpid
#undef pipe
#undef wait

static int sigpid;
static int threadpassfd;

static void
child(void)
{
	int status, pid;

	notedisable("sys: child");
	pid = waitpid(sigpid, &status, 0);
	if(pid < 0){
		fprint(2, "%s: wait: %r\n", argv0);
		_exit(97);
	}
	if(WIFEXITED(status))
		 _exit(WEXITSTATUS(status));
	if(WIFSIGNALED(status)){
		signal(WTERMSIG(status), SIG_DFL);
		raise(WTERMSIG(status));
		_exit(98);	/* not reached */
	}
	fprint(2, "%s: wait pid %d status 0x%ux\n", pid, status);
	_exit(99);
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

	noteenable("sys: child");
	signal(SIGCHLD, sigpass);
	switch(pid = fork()){
	case -1:
		sysfatal("passer fork: %r");
	default:
		close(p[1]);
		break;
	case 0:
		for(i=0; i<100; i++) sched_yield();
		notedisable("sys: child");
		signal(SIGCHLD, SIG_DFL);
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
		if(n == 0){	/* program exited */
			child();
		}
		if(n > 0)
			break;
		print("passer read: %r\n");
	}
	buf[n] = 0;
	_exit(atoi(buf));
}

void
_threaddaemonize(void)
{
	if(threadpassfd >= 0){
		write(threadpassfd, "0", 1);
		close(threadpassfd);
		threadpassfd = -1;
	}
}
