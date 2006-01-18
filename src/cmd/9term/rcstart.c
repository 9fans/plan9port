#include <u.h>
#include <signal.h>
#include <libc.h>
#include "term.h"

static void
sys(char *buf, int devnull)
{
	char buf2[100];
	char *f[20];
	int nf, pid;

	notedisable("sys: child");
	strcpy(buf2, buf);
	nf = tokenize(buf2, f, nelem(f));
	f[nf] = nil;
	switch(pid = fork()){
	case 0:
		close(1);
		open("/dev/null", OREAD);
		close(2);
		open("/dev/null", OREAD);
		execvp(f[0], f);
		_exit(2);
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
		argv[1] = "-l";
		argv[2] = 0;
	}
	/*
	 * fd0 is slave (tty), fd1 is master (pty)
	 */
	fd[0] = fd[1] = -1;
	if(getpts(fd, slave) < 0){
		exit(3);
		sysfatal("getpts: %r\n");
	}
	/*
	 * notedisable("sys: window size change");
	 * 
	 * Can't disable because will be inherited by other programs
	 * like if you run an xterm from the prompt, and then xterm's
	 * resizes won't get handled right.  Sigh.  
	 *
	 * Can't not disable because when we stty below we'll get a
	 * signal, which will drop us into the thread library note handler,
	 * which will get all confused because we just forked and thus
	 * have an unknown pid. 
	 *
	 * So disable it internally.  ARGH!
	 */
	notifyoff("sys: window size change");

	pid = fork();
	switch(pid){
	case 0:
		putenv("TERM", "9term");
		sfd = childpty(fd, slave);
		dup(sfd, 0);
		dup(sfd, 1);
		dup(sfd, 2);
		sys("stty tabs -onlcr icanon echo erase '^h' intr '^?'", 0);
		sys("stty onocr", 1);	/* not available on mac */
		if(noecho)
			sys("stty -echo", 0);
		for(i=3; i<100; i++)
			close(i);
		signal(SIGINT, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		execvp(argv[0], argv);
		fprint(2, "exec %s failed: %r\n", argv[0]);
		_exit(2);
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

