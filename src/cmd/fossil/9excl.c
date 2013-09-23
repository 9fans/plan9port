#include "stdinc.h"

#include "9.h"

static struct {
	VtLock*	lock;

	Excl*	head;
	Excl*	tail;
} ebox;

struct Excl {
	Fsys*	fsys;
	uvlong	path;
	ulong	time;

	Excl*	next;
	Excl*	prev;
};

enum {
	LifeTime	= (5*60),
};

int
exclAlloc(Fid* fid)
{
	ulong t;
	Excl *excl;

	assert(fid->excl == nil);

	t = time(0L);
	vtLock(ebox.lock);
	for(excl = ebox.head; excl != nil; excl = excl->next){
		if(excl->fsys != fid->fsys || excl->path != fid->qid.path)
			continue;
		/*
		 * Found it.
		 * Now, check if it's timed out.
		 * If not, return error, it's locked.
		 * If it has timed out, zap the old
		 * one and continue on to allocate a
		 * a new one.
		 */
		if(excl->time >= t){
			vtUnlock(ebox.lock);
			vtSetError("exclusive lock");
			return 0;
		}
		excl->fsys = nil;
	}

	/*
	 * Not found or timed-out.
	 * Alloc a new one and initialise.
	 */
	excl = vtMemAllocZ(sizeof(Excl));
	excl->fsys = fid->fsys;
	excl->path = fid->qid.path;
	excl->time = t+LifeTime;
	if(ebox.tail != nil){
		excl->prev = ebox.tail;
		ebox.tail->next = excl;
	}
	else{
		ebox.head = excl;
		excl->prev = nil;
	}
	ebox.tail = excl;
	excl->next = nil;
	vtUnlock(ebox.lock);

	fid->excl = excl;
	return 1;
}

int
exclUpdate(Fid* fid)
{
	ulong t;
	Excl *excl;

	excl = fid->excl;

	t = time(0L);
	vtLock(ebox.lock);
	if(excl->time < t || excl->fsys != fid->fsys){
		vtUnlock(ebox.lock);
		vtSetError("exclusive lock broken");
		return 0;
	}
	excl->time = t+LifeTime;
	vtUnlock(ebox.lock);

	return 1;
}

void
exclFree(Fid* fid)
{
	Excl *excl;

	if((excl = fid->excl) == nil)
		return;
	fid->excl = nil;

	vtLock(ebox.lock);
	if(excl->prev != nil)
		excl->prev->next = excl->next;
	else
		ebox.head = excl->next;
	if(excl->next != nil)
		excl->next->prev = excl->prev;
	else
		ebox.tail = excl->prev;
	vtUnlock(ebox.lock);

	vtMemFree(excl);
}

void
exclInit(void)
{
	ebox.lock = vtLockAlloc();
}
