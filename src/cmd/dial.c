#include <u.h>
#include <libc.h>

void
usage(void)
{
	fprint(2, "usage: dial [-e] addr\n");
	exits("usage");
}

void
killer(void *x, char *msg)
{
	USED(x);
	if(strcmp(msg, "kill") == 0)
		exits(0);
	noted(NDFLT);
}

void
main(int argc, char **argv)
{
	int fd, pid;
	char buf[8192];
	int n, waitforeof;

	notify(killer);
	waitforeof = 0;
	ARGBEGIN{
	case 'e':
		waitforeof = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if((fd = dial(argv[0], nil, nil, nil)) < 0)
		sysfatal("dial: %r");

	switch(pid = fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		while((n = read(0, buf, sizeof buf)) > 0)
			if(write(fd, buf, n) < 0)
				break;
		if(!waitforeof)
			postnote(PNPROC, getppid(), "kill");
		exits(nil);
	}

	while((n = read(fd, buf, sizeof buf)) > 0)
		if(write(1, buf, n) < 0)
			break;

	postnote(PNPROC, pid, "kill");
	waitpid();
	exits(0);
}
