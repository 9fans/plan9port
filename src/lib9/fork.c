#include <u.h>
#include <signal.h>
#include <libc.h>
#include "9proc.h"
#undef fork

int
p9fork(void)
{
	int pid;
	sigset_t all, old;

	sigfillset(&all);
	sigprocmask(SIG_SETMASK, &all, &old);
	pid = fork();
	if(pid == 0){
		_clearuproc();
		_p9uproc(0);
	}
	sigprocmask(SIG_SETMASK, &old, nil);
	return pid;
}
