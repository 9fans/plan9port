#include <u.h>
#include <signal.h>
#include <libc.h>
#include "term.h"

static void
sys(char *buf)
{
	char buf2[100];
	char *f[20];
	int nf, pid;

	strcpy(buf2, buf);
	nf = tokenize(buf2, f, nelem(f));
	f[nf] = nil;
	switch(pid = fork()){
	case 0:
		execvp(f[0], f);
		_exits("oops");
	default:
		waitpid();
	}
}

int
rcstart(int argc, char **argv, int *pfd, int *tfd)
{
	int fd[2], i, pid;
	char *xargv[3];
	char slave[256];
	int sfd;

	if(argc == 0){
		argc = 2;
		argv = xargv;
		argv[0] = getenv("SHELL");
		if(argv[0] == 0)
			argv[0] = "rc";
		argv[1] = "-i";
		argv[2] = 0;
	}
	/*
	 * fd0 is slave (tty), fd1 is master (pty)
	 */
	fd[0] = fd[1] = -1;
	if(getpts(fd, slave) < 0)
		sysfatal("getpts: %r\n");
	switch(pid = fork()) {
	case 0:
		putenv("TERM", "9term");
		sfd = childpty(fd, slave);
		dup(sfd, 0);
		dup(sfd, 1);
		dup(sfd, 2);
		sys("stty tabs -onlcr onocr icanon echo erase '^h' intr '^?'");
		if(noecho)
			sys("stty -echo");
		for(i=3; i<100; i++)
			close(i);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		execvp(argv[0], argv);
		fprint(2, "exec %s failed: %r\n", argv[0]);
		_exits("oops");
		break;
	case -1:
		sysfatal("proc failed: %r");
		break;
	}
	*pfd = fd[1];
	close(fd[0]);
	if(tfd){
		if((*tfd = open(slave, OREAD)) < 0)
			sysfatal("parent open %s: %r", slave);
	}
	return pid;
}

