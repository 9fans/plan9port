#include <lib9.h>
#include <thread.h>

Channel *c[3];


void
pingpong(void *v)
{
	int n;
	Channel **c;

	c = v;
	do{
		n = recvul(c[0]);
		sendul(c[1], n-1);
	}while(n > 0);
	exit(0);
}

void
threadmain(int argc, char **argv)
{
	c[0] = chancreate(sizeof(ulong), 1);
	c[1] = chancreate(sizeof(ulong), 1);
	c[2] = c[0];

	threadcreate(pingpong, c, 16384);
	threadcreate(pingpong, c+1, 16384);
	sendul(c[0], atoi(argv[1]));
}
