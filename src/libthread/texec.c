#include <lib9.h>
#include <thread.h>
extern int _threaddebuglevel;

void
doexec(void *v)
{
	char **argv = v;

print("doexec\n");
	procexec(nil, argv[0], argv);
	sendp(threadwaitchan(), nil);
}

void
threadmain(int argc, char **argv)
{
	Channel *c;
	Waitmsg *w;

	ARGBEGIN{
	case 'D':
		_threaddebuglevel = ~0;
		break;
	}ARGEND

	c = threadwaitchan();
	proccreate(doexec, argv, 8192);
	w = recvp(c);
	if(w == nil)
		print("exec failed: %r\n");
	else
		print("%d %lud %lud %lud %s\n", w->pid, w->time[0], w->time[1], w->time[2], w->msg);
	threadexits(nil);
}
