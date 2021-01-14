#include <u.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "threadimpl.h"

#undef waitpid
#undef pipe
#undef wait

static int sigpid;
static int threadpassfd = -1;
static int gotsigchld;

static void
child(void)
{
	int status, pid;
	struct rlimit rl;

	notedisable("sys: child");
	pid = waitpid(sigpid, &status, 0);
	if(pid < 0){
		fprint(2, "%s: wait: %r\n", argv0);
		_exit(97);
	}
	if(WIFEXITED(status))
		 _exit(WEXITSTATUS(status));
	if(WIFSIGNALED(status)){
		/*
		 * Make sure we don't scribble over the nice
		 * core file that our child just wrote out.
		 */
		rl.rlim_cur = 0;
		rl.rlim_max = 0;
		setrlimit(RLIMIT_CORE, &rl);

		signal(WTERMSIG(status), SIG_DFL);
		raise(WTERMSIG(status));
		_exit(98);	/* not reached */
	}
	if(WIFSTOPPED(status)){
		fprint(2, "%s: wait pid %d stopped\n", argv0, pid);
		return;
	}
#ifdef WIFCONTINUED
	if(WIFCONTINUED(status)){
		fprint(2, "%s: wait pid %d continued\n", argv0, pid);
		return;
	}
#endif
	fprint(2, "%s: wait pid %d status 0x%ux\n", argv0, pid, status);
	_exit(99);
}

static void
sigpass(int sig)
{
	if(sigpid == 1){
		gotsigchld = 1;
		return;
	}

	if(sig == SIGCHLD)
		child();
	else
		kill(sigpid, sig);
}

static int sigs[] =
{
	SIGHUP, SIGINT, SIGQUIT, SIGILL,
	SIGTRAP, SIGABRT, SIGBUS, SIGFPE,
	SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE,
	SIGALRM, SIGTERM, SIGCHLD, SIGSTOP,
	/*SIGTSTP, SIGTTIN, SIGTTOU,*/ SIGURG,
	SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF,
#ifdef SIGWINCH
	SIGWINCH,
#endif
#ifdef SIGIO
	SIGIO,
#endif
#ifdef SIGEMT
	SIGEMT,
#endif
#ifdef SIGPWR
	SIGPWR,
#endif
#ifdef SIGINFO
	SIGINFO,
#endif
	SIGSYS
};

void
_threadsetupdaemonize(void)
{
	int i, n, pid;
	int p[2];
	char buf[20];

	sigpid = 1;

	/*
	 * We've been told this program is likely to background itself.
	 * Put it in its own process group so that we don't get a SIGHUP
	 * when the parent exits.
	 */
	setpgrp();

	if(pipe(p) < 0)
		sysfatal("passer pipe: %r");

	/* hide these somewhere they won't cause harm */
	/* can't go too high: NetBSD max is 64, for example */
	if(dup(p[0], 28) < 0 || dup(p[1], 29) < 0)
		sysfatal("passer pipe dup: %r");
	close(p[0]);
	close(p[1]);
	p[0] = 28;
	p[1] = 29;

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
		notedisable("sys: child");
		signal(SIGCHLD, SIG_DFL);
	/*	rfork(RFNOTEG); */
		close(p[0]);
		threadpassfd = p[1];
		return;
	}

	sigpid = pid;
	if(gotsigchld)
		sigpass(SIGCHLD);

	for(i=0; i<nelem(sigs); i++){
		struct sigaction sa;

		memset(&sa, 0, sizeof sa);
		sa.sa_handler = sigpass;
		sa.sa_flags |= SA_RESTART;
		sigaction(sigs[i], &sa, nil);
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
	if(threadpassfd < 0)
		sysfatal("threads in main proc exited w/o threadmaybackground");
	write(threadpassfd, "0", 1);
	close(threadpassfd);
	threadpassfd = -1;
}
