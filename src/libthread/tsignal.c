#include <u.h>
#include <libc.h>
#include <thread.h>

extern int _threaddebuglevel;

void
usage(void)
{
	fprint(2, "usage: tsignal [-[ednf] note]*\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	Channel *c;
	char *msg;

	ARGBEGIN{
	case 'D':
		_threaddebuglevel = ~0;
		break;
	default:
		usage();
	case 'e':
		notifyenable(EARGF(usage()));
		break;
	case 'd':
		notifydisable(EARGF(usage()));
		break;
	case 'n':
		notifyon(EARGF(usage()));
		break;
	case 'f':
		notifyoff(EARGF(usage()));
		break;
	}ARGEND

	c = threadnotechan();
	while((msg = recvp(c)) != nil)
		print("note: %s\n", msg);
}
