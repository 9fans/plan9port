#include <u.h>
#include <signal.h>
#define NOPLAN9DEFINES
#include <libc.h>

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

typedef struct Jmp Jmp;
struct Jmp
{
	p9jmp_buf b;
};

static Jmp onejmp;

static Jmp*
getonejmp(void)
{
	return &onejmp;
}

Jmp *(*_notejmpbuf)(void) = getonejmp;
static void (*notifyf)(void*, char*);

static void
notifysigf(int sig)
{
	int v;
	char tmp[64];
	Jmp *j;

	j = (*_notejmpbuf)();
	v = p9setjmp(j->b);
	if(v == 0 && notifyf)
		(*notifyf)(nil, _p9sigstr(sig, tmp));
	else if(v == 2){
		if(0)print("HANDLED %d\n", sig);
		return;
	}
	if(0)print("DEFAULT %d\n", sig);
	signal(sig, SIG_DFL);
	raise(sig);
}

int
noted(int v)
{
	p9longjmp((*_notejmpbuf)()->b, v==NCONT ? 2 : 1);
	abort();
	return 0;
}

int
notify(void (*f)(void*, char*))
{
	int i;
	struct sigaction sa, osa;

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
		/*
		 * We assume that one jump buffer per thread
		 * is okay, which means that we can't deal with 
		 * signal handlers called during signal handlers.
		 */
		sigfillset(&sa.sa_mask);
		if(sigs[i].restart)
			sa.sa_flags |= SA_RESTART;
		else
			sa.sa_flags &= ~SA_RESTART;
		sigaction(sigs[i].sig, &sa, nil);
	}
	return 0;
}
