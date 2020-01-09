#include "u.h"
#include "libc.h"
#include "thread.h"

void
execproc(void *v)
{
	int i, fd[3];
	char buf[100], *args[3];

	i = (int)(uintptr)v;
	sprint(buf, "%d", i);
	fd[0] = dup(0, -1);
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	args[0] = "echo";
	args[1] = buf;
	args[2] = nil;
	threadexec(nil, fd, args[0], args);
}

void
threadmain(int argc, char **argv)
{
	int i;
	Channel *c;
	Waitmsg *w;

	ARGBEGIN{
	case 'D':
		break;
	}ARGEND

	c = threadwaitchan();
	for(i=0;; i++){
		proccreate(execproc, (void*)(uintptr)i, 16384);
		w = recvp(c);
		if(w == nil)
			sysfatal("exec/recvp failed: %r");
	}
}
