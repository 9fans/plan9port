#include <lib9.h>
#include <thread.h>

int quiet;
int goal;
int buffer;
int (*fn)(void(*)(void*), void*, uint) = threadcreate;

void
primethread(void *arg)
{
	Channel *c, *nc;
	int p, i;

	c = arg;
	p = recvul(c);
	if(p > goal)
		threadexitsall(nil);
	if(!quiet)
		print("%d\n", p);
	nc = chancreate(sizeof(ulong), buffer);
	(*fn)(primethread, nc, 8192);
	for(;;){
		i = recvul(c);
		if(i%p)
			sendul(nc, i);
	}
}

extern int _threaddebuglevel;

void
threadmain(int argc, char **argv)
{
	int i;
	Channel *c;

	ARGBEGIN{
	case 'D':
		_threaddebuglevel = atoi(ARGF());
		break;
	case 'q':
		quiet = 1;
		break;
	case 'b':
		buffer = atoi(ARGF());
		break;
	case 'p':
		fn=proccreate;
		break;
	}ARGEND

	if(argc>0)
		goal = atoi(argv[0]);
	else
		goal = 100;

	c = chancreate(sizeof(ulong), buffer);
	(*fn)(primethread, c, 8192);
	for(i=2;; i++)
		sendul(c, i);
}
