#include <u.h>
#include <libc.h>
#include <thread.h>

Channel *c;

void
f(void *v)
{
	recvp(c);
}

void
threadmain(int argc, char **argv)
{
	int i;

	c = chancreate(sizeof(ulong), 0);
	for(i=0;; i++){
		print("%d\n", i);
		proccreate(f, nil, 8192);
	}
}
