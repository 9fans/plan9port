#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

#undef waitpid
#undef pipe
#undef wait

static int sigpid;
static void
sigenable(int sig, int enabled)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, sig);
	sigprocmask(enabled ? SIG_UNBLOCK : SIG_BLOCK, &mask, 0);
}

static void
child(void)
{
	int status, pid;
printf("wait %d in %d\n", sigpid, getpid());
	pid = waitpid(sigpid, &status, __WALL);
	if(pid < 0)
		perror("wait");
	else if(WIFEXITED(status))
		 _exit(WEXITSTATUS(status));
printf("pid %d if %d %d\n", pid, WIFEXITED(status), WEXITSTATUS(status));
	_exit(97);
}

static void
sigpass(int sig)
{
	if(sig == SIGCHLD){
print("sig\n");
		child();
	}else
		kill(sigpid, sig);
}

void
_threadsetupdaemonize(void)
{
	int i, n, pid;
	int p[2];
	char buf[20];

	sigpid = 1;

	if(pipe(p) < 0)
		abort();

	signal(SIGCHLD, sigpass);
	switch(pid = fork()){
	case -1:
		abort();
	default:
		close(p[1]);
		break;
	case 0:
		close(p[0]);
		return;
	}

	sigpid = pid;

	read(p[0], buf, sizeof buf-1);
print("pipe\n");
	child();
}

void*
sleeper(void *v)
{
	pthread_mutex_t m;
	pthread_cond_t c;

	pthread_mutex_init(&m, 0);
	pthread_cond_init(&c, 0);
	pthread_cond_wait(&c, &m);
	return 0;
}

void
main(int argc, char **argv)
{
	pthread_t pid;

	_threadsetupdaemonize();
	pthread_create(&pid, 0, sleeper, 0);
	exit(1);
}
