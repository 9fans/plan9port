#include <lib9.h>
#include <thread.h>
extern int _threaddebuglevel;

void
doexec(void *v)
{
	int fd[3];
	char **argv = v;

	fd[0] = dup(0, -1);
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	threadexec(nil, fd, argv[0], argv);
	print("exec failed: %r\n");
	sendp(threadwaitchan(), nil);
	threadexits(nil);
}

void
threadmain(int argc, char **argv)
{
	Channel *c;
	Waitmsg *w;
	int (*mk)(void(*)(void*), void*, uint);

	mk = threadcreate;
	ARGBEGIN{
	case 'D':
		_threaddebuglevel = ~0;
		break;
	case 'p':
		mk = proccreate;
		break;
	}ARGEND

	c = threadwaitchan();
	mk(doexec, argv, 8192);
	w = recvp(c);
	if(w == nil)
		print("exec/recvp failed: %r\n");
	else
		print("%d %lud %lud %lud %s\n", w->pid, w->time[0], w->time[1], w->time[2], w->msg);
	threadexits(nil);
}
