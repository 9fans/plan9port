#include <u.h>
#include <signal.h>
#define NOPLAN9DEFINES
#include <libc.h>

extern char *_p9sigstr(int, char*);

static struct {
	int sig;
	int restart;
	int dumb;
} sigs[] = {
	SIGHUP, 0, 0,
	SIGINT, 0, 0,
	SIGQUIT, 0, 0,
	SIGILL, 0, 0,
	SIGTRAP, 0, 0,
/*	SIGABRT,	*/
#ifdef SIGEMT
	SIGEMT, 0, 0,
#endif
	SIGFPE, 0, 0,
	SIGBUS, 0, 0,
/*	SIGSEGV,	*/
	SIGCHLD, 1, 1,
	SIGSYS, 0, 0,
	SIGPIPE, 0, 1,
	SIGALRM, 0, 0,
	SIGTERM, 0, 0,
	SIGTSTP, 1, 1,
	SIGTTIN, 1, 1,
	SIGTTOU, 1, 1,
	SIGXCPU, 0, 0,
	SIGXFSZ, 0, 0,
	SIGVTALRM, 0, 0,
	SIGUSR1, 0, 0,
	SIGUSR2, 0, 0,
	SIGWINCH, 1, 1,
#ifdef SIGINFO
	SIGINFO, 0, 0,
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
static int alldumb;

static void
nop(int sig)
{
	USED(sig);
}

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

static void
handlesig(int s, int r, int skip)
{
	struct sigaction sa, osa;

	/*
	 * If someone has already installed a handler,
	 * It's probably some ld preload nonsense,
	 * like pct (a SIGVTALRM-based profiler).
	 * Leave it alone.
	 */
	sigaction(s, nil, &osa);
	if(osa.sa_handler != SIG_DFL && osa.sa_handler != notifysigf && osa.sa_handler != nop)
		return;

	memset(&sa, 0, sizeof sa);
	if(skip)
		sa.sa_handler = nop;
	else if(notifyf == 0)
		sa.sa_handler = SIG_DFL;
	else
		sa.sa_handler = notifysigf;

	/*
	 * We assume that one jump buffer per thread
	 * is okay, which means that we can't deal with 
	 * signal handlers called during signal handlers.
	 */
	sigfillset(&sa.sa_mask);
	if(r)
		sa.sa_flags |= SA_RESTART;
	else
		sa.sa_flags &= ~SA_RESTART;
	sigaction(s, &sa, nil);
}

int
notify(void (*f)(void*, char*))
{
	int i;

	notifyf = f;
	for(i=0; i<nelem(sigs); i++)
		handlesig(sigs[i].sig, sigs[i].restart, sigs[i].dumb && !alldumb);
	return 0;
}

void
notifyall(int all)
{
	int i;

	alldumb = all;
	for(i=0; i<nelem(sigs); i++)
		if(sigs[i].dumb)
			handlesig(sigs[i].sig, sigs[i].restart, !all);
}

void
notifyatsig(int sig, int use)
{
	int i;

	for(i=0; i<nelem(sigs); i++)
		if(sigs[i].sig == sig)
			handlesig(sigs[i].sig, sigs[i].restart, 0);
}

