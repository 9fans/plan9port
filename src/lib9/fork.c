#include <u.h>
#include <libc.h>
#include "9proc.h"
#undef fork

int
p9fork(void)
{
	int pid;

	pid = fork();
	_clearuproc();
	_p9uproc(0);
	return pid;
}
