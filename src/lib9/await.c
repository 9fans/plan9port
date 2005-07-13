#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifndef WCOREDUMP	/* not on Mac OS X Tiger */
#define WCOREDUMP(status) 0
#endif

static struct {
	int sig;
	char *str;
} tab[] = {
	SIGHUP,		"hangup",
	SIGINT,		"interrupt",
	SIGQUIT,		"quit",
	SIGILL,		"sys: illegal instruction",
	SIGTRAP,		"sys: breakpoint",
	SIGABRT,		"sys: abort",
#ifdef SIGEMT
	SIGEMT,		"sys: emulate instruction executed",
#endif
	SIGFPE,		"sys: fp: trap",
	SIGKILL,		"sys: kill",
	SIGBUS,		"sys: bus error",
	SIGSEGV,		"sys: segmentation violation",
	SIGALRM,		"alarm",
	SIGTERM,		"kill",
	SIGURG,		"sys: urgent condition on socket",
	SIGSTOP,		"sys: stop",
	SIGTSTP,		"sys: tstp",
	SIGCONT,		"sys: cont",
	SIGCHLD,		"sys: child",
	SIGTTIN,		"sys: ttin",
	SIGTTOU,		"sys: ttou",
#ifdef SIGIO	/* not on Mac OS X Tiger */
	SIGIO,		"sys: i/o possible on fd",
#endif
	SIGXCPU,		"sys: cpu time limit exceeded",
	SIGXFSZ,		"sys: file size limit exceeded",
	SIGVTALRM,	"sys: virtual time alarm",
	SIGPROF,		"sys: profiling timer alarm",
#ifdef SIGWINCH	/* not on Mac OS X Tiger */
	SIGWINCH,	"sys: window size change",
#endif
#ifdef SIGINFO
	SIGINFO,		"sys: status request",
#endif
	SIGUSR1,		"sys: usr1",
	SIGUSR2,		"sys: usr2",
	SIGPIPE,		"sys: write on closed pipe",
};
	
char*
_p9sigstr(int sig, char *tmp)
{
	int i;

	for(i=0; i<nelem(tab); i++)
		if(tab[i].sig == sig)
			return tab[i].str;
	if(tmp == nil)
		return nil;
	sprint(tmp, "sys: signal %d", sig);
	return tmp;
}

int
_p9strsig(char *s)
{
	int i;

	for(i=0; i<nelem(tab); i++)
		if(strcmp(s, tab[i].str) == 0)
			return tab[i].sig;
	return 0;
}

static int
_await(int pid4, char *str, int n, int opt)
{
	int pid, status, cd;
	struct rusage ru;
	char buf[128], tmp[64];
	ulong u, s;

	for(;;){
		/* On Linux, pid==-1 means anyone; on SunOS, it's pid==0. */
		if(pid4 == -1)
			pid = wait3(&status, opt, &ru);
		else
			pid = wait4(pid4, &status, opt, &ru);
		if(pid <= 0)
			return -1;
		u = ru.ru_utime.tv_sec*1000+((ru.ru_utime.tv_usec+500)/1000);
		s = ru.ru_stime.tv_sec*1000+((ru.ru_stime.tv_usec+500)/1000);
		if(WIFEXITED(status)){
			status = WEXITSTATUS(status);
			if(status)
				snprint(buf, sizeof buf, "%d %lud %lud %lud %d", pid, u, s, u+s, status);
			else
				snprint(buf, sizeof buf, "%d %lud %lud %lud ''", pid, u, s, u+s, status);
			strecpy(str, str+n, buf);
			return strlen(str);
		}
		if(WIFSIGNALED(status)){
			cd = WCOREDUMP(status);
			snprint(buf, sizeof buf, "%d %lud %lud %lud 'signal: %s%s'", pid, u, s, u+s, _p9sigstr(WTERMSIG(status), tmp), cd ? " (core dumped)" : "");
			strecpy(str, str+n, buf);
			return strlen(str);
		}
	}
}

int
await(char *str, int n)
{
	return _await(-1, str, n, 0);
}

int
awaitnohang(char *str, int n)
{
	return _await(-1, str, n, WNOHANG);
}

int
awaitfor(int pid, char *str, int n)
{
	return _await(pid, str, n, 0);
}

