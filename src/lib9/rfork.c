#include <u.h>
#include <sys/wait.h>
#include <signal.h>
#include <libc.h>
#undef rfork

int
p9rfork(int flags)
{
	int pid, status;
	int p[2];
	int n;
	char buf[128], *q;

	if((flags&(RFPROC|RFFDG|RFMEM)) == (RFPROC|RFFDG)){
		/* check other flags before we commit */
		flags &= ~(RFPROC|RFFDG);
		n = (flags & ~(RFNOTEG|RFNAMEG|RFNOWAIT));
		if(n){
			werrstr("unknown flags %08ux in rfork", n);
			return -1;
		}
		if(flags&RFNOWAIT){
			if(pipe(p) < 0)
				return -1;
		}
		pid = fork();
		if(pid == -1)
			return -1;
		if(flags&RFNOWAIT){
			flags &= ~RFNOWAIT;
			if(pid){
				/*
				 * Parent - wait for child to fork wait-free child.
				 * Then read pid from pipe.  Assume pipe buffer can absorb the write.
				 */
				close(p[1]);
				wait4(pid, &status, 0, 0);
				n = readn(p[0], buf, sizeof buf-1);
				close(p[0]);
				if(!WIFEXITED(status) || WEXITSTATUS(status)!=0 || n <= 0){
					werrstr("pipe dance failed in rfork");
					return -1;
				}
				buf[n] = 0;
				n = strtol(buf, &q, 0);
				if(*q != 0){
					werrstr("%s", q);
					return -1;
				}
				pid = n;
			}else{
				/*
				 * Child - fork a new child whose wait message can't 
				 * get back to the parent because we're going to exit!
				 */
				signal(SIGCHLD, SIG_IGN);
				close(p[0]);
				pid = fork();
				if(pid){
					/* Child parent - send status over pipe and exit. */
					if(pid > 0)
						fprint(p[1], "%d", pid);
					else
						fprint(p[1], " %r");
					close(p[1]);
					_exit(0);
				}else{
					/* Child child - close pipe. */
					close(p[1]);
				}
			}
		}
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
	if(flags&RFNOWAIT){
		werrstr("cannot use RFNOWAIT without RFPROC");
		return -1;
	}
	if(flags){
		werrstr("unknown flags %08ux in rfork", flags);
		return -1;
	}
	return 0;
}
