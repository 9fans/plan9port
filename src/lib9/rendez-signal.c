/*
     NAME
          rendezvous - user level process synchronization

     SYNOPSIS
          ulong rendezvous(ulong tag, ulong value)

     DESCRIPTION
          The rendezvous system call allows two processes to synchro-
          nize and exchange a value.  In conjunction with the shared
          memory system calls (see segattach(2) and fork(2)), it
          enables parallel programs to control their scheduling.

          Two processes wishing to synchronize call rendezvous with a
          common tag, typically an address in memory they share.  One
          process will arrive at the rendezvous first; it suspends
          execution until a second arrives.  When a second process
          meets the rendezvous the value arguments are exchanged
          between the processes and returned as the result of the
          respective rendezvous system calls.  Both processes are
          awakened when the rendezvous succeeds.

          The set of tag values which two processes may use to
          rendezvous-their tag space-is inherited when a process
          forks, unless RFREND is set in the argument to rfork; see
          fork(2).

          If a rendezvous is interrupted the return value is ~0, so
          that value should not be used in normal communication.

 * This simulates rendezvous with shared memory, sigsuspend, and SIGUSR1.
 */

#include <u.h>
#include <signal.h>
#include <libc.h>

#define DBG 0

enum
{
	VOUSHASH = 257,
};

typedef struct Vous Vous;
struct Vous
{
	Vous *link;
	int pid;
	int wokeup;
	ulong tag;
	ulong val1;		/* value for the sleeper */
	ulong val2;		/* value for the waker */
};

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

static Vous vouspool[2048];
static int nvousused;
static Vous *vousfree;
static Vous *voushash[VOUSHASH];
static Lock vouslock;

static Vous*
getvous(void)
{
	Vous *v;

	if(vousfree){
		v = vousfree;
		vousfree = v->link;
	}else if(nvousused < nelem(vouspool))
		v = &vouspool[nvousused++];
	else{
		fprint(2, "rendezvous: out of vous!\n");
		abort();
	}
	return v;
}

static void
putvous(Vous *v)
{
	v->link = vousfree;
	vousfree = v;
}

static Vous*
findvous(ulong tag)
{
	int h;
	Vous *v, **l;

	h = tag%VOUSHASH;
	for(l=&voushash[h], v=*l; v; l=&(*l)->link, v=*l){
		if(v->tag == tag){
			*l = v->link;
			v->link = nil;
			return v;
		}
	}
	return nil;
}

static Vous*
mkvous(ulong tag)
{
	Vous *v;
	int h;

	h = tag%VOUSHASH;
	v = getvous();
	v->link = voushash[h];
	v->tag = tag;
	voushash[h] = v;
	return v;
}

ulong
rendezvous(ulong tag, ulong val)
{
	int vpid, pid;
	ulong rval;
	Vous *v;
	sigset_t mask;

	pid = getpid();
	lock(&vouslock);
	if((v = findvous(tag)) == nil){
		/*
		 * Go to sleep.
		 *
		 * Block USR1, set the handler to interrupt system calls,
		 * unlock the vouslock so our waker can wake us,
		 * and then suspend.
		 */
		v = mkvous(tag);
		v->pid = pid;
		v->val2 = val;
		v->wokeup = 0;
		sigprocmask(SIG_SETMASK, nil, &mask);
		sigaddset(&mask, SIGUSR1);
		sigprocmask(SIG_SETMASK, &mask, nil);
		ignusr1(0);
		if(DBG) fprint(2, "%d rv(%lux, %lux) -> s\n", pid, tag, val);
		unlock(&vouslock);
		sigdelset(&mask, SIGUSR1);
		sigsuspend(&mask);

		/*
		 * We're awake.  Make USR1 not interrupt system calls.
		 * Were we awakened or interrupted?
		 */
		ignusr1(1);
		lock(&vouslock);
		if(v->wokeup){
			rval = v->val1;
			if(DBG) fprint(2, "%d rv(%lux, %lux) -> g %lux\n", pid, tag, val, rval);
		}else{
			if(findvous(tag) != v){
				fprint(2, "rendezvous: interrupted but not found in hash table\n");
				abort();
			}
			rval = ~(ulong)0;
			if(DBG) fprint(2, "%d rv(%lux, %lux) -> g i\n", pid, tag, val);
		}
		putvous(v);
		unlock(&vouslock);
	}else{
		/*
		 * Wake up sleeper.
		 */
		rval = v->val2;
		v->val1 = val;
		vpid = v->pid;
		v->wokeup = 1;
		if(DBG) fprint(2, "%d rv(%lux, %lux) -> g %lux, w %d\n", pid, tag, val, rval, vpid);
		unlock(&vouslock);
		kill(vpid, SIGUSR1);
	}
	return rval;
}
