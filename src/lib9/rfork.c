#define NOPLAN9DEFINES
#include <lib9.h>

int
p9rfork(int flags)
{
	if(flags&RFPROC){
		werrstr("cannot use rfork to fork -- use ffork");
		return -1;
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
