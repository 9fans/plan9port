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

 * This simulates rendezvous with shared memory, pause, and SIGUSR1.
 */

#include <signal.h>
#include <lib9.h>

enum
{
	VOUSHASH = 257,
};

typedef struct Vous Vous;
struct Vous
{
	Vous *link;
	Lock lk;
	int pid;
	int wakeup;
	ulong val;
	ulong tag;
};

static void
ign(int x)
{
	USED(x);
}

void /*__attribute__((constructor))*/
ignusr1(void)
{
	signal(SIGUSR1, ign);
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
	else
		abort();
	return v;
}

static void
putvous(Vous *v)
{
	lock(&vouslock);
	v->link = vousfree;
	vousfree = v;
	unlock(&vouslock);
}

static Vous*
findvous(ulong tag, ulong val, int pid)
{
	int h;
	Vous *v, **l;

	lock(&vouslock);
	h = tag%VOUSHASH;
	for(l=&voushash[h], v=*l; v; l=&(*l)->link, v=*l){
		if(v->tag == tag){
			*l = v->link;
			unlock(&vouslock);
			return v;
		}
	}
	v = getvous();
	v->pid = pid;
	v->link = voushash[h];
	v->val = val;
	v->tag = tag;
	lock(&v->lk);
	voushash[h] = v;
	unlock(&vouslock);
	return v;
}

#define DBG 0
ulong
rendezvous(ulong tag, ulong val)
{
	int me, vpid;
	ulong rval;
	Vous *v;
	sigset_t mask;

	me = getpid();
	v = findvous(tag, val, me);
	if(v->pid == me){
		if(DBG)fprint(2, "pid is %d tag %lux, sleeping\n", me, tag);
		/*
		 * No rendezvous partner was found; the next guy
		 * through will find v and wake us, so we must go
		 * to sleep.
		 *
		 * To go to sleep:
		 *	1. disable USR1 signals.
		 *	2. unlock v->lk (tells waker okay to signal us).
		 *	3. atomically suspend and enable USR1 signals.
		 *
		 * The call to ignusr1() could be done once at 
		 * process creation instead of every time through rendezvous.
		 */
		v->val = val;
		ignusr1();
		sigprocmask(SIG_SETMASK, NULL, &mask);
		sigaddset(&mask, SIGUSR1);
		sigprocmask(SIG_SETMASK, &mask, NULL);
		sigdelset(&mask, SIGUSR1);
		v->wakeup = 0;
		unlock(&v->lk);
		for(;;){
			/*
			 * There may well be random signals flying around,
			 * so we can't be sure why we woke up.  If we weren't
			 * properly awakened, we need to go back to sleep.
			 */
			sigsuspend(&mask);
			lock(&v->lk);	/* do some memory synchronization */
			unlock(&v->lk);
			if(v->wakeup == 1)
				break;
		}
		rval = v->val;
		if(DBG)fprint(2, "pid is %d, awake\n", me);
		putvous(v);
	}else{
		/*
		 * Found someone to meet.  Wake him:
		 *
		 *	A. lock v->lk (waits for him to get to his step 2)
		 *	B. send a USR1
		 *
		 * He won't get the USR1 until he suspends, which
		 * means it must wake him up (it can't get delivered
		 * before he sleeps).
		 */
		vpid = v->pid;
		lock(&v->lk);
		rval = v->val;
		v->val = val;
		v->wakeup = 1;
		unlock(&v->lk);
		if(kill(vpid, SIGUSR1) < 0){
			if(DBG)fprint(2, "pid is %d, kill %d failed: %r\n", me, vpid);
			abort();
		}
	}
	return rval;
}

