#include <u.h>
#include <libc.h>
#include "9proc.h"

static Lock uproclock;
static Uproc *phash[PIDHASH];

Uproc*
_p9uproc(void)
{
	/* for now, assume getpid is fast or cached */
	int pid;
	Uproc *up;

	pid = getpid();
again:
if(0)print("find %d\n", pid);
	lock(&uproclock);
	for(up=phash[pid%PIDHASH]; up; up=up->next){
		if(up->pid == pid){
if(0)print("found %d\n", pid);
			unlock(&uproclock);
			return up;
		}
	}

	up = mallocz(sizeof(Uproc), 1);
	if(up == nil){
if(0)print("again %d\n", pid);
		unlock(&uproclock);
		sleep(1000);
		goto again;
	}

againpipe:
	if(pipe(up->pipe) < 0){
if(0)print("againpipe %d\n", pid);
		sleep(1000);
		goto againpipe;
	}

	up->pid = pid;
	up->next = phash[pid%PIDHASH];
	phash[pid%PIDHASH] = up;
if(0)print("link %d\n", pid);
	unlock(&uproclock);
	return up;
}

void
_p9uprocdie(void)
{
	Uproc **l, *up;
	int pid;

	pid = getpid();
if(0)print("die %d\n", pid);
	lock(&uproclock);
	for(l=&phash[pid%33]; *l; l=&(*l)->next){
		if((*l)->pid == pid){
			up = *l;
			*l = up->next;
if(0)print("died %d\n", pid);
			unlock(&uproclock);
			close(up->pipe[0]);
			close(up->pipe[1]);
			free(up);
			return;
		}
	}
if(0)print("not started %d\n", pid);
	unlock(&uproclock);
}
