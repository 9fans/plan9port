/*
 * This needs to be callable from a signal handler, so it has been
 * written to avoid locks.  The only lock is the one used to acquire
 * an entry in the table, and we make sure that acquiring is done
 * when not in a handler.  Lookup and delete do not need locks.
 * It's a scan-forward hash table.  To avoid breaking chains, 
 * T ((void*)-1) is used as a non-breaking nil.
 */

#include <u.h>
#include <libc.h>
#include "9proc.h"

enum { PIDHASH = 1021 };

#define T ((void*)-1)
static Uproc *alluproc[PIDHASH];
static int allupid[PIDHASH];
static Lock uproclock;

Uproc*
_p9uproc(int inhandler)
{
	int i, h, pid;
	Uproc *up;

	/* for now, assume getpid is fast or cached */
	pid = getpid();

	/*
	 * this part - the lookup - needs to run without locks
	 * so that it can safely be called from within the notify handler.
	 * notify calls _p9uproc, and fork and rfork call _p9uproc
	 * in both parent and child, so if we're in a signal handler,
	 * we should find something in the table.
	 */
	h = pid%PIDHASH;
	for(i=0; i<PIDHASH; i++){
		up = alluproc[h];
		if(up == nil)
			break;
		if(allupid[h] == pid)
			return up;
		if(++h == PIDHASH)
			h = 0;
	}

	if(inhandler){
		fprint(2, "%s: did not find uproc for pid %d in signal handler\n", argv0, pid);
		abort();	
	}

	/* need to allocate */
	while((up = mallocz(sizeof(Uproc), 1)) == nil)
		sleep(1000);

fprint(2, "alloc uproc for pid %d\n", pid);
	up->pid = pid;
	lock(&uproclock);
	h = pid%PIDHASH;
	for(i=0; i<PIDHASH; i++){
		if(alluproc[h]==T || alluproc[h]==nil){
			alluproc[h] = up;
			allupid[h] = pid;
			unlock(&uproclock);
			return up;
		}
		if(++h == PIDHASH)
			h = 0;
	}
	unlock(&uproclock);

	/* out of pids! */
	sysfatal("too many processes in uproc table");
	return nil;
}

void
_p9uprocdie(void)
{
	Uproc *up;
	int pid, i, h;

	pid = getpid();
fprint(2, "reap uproc for pid %d\n", pid);
	h = pid%PIDHASH;
	for(i=0; i<PIDHASH; i++){
		up = alluproc[h];
		if(up == nil)
			break;
		if(up == T)
			continue;
		if(allupid[h] == pid){
			up = alluproc[h];
			alluproc[h] = T;
			free(up);
			allupid[h] = 0;
		}
	}
}
