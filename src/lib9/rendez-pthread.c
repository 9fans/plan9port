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

 * This assumes we're using pthreads and simulates rendezvous using
 * shared memory and mutexes.
 */

#include <pthread.h>
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
	ulong val;
	ulong tag;
	pthread_mutex_t mutex;
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
	}else if(nvousused < nelem(vouspool)){
		v = &vouspool[nvousused++];
		pthread_mutex_init(&v->mutex, NULL);
	}else
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
findvous(ulong tag, ulong val, int *found)
{
	int h;
	Vous *v, **l;

	lock(&vouslock);
	h = tag%VOUSHASH;
	for(l=&voushash[h], v=*l; v; l=&(*l)->link, v=*l){
		if(v->tag == tag){
			*l = v->link;
			*found = 1;
			unlock(&vouslock);
			return v;
		}
	}
	v = getvous();
	v->link = voushash[h];
	v->val = val;
	v->tag = tag;
	lock(&v->lk);
	voushash[h] = v;
	unlock(&vouslock);
	*found = 0;
	return v;
}

#define DBG 0
ulong
rendezvous(ulong tag, ulong val)
{
	int found;
	ulong rval;
	Vous *v;

	v = findvous(tag, val, &found);
	if(!found){
		if(DBG)fprint(2, "tag %lux, sleeping on %p\n", tag, v);
		/*
		 * No rendezvous partner was found; the next guy
		 * through will find v and wake us, so we must go
		 * to sleep.  Do this by locking the mutex (it is
		 * unlocked) and then locking it again (our waker will
		 * unlock it for us).
		 */
		if(pthread_mutex_lock(&v->mutex) != 0)
			abort();
		unlock(&v->lk);
		if(pthread_mutex_lock(&v->mutex) != 0)
			abort();
		rval = v->val;
		pthread_mutex_unlock(&v->mutex);
		if(DBG)fprint(2, " awake on %p\n", v);
		unlock(&v->lk);
		putvous(v);
	}else{
		/*
		 * Found someone to meet.  Wake him:
		 *
		 *	A. lock v->lk (waits for him to lock the mutex once.
		 *	B. unlock the mutex (wakes him up)
		 */
		if(DBG)fprint(2, "found tag %lux on %p, waking\n", tag, v);
		lock(&v->lk);
		rval = v->val;
		v->val = val;
		if(pthread_mutex_unlock(&v->mutex) != 0)
			abort();
		/* lock passes to him */
	}
	return rval;
}

