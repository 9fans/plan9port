#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int
main(void)
{
	ulong x;

	x = (ulong)pthread_self();
	printf("%lx\n", x);
	if(x < 1024*1024)
		exit(1);	/* NOT NPTL */
	exit(0);
}
