#include "u.h"
#include "libc.h"
#include "thread.h"

void
threadmain(int argc, char **argv)
{
	int fd[3];
	Channel *c;
	Waitmsg *w;

	ARGBEGIN{
	case 'D':
		break;
	}ARGEND

	c = threadwaitchan();
	fd[0] = dup(0, -1);
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	if(threadspawn(fd, argv[0], argv) < 0)
		sysfatal("threadspawn: %r");
	fd[0] = dup(0, -1);
	fd[1] = dup(1, -1);
	fd[2] = dup(2, -1);
	if(threadspawn(fd, argv[0], argv) < 0)
		sysfatal("threadspawn: %r");
	w = recvp(c);
	if(w == nil)
		print("exec/recvp failed: %r\n");
	else
		print("%d %lud %lud %lud %s\n", w->pid, w->time[0], w->time[1], w->time[2], w->msg);
	w = recvp(c);
	if(w == nil)
		print("exec/recvp failed: %r\n");
	else
		print("%d %lud %lud %lud %s\n", w->pid, w->time[0], w->time[1], w->time[2], w->msg);
	threadexits(nil);
}
