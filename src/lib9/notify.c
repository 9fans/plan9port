#include <signal.h>

#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include "9proc.h"

extern char *_p9sigstr(int, char*);

static int sigs[] = {
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGILL,
	SIGTRAP,
/*	SIGABRT,	*/
#ifdef SIGEMT
	SIGEMT,
#endif
	SIGFPE,
	SIGBUS,
	SIGSEGV,
	SIGSYS,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	SIGTSTP,
	SIGTTIN,
	SIGTTOU,
	SIGXCPU,
	SIGXFSZ,
	SIGVTALRM,
	SIGUSR1,
	SIGUSR2,
#ifdef SIGINFO
	SIGINFO,
#endif
};

static void (*notifyf)(void*, char*);

static void
notifysigf(int sig)
{
	int v;
	char tmp[64];
	Uproc *up;

	up = _p9uproc();
	v = p9setjmp(up->notejb);
	if(v == 0 && notifyf)
		(*notifyf)(nil, _p9sigstr(sig, tmp));
	else if(v == 2){
if(0)print("HANDLED %d\n", sig);
		return;
	}
if(0)print("DEFAULT %d\n", sig);
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}
	
int
notify(void (*f)(void*, char*))
{
	int i;
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	if(f == nil)
		sa.sa_handler = SIG_DFL;
	else{
		notifyf = f;
		sa.sa_handler = notifysigf;
	}
	for(i=0; i<nelem(sigs); i++)
		sigaction(sigs[i], &sa, 0);
	return 0;
}

int
noted(int v)
{
	Uproc *up;

	up = _p9uproc();
	p9longjmp(up->notejb, v==NCONT ? 2 : 1);
	abort();
	return 0;
}
