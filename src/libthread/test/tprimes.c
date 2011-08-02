#include "u.h"
#include "libc.h"
#include "thread.h"

enum
{
	STACK = 8192
};

int max = 10000;
int (*mk)(void (*fn)(void*), void *arg, uint stack);
void printmsg(void*, char*);

void
countthread(void *v)
{
	uint i;
	Channel *c;

	c = v;
	for(i=2;; i++){
		sendul(c, i);
	}
}

void
filterthread(void *v)
{
	uint i, p;
	Channel *c, *nextc;

	c = v;
	p = recvul(c);
	print("%d\n", p);
	if(p > max)
		threadexitsall(0);
	nextc = chancreate(sizeof(ulong), 0);
	mk(filterthread, nextc, STACK);
	for(;;){
		i = recvul(c);
		if(i%p)
			sendul(nextc, i);
	}
}

void
usage(void)
{
	fprint(2, "usage: tprimes [-p] [max]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	Channel *c;
	int nbuf;

	notify(printmsg);
	nbuf = 0;
	mk = threadcreate;
	ARGBEGIN{
	default:
		usage();
	case 'b':
		nbuf = atoi(EARGF(usage()));
		break;
	case 'p':
		mk = proccreate;
		max = 1000;
		break;
	}ARGEND

	if(argc == 1)
		max = atoi(argv[0]);
	else if(argc)
		usage();

	c = chancreate(sizeof(ulong), nbuf);
	mk(countthread, c, STACK);
	mk(filterthread, c, STACK);
	recvp(chancreate(sizeof(void*), 0));
	threadexitsall(0);
}

void
printmsg(void *v, char *msg)
{
	print("note: %s\n", msg);
}
