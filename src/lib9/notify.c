#include <u.h>
#include <signal.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include "9proc.h"

extern char *_p9sigstr(int, char*);

static struct {
	int sig;
	int restart;
} sigs[] = {
	SIGHUP, 0,
	SIGINT, 0,
	SIGQUIT, 0,
	SIGILL, 0,
	SIGTRAP, 0,
/*	SIGABRT,	*/
#ifdef SIGEMT
	SIGEMT, 0,
#endif
	SIGFPE, 0,
	SIGBUS, 0,
/*	SIGSEGV,	*/
	SIGCHLD, 1,
	SIGSYS, 0,
	SIGPIPE, 0,
	SIGALRM, 0,
	SIGTERM, 0,
	SIGTSTP, 1,
	SIGTTIN, 1,
	SIGTTOU, 1,
	SIGXCPU, 0,
	SIGXFSZ, 0,
	SIGVTALRM, 0,
	SIGUSR1, 0,
	SIGUSR2, 0,
	SIGWINCH, 1,
#ifdef SIGINFO
	SIGINFO, 0,
#endif
};

static void (*notifyf)(void*, char*);

static void
notifysigf(int sig)
{
	int v;
	char tmp[64];
	Uproc *up;

	up = _p9uproc(1);
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
	struct sigaction sa, osa;

	_p9uproc(0);
	memset(&sa, 0, sizeof sa);
	if(f == 0)
		sa.sa_handler = SIG_DFL;
	else{
		notifyf = f;
		sa.sa_handler = notifysigf;
	}
	for(i=0; i<nelem(sigs); i++){
		/*
		 * If someone has already installed a handler,
		 * It's probably some ld preload nonsense,
		 * like pct (a SIGVTALRM-based profiler).
		 * Leave it alone.
		 */
		sigaction(sigs[i].sig, nil, &osa);
		if(osa.sa_handler != SIG_DFL)
			continue;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, i);
		if(sigs[i].restart)
			sa.sa_flags |= SA_RESTART;
		else
			sa.sa_flags &= ~SA_RESTART;
		sigaction(sigs[i].sig, &sa, nil);
	}
	return 0;
}

int
noted(int v)
{
	Uproc *up;

	up = _p9uproc(1);
	p9longjmp(up->notejb, v==NCONT ? 2 : 1);
	abort();
	return 0;
}
