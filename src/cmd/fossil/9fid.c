#include "stdinc.h"

#include "9.h"

static struct {
	VtLock*	lock;

	Fid*	free;
	int	nfree;
	int	inuse;
} fbox;

static void
fidLock(Fid* fid, int flags)
{
	if(flags & FidFWlock){
		vtLock(fid->lock);
		fid->flags = flags;
	}
	else
		vtRLock(fid->lock);

	/*
	 * Callers of file* routines are expected to lock fsys->fs->elk
	 * before making any calls in order to make sure the epoch doesn't
	 * change underfoot. With the exception of Tversion and Tattach,
	 * that implies all 9P functions need to lock on entry and unlock
	 * on exit. Fortunately, the general case is the 9P functions do
	 * fidGet on entry and fidPut on exit, so this is a convenient place
	 * to do the locking.
	 * No fsys->fs->elk lock is required if the fid is being created
	 * (Tauth, Tattach and Twalk). FidFCreate is always accompanied by
	 * FidFWlock so the setting and testing of FidFCreate here and in
	 * fidUnlock below is always done under fid->lock.
	 * A side effect is that fidFree is called with the fid locked, and
	 * must call fidUnlock only after it has disposed of any File
	 * resources still held.
	 */
	if(!(flags & FidFCreate))
		fsysFsRlock(fid->fsys);
}

static void
fidUnlock(Fid* fid)
{
	if(!(fid->flags & FidFCreate))
		fsysFsRUnlock(fid->fsys);
	if(fid->flags & FidFWlock){
		fid->flags = 0;
		vtUnlock(fid->lock);
		return;
	}
	vtRUnlock(fid->lock);
}

static Fid*
fidAlloc(void)
{
	Fid *fid;

	vtLock(fbox.lock);
	if(fbox.nfree > 0){
		fid = fbox.free;
		fbox.free = fid->hash;
		fbox.nfree--;
	}
	else{
		fid = vtMemAllocZ(sizeof(Fid));
		fid->lock = vtLockAlloc();
		fid->alock = vtLockAlloc();
	}
	fbox.inuse++;
	vtUnlock(fbox.lock);

	fid->con = nil;
	fid->fidno = NOFID;
	fid->ref = 0;
	fid->flags = 0;
	fid->open = FidOCreate;
	assert(fid->fsys == nil);
	assert(fid->file == nil);
	fid->qid = (Qid){0, 0, 0};
	assert(fid->uid == nil);
	assert(fid->uname == nil);
	assert(fid->db == nil);
	assert(fid->excl == nil);
	assert(fid->rpc == nil);
	assert(fid->cuname == nil);
	fid->hash = fid->next = fid->prev = nil;

	return fid;
}

static void
fidFree(Fid* fid)
{
	if(fid->file != nil){
		fileDecRef(fid->file);
		fid->file = nil;
	}
	if(fid->db != nil){
		dirBufFree(fid->db);
		fid->db = nil;
	}
	fidUnlock(fid);

	if(fid->uid != nil){
		vtMemFree(fid->uid);
		fid->uid = nil;
	}
	if(fid->uname != nil){
		vtMemFree(fid->uname);
		fid->uname = nil;
	}
	if(fid->excl != nil)
		exclFree(fid);
	if(fid->rpc != nil){
		close(fid->rpc->afd);
		auth_freerpc(fid->rpc);
		fid->rpc = nil;
	}
	if(fid->fsys != nil){
		fsysPut(fid->fsys);
		fid->fsys = nil;
	}
	if(fid->cuname != nil){
		vtMemFree(fid->cuname);
		fid->cuname = nil;
	}

	vtLock(fbox.lock);
	fbox.inuse--;
	if(fbox.nfree < 10){
		fid->hash = fbox.free;
		fbox.free = fid;
		fbox.nfree++;
	}
	else{
		vtLockFree(fid->alock);
		vtLockFree(fid->lock);
		vtMemFree(fid);
	}
	vtUnlock(fbox.lock);
}

static void
fidUnHash(Fid* fid)
{
	Fid *fp, **hash;

	assert(fid->ref == 0);

	hash = &fid->con->fidhash[fid->fidno % NFidHash];
	for(fp = *hash; fp != nil; fp = fp->hash){
		if(fp == fid){
			*hash = fp->hash;
			break;
		}
		hash = &fp->hash;
	}
	assert(fp == fid);

	if(fid->prev != nil)
		fid->prev->next = fid->next;
	else
		fid->con->fhead = fid->next;
	if(fid->next != nil)
		fid->next->prev = fid->prev;
	else
		fid->con->ftail = fid->prev;
	fid->prev = fid->next = nil;

	fid->con->nfid--;
}

Fid*
fidGet(Con* con, u32int fidno, int flags)
{
	Fid *fid, **hash;

	if(fidno == NOFID)
		return nil;

	hash = &con->fidhash[fidno % NFidHash];
	vtLock(con->fidlock);
	for(fid = *hash; fid != nil; fid = fid->hash){
		if(fid->fidno != fidno)
			continue;

		/*
		 * Already in use is an error
		 * when called from attach, clone or walk.
		 */
		if(flags & FidFCreate){
			vtUnlock(con->fidlock);
			vtSetError("%s: fid 0x%ud in use", argv0, fidno);
			return nil;
		}
		fid->ref++;
		vtUnlock(con->fidlock);

		fidLock(fid, flags);
		if((fid->open & FidOCreate) || fid->fidno == NOFID){
			fidPut(fid);
			vtSetError("%s: fid invalid", argv0);
			return nil;
		}
		return fid;
	}

	if((flags & FidFCreate) && (fid = fidAlloc()) != nil){
		assert(flags & FidFWlock);
		fid->con = con;
		fid->fidno = fidno;
		fid->ref = 1;

		fid->hash = *hash;
		*hash = fid;
		if(con->ftail != nil){
			fid->prev = con->ftail;
			con->ftail->next = fid;
		}
		else{
			con->fhead = fid;
			fid->prev = nil;
		}
		con->ftail = fid;
		fid->next = nil;

		con->nfid++;
		vtUnlock(con->fidlock);

		/*
		 * The FidOCreate flag is used to prevent any
		 * accidental access to the Fid between unlocking the
		 * hash and acquiring the Fid lock for return.
		 */
		fidLock(fid, flags);
		fid->open &= ~FidOCreate;
		return fid;
	}
	vtUnlock(con->fidlock);

	vtSetError("%s: fid not found", argv0);
	return nil;
}

void
fidPut(Fid* fid)
{
	vtLock(fid->con->fidlock);
	assert(fid->ref > 0);
	fid->ref--;
	vtUnlock(fid->con->fidlock);

	if(fid->ref == 0 && fid->fidno == NOFID){
		fidFree(fid);
		return;
	}
	fidUnlock(fid);
}

void
fidClunk(Fid* fid)
{
	assert(fid->flags & FidFWlock);

	vtLock(fid->con->fidlock);
	assert(fid->ref > 0);
	fid->ref--;
	fidUnHash(fid);
	fid->fidno = NOFID;
	vtUnlock(fid->con->fidlock);

	if(fid->ref > 0){
		/* not reached - fidUnHash requires ref == 0 */
		fidUnlock(fid);
		return;
	}
	fidFree(fid);
}

void
fidClunkAll(Con* con)
{
	Fid *fid;
	u32int fidno;

	vtLock(con->fidlock);
	while(con->fhead != nil){
		fidno = con->fhead->fidno;
		vtUnlock(con->fidlock);
		if((fid = fidGet(con, fidno, FidFWlock)) != nil)
			fidClunk(fid);
		vtLock(con->fidlock);
	}
	vtUnlock(con->fidlock);
}

void
fidInit(void)
{
	fbox.lock = vtLockAlloc();
}
