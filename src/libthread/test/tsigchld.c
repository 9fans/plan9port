#include <u.h>
#include <libc.h>
#include <thread.h>

void
f(void *v)
{
	USED(v);
}

void
threadmain(int argc, char **argv)
{
	int i, j;

	for(i=0; i<1000; i++){
		proccreate(f, nil, 32000);
		for(j=0; j<8; j++)
			if(fork() == 0)
				_exit(0);
		for(j=0; j<8; j++)
			waitpid();
	}
	threadexitsall(nil);
}
