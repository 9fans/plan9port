#include <u.h>
#include <libc.h>
#include <thread.h>

void
f(void *v)
{
	USED(v);

	recvp(chancreate(sizeof(void*), 0));
}

void
threadmain(int argc, char **argv)
{
	proccreate(f, nil, 32000);
	exit(1);
}
