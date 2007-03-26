#include <u.h>
#include <signal.h>
#if defined(PLAN9PORT) && defined(__sun__)
#	define BSD_COMP	/* sigh.  for TIOCNOTTY */
#endif
#include <sys/ioctl.h>
#include "rc.h"
#include "getflags.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

int havefork = 1;

void
Xasync(void)
{
	int null = open("/dev/null", 0);
	int tty;
	int pid;
	char npid[10];
	if(null<0){
		Xerror("Can't open /dev/null\n");
		return;
	}
	switch(pid = rfork(RFFDG|RFPROC|RFNOTEG)){
	case -1:
		close(null);
		Xerror("try again");
		break;
	case 0:
		clearwaitpids();
		/*
		 * I don't know what the right thing to do here is,
		 * so this is all experimentally determined.
		 * If we just dup /dev/null onto 0, then running
		 * ssh foo & will reopen /dev/tty, try to read a password,
		 * get a signal, and repeat, in a tight loop, forever.
		 * Arguably this is a bug in ssh (it behaves the same
		 * way under bash as under rc) but I'm fixing it here 
		 * anyway.  If we dissociate the process from the tty,
		 * then it won't be able to open /dev/tty ever again.
		 * The SIG_IGN on SIGTTOU makes writing the tty
		 * (via fd 1 or 2, for example) succeed even though 
		 * our pgrp is not the terminal's controlling pgrp.
		 */
		if((tty = open("/dev/tty", OREAD)) >= 0){
			/*
			 * Should make reads of tty fail, writes succeed.
			 */
			signal(SIGTTIN, SIG_IGN);
			signal(SIGTTOU, SIG_IGN);
			ioctl(tty, TIOCNOTTY);
			close(tty);
		}
		if(isatty(0))
			pushredir(ROPEN, null, 0);
		else
			close(null);
		start(runq->code, runq->pc+1, runq->local);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		close(null);
		runq->pc = runq->code[runq->pc].i;
		inttoascii(npid, pid);
		setvar("apid", newword(npid, (word *)0));
		break;
	}
}

void
Xpipe(void)
{
	struct thread *p = runq;
	int pc = p->pc, forkid;
	int lfd = p->code[pc++].i;
	int rfd = p->code[pc++].i;
	int pfd[2];
	if(pipe(pfd)<0){
		Xerror("can't get pipe");
		return;
	}
	switch(forkid = fork()){
	case -1:
		Xerror("try again");
		break;
	case 0:
		clearwaitpids();
		start(p->code, pc+2, runq->local);
		runq->ret = 0;
		close(pfd[PRD]);
		pushredir(ROPEN, pfd[PWR], lfd);
		break;
	default:
		addwaitpid(forkid);
		start(p->code, p->code[pc].i, runq->local);
		close(pfd[PWR]);
		pushredir(ROPEN, pfd[PRD], rfd);
		p->pc = p->code[pc+1].i;
		p->pid = forkid;
		break;
	}
}

/*
 * Who should wait for the exit from the fork?
 */
void
Xbackq(void)
{
	char wd[8193];
	int c;
	char *s, *ewd=&wd[8192], *stop;
	struct io *f;
	var *ifs = vlook("ifs");
	word *v, *nextv;
	int pfd[2];
	int pid;
	stop = ifs->val?ifs->val->word:"";
	if(pipe(pfd)<0){
		Xerror("can't make pipe");
		return;
	}
	switch(pid = fork()){
	case -1:
		Xerror("try again");
		close(pfd[PRD]);
		close(pfd[PWR]);
		return;
	case 0:
		clearwaitpids();
		close(pfd[PRD]);
		start(runq->code, runq->pc+1, runq->local);
		pushredir(ROPEN, pfd[PWR], 1);
		return;
	default:
		addwaitpid(pid);
		close(pfd[PWR]);
		f = openfd(pfd[PRD]);
		s = wd;
		v = 0;
		while((c = rchr(f))!=EOF){
			if(strchr(stop, c) || s==ewd){
				if(s!=wd){
					*s='\0';
					v = newword(wd, v);
					s = wd;
				}
			}
			else *s++=c;
		}
		if(s!=wd){
			*s='\0';
			v = newword(wd, v);
		}
		closeio(f);
		Waitfor(pid, 0);
		/* v points to reversed arglist -- reverse it onto argv */
		while(v){
			nextv = v->next;
			v->next = runq->argv->words;
			runq->argv->words = v;
			v = nextv;
		}
		runq->pc = runq->code[runq->pc].i;
		return;
	}
}

void
Xpipefd(void)
{
	struct thread *p = runq;
	int pc = p->pc, pid;
	char name[40];
	int pfd[2];
	int sidefd, mainfd;
	if(pipe(pfd)<0){
		Xerror("can't get pipe");
		return;
	}
	if(p->code[pc].i==READ){
		sidefd = pfd[PWR];
		mainfd = pfd[PRD];
	}
	else{
		sidefd = pfd[PRD];
		mainfd = pfd[PWR];
	}
	switch(pid = fork()){
	case -1:
		Xerror("try again");
		break;
	case 0:
		clearwaitpids();
		start(p->code, pc+2, runq->local);
		close(mainfd);
		pushredir(ROPEN, sidefd, p->code[pc].i==READ?1:0);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		close(sidefd);
		pushredir(ROPEN, mainfd, mainfd);	/* isn't this a noop? */
		strcpy(name, Fdprefix);
		inttoascii(name+strlen(name), mainfd);
		pushword(name);
		p->pc = p->code[pc+1].i;
		break;
	}
}

void
Xsubshell(void)
{
	int pid;
	switch(pid = fork()){
	case -1:
		Xerror("try again");
		break;
	case 0:
		clearwaitpids();
		start(runq->code, runq->pc+1, runq->local);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		Waitfor(pid, 1);
		runq->pc = runq->code[runq->pc].i;
		break;
	}
}

int
execforkexec(void)
{
	int pid;
	int n;
	char buf[ERRMAX];

	switch(pid = fork()){
	case -1:
		return -1;
	case 0:
		clearwaitpids();
		pushword("exec");
		execexec();
		strcpy(buf, "can't exec: ");
		n = strlen(buf);
		errstr(buf+n, ERRMAX-n);
		Exit(buf);
	}
	addwaitpid(pid);
	return pid;
}
