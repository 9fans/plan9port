#define NOPLAN9DEFINES
#include <lib9.h>

int
p9rfork(int flags)
{
	int pid;

	if((flags&(RFPROC|RFFDG|RFMEM)) == (RFPROC|RFFDG)){
		/* check other flags before we commit */
		flags &= ~(RFPROC|RFFDG);
		if(flags & ~(RFNOTEG|RFNAMEG)){
			werrstr("unknown flags %08ux in rfork", flags);
			return -1;
		}
		pid = fork();
		if(pid != 0)
			return pid;
	}
	if(flags&RFPROC){
		werrstr("cannot use rfork for shared memory -- use ffork");
		return -1;
	}
	if(flags&RFNAMEG){
		/* XXX set $NAMESPACE to a new directory */
		flags &= ~RFNAMEG;
	}
	if(flags&RFNOTEG){
		setpgid(0, getpid());
		flags &= ~RFNOTEG;
	}
	if(flags){
		werrstr("unknown flags %08ux in rfork", flags);
		return -1;
	}
	return 0;
}
