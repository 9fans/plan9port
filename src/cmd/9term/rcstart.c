#include <u.h>
#include <libc.h>
#include "term.h"

int
rcstart(int argc, char **argv, int *pfd)
{
	int pid;
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
	close(fd[0]);
	*pfd = fd[1];
	return pid;
}

