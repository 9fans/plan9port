#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <lib9.h>

static struct {
	int sig;
	char *str;
} tab[] = {
	SIGHUP,		"hangup",
	SIGINT,		"interrupt",
	SIGQUIT,		"quit",
	SIGILL,		"sys: trap: illegal instruction",
	SIGTRAP,		"sys: trace trap",
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
	SIGIO,		"sys: i/o possible on fd",
	SIGXCPU,		"sys: cpu time limit exceeded",
	SIGXFSZ,		"sys: file size limit exceeded",
	SIGVTALRM,	"sys: virtual time alarm",
	SIGPROF,		"sys: profiling timer alarm",
	SIGWINCH,	"sys: window size change",
#ifdef SIGINFO
	SIGINFO,		"sys: status request",
#endif
	SIGUSR1,		"sys: usr1",
	SIGUSR2,		"sys: usr2",
};
	
static char*
_p9sigstr(int sig, char *tmp)
{
	int i;

	for(i=0; i<nelem(tab); i++)
		if(tab[i].sig == sig)
			return tab[i].str;
	sprint(tmp, "sys: signal %d", sig);
	return tmp;
}

/*
static int
_p9strsig(char *s)
{
	int i;

	for(i=0; i<nelem(tab); i++)
		if(strcmp(s, tab[i].str) == 0)
			return tab[i].sig;
	return 0;
}
*/

int
await(char *str, int n)
{
	int pid, status, cd;
	struct rusage ru;
	char buf[128], tmp[64];
	ulong u, s;

	for(;;){
		pid = wait3(&status, 0, &ru);
		if(pid < 0)
			return -1;
		u = ru.ru_utime.tv_sec*1000+((ru.ru_utime.tv_usec+500)/1000);
		s = ru.ru_stime.tv_sec*1000+((ru.ru_stime.tv_usec+500)/1000);
		if(WIFEXITED(status)){
			status = WEXITSTATUS(status);
			if(status)
				snprint(buf, sizeof buf, "%d %lu %lu %lu %d", pid, u, s, u+s, status);
			else
				snprint(buf, sizeof buf, "%d %lu %lu %lu ''", pid, u, s, u+s);
			strecpy(str, str+n, buf);
			return strlen(str);
		}
		if(WIFSIGNALED(status)){
			cd = WCOREDUMP(status);
			USED(cd);
			snprint(buf, sizeof buf, "%d %lu %lu %lu '%s'", pid, u, s, u+s, _p9sigstr(WTERMSIG(status), tmp));
			strecpy(str, str+n, buf);
			return strlen(str);
		}
	}
}
