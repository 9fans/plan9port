#include <u.h>
#include <signal.h>
#include <libc.h>

#define DBG 0

static void
ign(int x)
{
	USED(x);
}

void /*__attribute__((constructor))*/
ignusr1(int restart)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = ign;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGUSR1);
	if(restart)
		sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, nil);
}

void
_procsleep(_Procrend *r)
{
	sigset_t mask;

	/*
	 * Go to sleep.
	 *
	 * Block USR1, set the handler to interrupt system calls,
	 * unlock the vouslock so our waker can wake us,
	 * and then suspend.
	 */
	r->asleep = 1;
	r->pid = getpid();

	sigprocmask(SIG_SETMASK, nil, &mask);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_SETMASK, &mask, nil);
	ignusr1(0);
	unlock(r->l);
	sigdelset(&mask, SIGUSR1);
	sigsuspend(&mask);

	/*
	 * We're awake.  Make USR1 not interrupt system calls.
	 */
	ignusr1(1);
	assert(r->asleep == 0);
	lock(r->l);
}

void
_procwakeup(_Procrend *r)
{
	r->asleep = 0;
	assert(r->pid >= 1);
	kill(r->pid, SIGUSR1);
}
