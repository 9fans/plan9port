#include "rc.h"
#include "getflags.h"
#include "exec.h"
#include "io.h"
#include "fns.h"

static int *waitpids;
static int nwaitpids;

void
addwaitpid(int pid)
{
	waitpids = erealloc(waitpids, (nwaitpids+1)*sizeof waitpids[0]);
	waitpids[nwaitpids++] = pid;
}

void
delwaitpid(int pid)
{
	int r, w;
	
	for(r=w=0; r<nwaitpids; r++)
		if(waitpids[r] != pid)
			waitpids[w++] = waitpids[r];
	nwaitpids = w;
}

void
clearwaitpids(void)
{
	nwaitpids = 0;
}

int
havewaitpid(int pid)
{
	int i;

	for(i=0; i<nwaitpids; i++)
		if(waitpids[i] == pid)
			return 1;
	return 0;
}

void
Xasync(void)
{
	int pid;
	char npid[10];

	switch(pid = Fork()){
	case -1:
		Xerror2("try again", Errstr());
		break;
	case 0:
		clearwaitpids();
		start(runq->code, runq->pc+1, runq->local, runq->redir);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		runq->pc = runq->code[runq->pc].i;
		inttoascii(npid, pid);
		setvar("apid", newword(npid, (word *)0));
		break;
	}
}

void
Xpipe(void)
{
	thread *p = runq;
	int pid, pc = p->pc;
	int lfd = p->code[pc++].i;
	int rfd = p->code[pc++].i;
	int pfd[2];

	if(pipe(pfd)<0){
		Xerror2("can't get pipe", Errstr());
		return;
	}
	switch(pid = Fork()){
	case -1:
		Xerror2("try again", Errstr());
		break;
	case 0:
		clearwaitpids();
		Close(pfd[PRD]);
		start(p->code, pc+2, runq->local, runq->redir);
		runq->ret = 0;
		pushredir(ROPEN, pfd[PWR], lfd);
		break;
	default:
		addwaitpid(pid);
		Close(pfd[PWR]);
		start(p->code, p->code[pc].i, runq->local, runq->redir);
		pushredir(ROPEN, pfd[PRD], rfd);
		p->pc = p->code[pc+1].i;
		p->pid = pid;
		break;
	}
}

/*
 * Who should wait for the exit from the fork?
 */

void
Xbackq(void)
{
	int pid, pfd[2];
	char *s, *split;
	word *end, **link;
	io *f;

	if(pipe(pfd)<0){
		Xerror2("can't make pipe", Errstr());
		return;
	}
	switch(pid = Fork()){
	case -1:
		Xerror2("try again", Errstr());
		Close(pfd[PRD]);
		Close(pfd[PWR]);
		return;
	case 0:
		clearwaitpids();
		Close(pfd[PRD]);
		start(runq->code, runq->pc+1, runq->local, runq->redir);
		pushredir(ROPEN, pfd[PWR], 1);
		return;
	default:
		addwaitpid(pid);
		Close(pfd[PWR]);

		split = Popword();
		poplist();
		f = openiofd(pfd[PRD]);
		end = runq->argv->words;
		link = &runq->argv->words;
		while((s = rstr(f, split)) != 0){
			*link = Newword(s, (word*)0);
			link = &(*link)->next;
		}
		*link = end;
		closeio(f);
		free(split);

		Waitfor(pid);

		runq->pc = runq->code[runq->pc].i;
		return;
	}
}

void
Xpipefd(void)
{
	thread *p = runq;
	int pid, pc = p->pc;
	char name[40];
	int pfd[2];
	int sidefd, mainfd;

	if(pipe(pfd)<0){
		Xerror2("can't get pipe", Errstr());
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
	switch(pid = Fork()){
	case -1:
		Xerror2("try again", Errstr());
		break;
	case 0:
		clearwaitpids();
		Close(mainfd);
		start(p->code, pc+2, runq->local, runq->redir);
		pushredir(ROPEN, sidefd, p->code[pc].i==READ?1:0);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		Close(sidefd);
		pushredir(ROPEN, mainfd, mainfd);
		shuffleredir();	/* shuffle redir to bottom of stack for Xpopredir() */
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

	switch(pid = Fork()){
	case -1:
		Xerror2("try again", Errstr());
		break;
	case 0:
		clearwaitpids();
		start(runq->code, runq->pc+1, runq->local, runq->redir);
		runq->ret = 0;
		break;
	default:
		addwaitpid(pid);
		while(Waitfor(pid) < 0)
			;
		runq->pc = runq->code[runq->pc].i;
		break;
	}
}

int
execforkexec(void)
{
	int pid;

	switch(pid = Fork()){
	case -1:
		return -1;
	case 0:
		clearwaitpids();
		pushword("exec");
		execexec();
		/* does not return */
	}
	addwaitpid(pid);
	return pid;
}
