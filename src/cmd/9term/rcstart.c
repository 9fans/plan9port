#include <u.h>
#include <libc.h>
#if 0
#include <sys/wait.h>
#endif
#include <signal.h>
#include "term.h"

/*
 * Somehow we no longer automatically exit
 * when the shell exits; hence the SIGCHLD stuff.
 * Something that can be fixed? Axel.
 */
static int pid;

int
rcstart(int argc, char **argv, int *pfd, int *tfd)
{
	int fd[2];
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
		system("stty tabs -onlcr -echo erase '^h' intr '^?'");
		execvp(argv[0], argv);
		fprint(2, "exec %s failed: %r\n", argv[0]);
		_exits("oops");
		break;
	case -1:
		sysfatal("proc failed: %r");
		break;
	}
	*pfd = fd[1];
	if(tfd)
		*tfd = fd[0];
	else
		close(fd[0]);
	return pid;
}

