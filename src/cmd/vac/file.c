#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

/*
 * locking order is upwards.  A thread can hold the lock for a VacFile
 * and then acquire the lock of its parent
 */

struct VacFile {
	/* meta data for file: protected by the lk in the parent */
	int ref;		/* holds this data structure up */
	VacFS *fs;		/* immutable */

	int	removed;	/* file has been removed */
	int	dirty;		/* dir is dirty with respect to meta data in block */
	ulong	block;		/* block offset withing msource for this file's meta data */

	VacDir dir;		/* meta data for this file */

	VacFile *up;	/* parent file */
	VacFile *next;	/* sibling */

	/* data for file */
	VtLock *lk;		/* lock for source and msource */
	Source *source;
	Source *msource;	/* for directories: meta data for children */
	VacFile *down;		/* children */
};

static int vfMetaFlush(VacFile*);
static ulong msAlloc(Source *ms, ulong, int n);

static void
vfRUnlock(VacFile *vf)
{
	vtRUnlock(vf->lk);
}
	

static int
vfRLock(VacFile *vf)
{
	vtRLock(vf->lk);
	if(vf->source == nil) {
		vfRUnlock(vf);
		vtSetError(ERemoved);
		return 0;
	}
	return 1;
}

static void
vfUnlock(VacFile *vf)
{
	vtUnlock(vf->lk);
}

static int
vfLock(VacFile *vf)
{
	vtLock(vf->lk);
	if(vf->source == nil) {
		vfUnlock(vf);
		vtSetError(ERemoved);
		return 0;
	}
	return 1;
}

static void
vfMetaLock(VacFile *vf)
{
	assert(vf->up->msource != nil);
	vtLock(vf->up->lk);
}

static void
vfMetaUnlock(VacFile *vf)
{
	vtUnlock(vf->up->lk);
}


static void
vfRAccess(VacFile* vf)
{
	vfMetaLock(vf);
	vf->dir.atime = time(0L);
	vf->dirty = 1;
	vfMetaUnlock(vf);
	vfMetaFlush(vf);
}

static void
vfWAccess(VacFile* vf, char *mid)
{
	vfMetaLock(vf);
	vf->dir.atime = vf->dir.mtime = time(0L);
	if(strcmp(vf->dir.mid, mid) != 0) {
		vtMemFree(vf->dir.mid);
		vf->dir.mid = vtStrDup(mid);
	}
	vf->dir.mcount++;
	vf->dirty = 1;
	vfMetaUnlock(vf);
	vfMetaFlush(vf);
}

void
vdCleanup(VacDir *dir)
{
	vtMemFree(dir->elem);
	dir->elem = nil;
	vtMemFree(dir->uid);
	dir->uid = nil;
	vtMemFree(dir->gid);
	dir->gid = nil;
	vtMemFree(dir->mid);
	dir->mid = nil;
}

void
vdCopy(VacDir *dst, VacDir *src)
{
	*dst = *src;
	dst->elem = vtStrDup(src->elem);
	dst->uid = vtStrDup(src->uid);
	dst->gid = vtStrDup(src->gid);
	dst->mid = vtStrDup(src->mid);
}

static int
mbSearch(MetaBlock *mb, char *elem, int *ri, MetaEntry *me)
{
	int i;
	int b, t, x;

	/* binary search within block */
	b = 0;
	t = mb->nindex;
	while(b < t) {
		i = (b+t)>>1;
		if(!meUnpack(me, mb, i))
			return 0;
		if(mb->unbotch)
			x = meCmpNew(me, elem);
		else
			x = meCmp(me, elem);

		if(x == 0) {
			*ri = i;
			return 1;
		}
	
		if(x < 0)
			b = i+1;
		else /* x > 0 */
			t = i;
	}

	assert(b == t);
	
	*ri = b;	/* b is the index to insert this entry */
	memset(me, 0, sizeof(*me));

	return 1;
}

static void
mbInit(MetaBlock *mb, uchar *p, int n)
{
	memset(mb, 0, sizeof(MetaBlock));
	mb->maxsize = n;
	mb->buf = p;
	mb->maxindex = n/100;
	mb->size = MetaHeaderSize + mb->maxindex*MetaIndexSize;
}

static int
vfMetaFlush(VacFile *vf)
{
	VacFile *vfp;
	Lump *u;
	MetaBlock mb;
	MetaEntry me, nme;
	uchar *p;
	int i, n, moved;

//print("vfMetaFlush %s\n", vf->dir.elem);

	/* assume name has not changed for the moment */

	vfMetaLock(vf);

	vfp = vf->up;
	moved = 0;

	u = sourceGetLump(vfp->msource, vf->block, 0, 1);
	if(u == nil)
		goto Err;

	if(!mbUnpack(&mb, u->data, u->asize))
		goto Err;
	if(!mbSearch(&mb, vf->dir.elem, &i, &me) || me.p == nil)
		goto Err;

	nme = me;
	n = vdSize(&vf->dir);
//print("old size %d new size %d\n", me.size, n);
	if(n <= nme.size) {
		nme.size = n;
	} else {
		/* try expand entry? */
		p = mbAlloc(&mb, n);
//print("alloced %ld\n", p - mb.buf);
		if(p == nil) {
assert(0);
			/* much more work */
		}
		nme.p = p;
		nme.size = n;
	}

	mbDelete(&mb, i, &me);
	memset(me.p, 0, me.size);
	if(!moved) {
		vdPack(&vf->dir, &nme);
		mbInsert(&mb, i, &nme);
	}

	mbPack(&mb);
	lumpDecRef(u, 1);

	vf->dirty = 0;

	vfMetaUnlock(vf);
	return 1;

Err:
	lumpDecRef(u, 1);
	vfMetaUnlock(vf);
	return 0;
}

static VacFile *
vfAlloc(VacFS *fs)
{
	VacFile *vf;

	vf = vtMemAllocZ(sizeof(VacFile));
	vf->lk = vtLockAlloc();
	vf->ref = 1;
	vf->fs = fs;
	return vf;
}

static void
vfFree(VacFile *vf)
{
	sourceFree(vf->source);
	vtLockFree(vf->lk);	
	sourceFree(vf->msource);
	vdCleanup(&vf->dir);
	
	vtMemFree(vf);
}

/* the file is locked already */
static VacFile *
dirLookup(VacFile *vf, char *elem)
{
	int i, j, nb;
	MetaBlock mb;
	MetaEntry me;
	Lump *u;
	Source *meta;
	VacFile *nvf;

	meta = vf->msource;
	u = nil;
	nb = sourceGetNumBlocks(meta);
	for(i=0; i<nb; i++) {
		u = sourceGetLump(meta, i, 1, 1);
		if(u == nil)
			goto Err;
		if(!mbUnpack(&mb, u->data, u->asize))
			goto Err;
		if(!mbSearch(&mb, elem, &j, &me))
			goto Err;
		if(me.p != nil) {
			nvf = vfAlloc(vf->fs);
			if(!vdUnpack(&nvf->dir, &me)) {
				vfFree(nvf);
				goto Err;
			}
			lumpDecRef(u, 1);
			nvf->block = i;
			return nvf;
		}
		
		lumpDecRef(u, 1);
		u = nil;
	}
	vtSetError("file does not exist");
	/* fall through */
Err:
	lumpDecRef(u, 1);
	return nil;
}

VacFile *
vfRoot(VacFS *fs, uchar *score)
{
	VtEntry e;
	Lump *u, *v;
	Source *r, *r0, *r1, *r2;
	MetaBlock mb;
	MetaEntry me;
	VacFile *root, *mr;

	root = nil;
	mr = nil;
	r0 = nil;
	r1 = nil;
	r2 = nil;
	v = nil;
	r = nil;

	u = cacheGetLump(fs->cache, score, VtDirType, fs->bsize);
	if(u == nil)
		goto Err;
	if(!fs->readOnly) {
		v = cacheAllocLump(fs->cache, VtDirType, fs->bsize, 1);
		if(v == nil) {
			vtUnlock(u->lk);
			goto Err;
		}
		v->gen = u->gen;
		v->asize = u->asize;
		v->state = LumpActive;
		memmove(v->data, u->data, v->asize);
		lumpDecRef(u, 1);
		u = v;
		v = nil;
	}
	vtUnlock(u->lk);
	vtEntryUnpack(&e, u->data, 2);
	if(e.flags == 0){		/* just one entry */
		r = sourceAlloc(fs->cache, u, 0, 0, fs->readOnly);
		if(r == nil)
			goto Err;
		r0 = sourceOpen(r, 0, fs->readOnly);
		if(r0 == nil)
			goto Err;
		r1 = sourceOpen(r, 1, fs->readOnly);
		if(r1 == nil)
			goto Err;
		r2 = sourceOpen(r, 2, fs->readOnly);
		if(r2 == nil)
			goto Err;
		sourceFree(r);
		r = nil;
	}else{
		r0 = sourceAlloc(fs->cache, u, 0, 0, fs->readOnly);
		if(r0 == nil)
			goto Err;
		r1 = sourceAlloc(fs->cache, u, 0, 1, fs->readOnly);
		if(r1 == nil)
			goto Err;
		r2 = sourceAlloc(fs->cache, u, 0, 2, fs->readOnly);
		if(r2 == nil)
			goto Err;
	}
	lumpDecRef(u, 0);
	u = sourceGetLump(r2, 0, 1, 0);
	if(u == nil)
		goto Err;

	mr = vfAlloc(fs);
	mr->msource = r2;
	r2 = nil;

	root = vfAlloc(fs);
	root->up = mr;
	root->source = r0;
	r0 = nil;
	root->msource = r1;
	r1 = nil;

	mr->down = root;

	if(!mbUnpack(&mb, u->data, u->asize))
		goto Err;

	if(!meUnpack(&me, &mb, 0))
		goto Err;
	if(!vdUnpack(&root->dir, &me))
		goto Err;

	vfRAccess(root);
	lumpDecRef(u, 0);
	sourceFree(r2);

	return root;
Err:
	lumpDecRef(u, 0);
	lumpDecRef(v, 0);
	if(r0)
		sourceFree(r0);
	if(r1)
		sourceFree(r1);
	if(r2)
		sourceFree(r2);
	if(r)
		sourceFree(r);
	if(mr)
		vfFree(mr);
	if(root)
		vfFree(root);

	return nil;
}

VacFile *
vfWalk(VacFile *vf, char *elem)
{
	VacFile *nvf;

	vfRAccess(vf);

	if(elem[0] == 0) {
		vtSetError("illegal path element");
		return nil;
	}
	if(!vfIsDir(vf)) {
		vtSetError("not a directory");
		return nil;
	}

	if(strcmp(elem, ".") == 0) {
		return vfIncRef(vf);
	}

	if(strcmp(elem, "..") == 0) {
		if(vfIsRoot(vf))
			return vfIncRef(vf);
		return vfIncRef(vf->up);
	}

	if(!vfLock(vf))
		return nil;

	for(nvf = vf->down; nvf; nvf=nvf->next) {
		if(strcmp(elem, nvf->dir.elem) == 0 && !nvf->removed) {
			nvf->ref++;
			goto Exit;
		}
	}

	nvf = dirLookup(vf, elem);
	if(nvf == nil)
		goto Err;
	nvf->source = sourceOpen(vf->source, nvf->dir.entry, vf->fs->readOnly);
	if(nvf->source == nil)
		goto Err;
	if(nvf->dir.mode & ModeDir) {
		nvf->msource = sourceOpen(vf->source, nvf->dir.mentry, vf->fs->readOnly);
		if(nvf->msource == nil)
			goto Err;
	}

	/* link in and up parent ref count */
	nvf->next = vf->down;
	vf->down = nvf;
	nvf->up = vf;
	vfIncRef(vf);
Exit:
	vfUnlock(vf);
	return nvf;
Err:
	vfUnlock(vf);
	if(nvf != nil)
		vfFree(nvf);
	return nil;
}

VacFile *
vfOpen(VacFS *fs, char *path)
{
	VacFile *vf, *nvf;
	char *p, elem[VtMaxStringSize];
	int n;

	vf = fs->root;
	vfIncRef(vf);
	while(*path != 0) {
		for(p = path; *p && *p != '/'; p++)
			;
		n = p - path;
		if(n > 0) {
			if(n > VtMaxStringSize) {
				vtSetError("path element too long");
				goto Err;
			}
			memmove(elem, path, n);
			elem[n] = 0;
			nvf = vfWalk(vf, elem);
			if(nvf == nil)
				goto Err;
			vfDecRef(vf);
			vf = nvf;
		}
		if(*p == '/')
			p++;
		path = p;
	}
	return vf;
Err:
	vfDecRef(vf);
	return nil;
}

VacFile *
vfCreate(VacFile *vf, char *elem, ulong mode, char *user)
{
	VacFile *nvf;
	VacDir *dir;
	int n, i;
	uchar *p;
	Source *pr, *r, *mr;
	int isdir;
	MetaBlock mb;
	MetaEntry me;
	Lump *u;

	if(!vfLock(vf))
		return nil;

	r = nil;
	mr = nil;
	u = nil;

	for(nvf = vf->down; nvf; nvf=nvf->next) {
		if(strcmp(elem, nvf->dir.elem) == 0 && !nvf->removed) {
			nvf = nil;
			vtSetError(EExists);
			goto Err;
		}
	}

	nvf = dirLookup(vf, elem);
	if(nvf != nil) {
		vtSetError(EExists);
		goto Err;
	}

	nvf = vfAlloc(vf->fs);
	isdir = mode & ModeDir;

	pr = vf->source;
	r = sourceCreate(pr, pr->psize, pr->dsize, isdir, 0);
	if(r == nil)
		goto Err;
	if(isdir) {
		mr = sourceCreate(pr, pr->psize, pr->dsize, 0, r->block*pr->epb + r->entry);
		if(mr == nil)
			goto Err;
	}
	
	dir = &nvf->dir;
	dir->elem = vtStrDup(elem);
	dir->entry = r->block*pr->epb + r->entry;
	dir->gen = r->gen;
	if(isdir) {
		dir->mentry = mr->block*pr->epb + mr->entry;
		dir->mgen = mr->gen;
	}
	dir->size = 0;
	dir->qid = vf->fs->qid++;
	dir->uid = vtStrDup(user);
	dir->gid = vtStrDup(vf->dir.gid);
	dir->mid = vtStrDup(user);
	dir->mtime = time(0L);
	dir->mcount = 0;
	dir->ctime = dir->mtime;
	dir->atime = dir->mtime;
	dir->mode = mode;

	n = vdSize(dir);
	nvf->block = msAlloc(vf->msource, 0, n);
	if(nvf->block == NilBlock)
		goto Err;
	u = sourceGetLump(vf->msource, nvf->block, 0, 1);
	if(u == nil)
		goto Err;
	if(!mbUnpack(&mb, u->data, u->asize))
		goto Err;
	p = mbAlloc(&mb, n);
	if(p == nil)
		goto Err;
		
	if(!mbSearch(&mb, elem, &i, &me))
		goto Err;
	assert(me.p == nil);
	me.p = p;
	me.size = n;

	vdPack(dir, &me);
	mbInsert(&mb, i, &me);
	mbPack(&mb);
	lumpDecRef(u, 1);

	nvf->source = r;
	nvf->msource = mr;

	/* link in and up parent ref count */
	nvf->next = vf->down;
	vf->down = nvf;
	nvf->up = vf;
	vfIncRef(vf);

	vfWAccess(vf, user);

	vfUnlock(vf);
	return nvf;

Err:
	lumpDecRef(u, 1);
	if(r)
		sourceRemove(r);
	if(mr)
		sourceRemove(mr);
	if(nvf)
		vfFree(nvf);
	vfUnlock(vf);
	return 0;
}


int
vfRead(VacFile *vf, void *buf, int cnt, vlong offset)
{
	Source *s;
	uvlong size;
	ulong bn;
	int off, dsize, n, nn;
	Lump *u;
	uchar *b;

if(0)fprint(2, "vfRead: %s %d, %lld\n", vf->dir.elem, cnt, offset);

	if(!vfRLock(vf))
		return -1;

	s = vf->source;

	dsize = s->dsize;
	size = sourceGetSize(s);

	if(offset < 0) {
		vtSetError(EBadOffset);
		goto Err;
	}

	vfRAccess(vf);

	if(offset >= size)
		offset = size;

	if(cnt > size-offset)
		cnt = size-offset;
	bn = offset/dsize;
	off = offset%dsize;
	b = buf;
	while(cnt > 0) {
		u = sourceGetLump(s, bn, 1, 0);
		if(u == nil)
			goto Err;
		if(u->asize <= off) {
			lumpDecRef(u, 0);
			goto Err;
		}
		n = cnt;
		if(n > dsize-off)
			n = dsize-off;
		nn = u->asize-off;
		if(nn > n)
			nn = n;
		memmove(b, u->data+off, nn);
		memset(b+nn, 0, n-nn);
		off = 0;
		bn++;
		cnt -= n;
		b += n;
		lumpDecRef(u, 0);
	}
	vfRUnlock(vf);
	return b-(uchar*)buf;
Err:
	vfRUnlock(vf);
	return -1;
}

int
vfWrite(VacFile *vf, void *buf, int cnt, vlong offset, char *user)
{
	Source *s;
	ulong bn;
	int off, dsize, n;
	Lump *u;
	uchar *b;

	USED(user);

	if(!vfLock(vf))
		return -1;

	if(vf->fs->readOnly) {
		vtSetError(EReadOnly);
		goto Err;
	}

	if(vf->dir.mode & ModeDir) {
		vtSetError(ENotFile);
		goto Err;
	}
if(0)fprint(2, "vfWrite: %s %d, %lld\n", vf->dir.elem, cnt, offset);

	s = vf->source;
	dsize = s->dsize;

	if(offset < 0) {
		vtSetError(EBadOffset);
		goto Err;
	}

	vfWAccess(vf, user);

	bn = offset/dsize;
	off = offset%dsize;
	b = buf;
	while(cnt > 0) {
		n = cnt;
		if(n > dsize-off)
			n = dsize-off;
		if(!sourceSetDepth(s, offset+n))
			goto Err;
		u = sourceGetLump(s, bn, 0, 0);
		if(u == nil)
			goto Err;
		if(u->asize < dsize) {
			vtSetError("runt block");
			lumpDecRef(u, 0);
			goto Err;
		}
		memmove(u->data+off, b, n);
		off = 0;
		cnt -= n;
		b += n;
		offset += n;
		bn++;
		lumpDecRef(u, 0);
		if(!sourceSetSize(s, offset))
			goto Err;
	}
	vfLock(vf);
	return b-(uchar*)buf;
Err:
	vfLock(vf);
	return -1;
}

int
vfGetDir(VacFile *vf, VacDir *dir)
{
	if(!vfRLock(vf))
		return 0;

	vfMetaLock(vf);
	vdCopy(dir, &vf->dir);
	vfMetaUnlock(vf);

	if(!vfIsDir(vf))
		dir->size = sourceGetSize(vf->source);
	vfRUnlock(vf);

	return 1;
}

uvlong
vfGetId(VacFile *vf)
{
	/* immutable */
	return vf->dir.qid;
}

ulong
vfGetMcount(VacFile *vf)
{
	ulong mcount;
	
	vfMetaLock(vf);
	mcount = vf->dir.mcount;
	vfMetaUnlock(vf);
	return mcount;
}


int
vfIsDir(VacFile *vf)
{
	/* immutable */
	return (vf->dir.mode & ModeDir) != 0;
}

int
vfIsRoot(VacFile *vf)
{
	return vf == vf->fs->root;
}

int
vfGetSize(VacFile *vf, uvlong *size)
{
	if(!vfRLock(vf))
		return 0;
	*size = sourceGetSize(vf->source);
	vfRUnlock(vf);

	return 1;
}

static int
vfMetaRemove(VacFile *vf, char *user)
{
	Lump *u;
	MetaBlock mb;
	MetaEntry me;
	int i;
	VacFile *vfp;

	vfp = vf->up;

	vfWAccess(vfp, user);

	vfMetaLock(vf);

	u = sourceGetLump(vfp->msource, vf->block, 0, 1);
	if(u == nil)
		goto Err;

	if(!mbUnpack(&mb, u->data, u->asize))
		goto Err;
	if(!mbSearch(&mb, vf->dir.elem, &i, &me) || me.p == nil)
		goto Err;
print("deleting %d entry\n", i);
	mbDelete(&mb, i, &me);
	memset(me.p, 0, me.size);
	mbPack(&mb);
	
	lumpDecRef(u, 1);
	
	vf->removed = 1;
	vf->block = NilBlock;

	vfMetaUnlock(vf);
	return 1;

Err:
	lumpDecRef(u, 1);
	vfMetaUnlock(vf);
	return 0;
}


static int
vfCheckEmpty(VacFile *vf)
{
	int i, n;
	Lump *u;
	MetaBlock mb;
	Source *r;

	r = vf->msource;
	n = sourceGetNumBlocks(r);
	for(i=0; i<n; i++) {
		u = sourceGetLump(r, i, 1, 1);
		if(u == nil)
			goto Err;
		if(!mbUnpack(&mb, u->data, u->asize))
			goto Err;
		if(mb.nindex > 0) {
			vtSetError(ENotEmpty);
			goto Err;
		}
		lumpDecRef(u, 1);
	}
	return 1;
Err:
	lumpDecRef(u, 1);
	return 0;
}

int
vfRemove(VacFile *vf, char *user)
{	
	/* can not remove the root */
	if(vfIsRoot(vf)) {
		vtSetError(ERoot);
		return 0;
	}

	if(!vfLock(vf))
		return 0;

	if(vfIsDir(vf) && !vfCheckEmpty(vf))
		goto Err;
			
	assert(vf->down == nil);

	sourceRemove(vf->source);
	vf->source = nil;
	if(vf->msource) {
		sourceRemove(vf->msource);
		vf->msource = nil;
	}
	
	vfUnlock(vf);
	
	if(!vfMetaRemove(vf, user))
		return 0;
	
	return 1;
		
Err:
	vfUnlock(vf);
	return 0;
}

VacFile *
vfIncRef(VacFile *vf)
{
	vfMetaLock(vf);
	assert(vf->ref > 0);
	vf->ref++;
	vfMetaUnlock(vf);
	return vf;
}

void
vfDecRef(VacFile *vf)
{
	VacFile *p, *q, **qq;

	if(vf->up == nil) {
		vfFree(vf);
		return;
	}

	vfMetaLock(vf);
	vf->ref--;
	if(vf->ref > 0) {
		vfMetaUnlock(vf);
		return;
	}
	assert(vf->ref == 0);
	assert(vf->down == nil);

	p = vf->up;
	qq = &p->down;
	for(q = *qq; q; qq=&q->next,q=*qq)
		if(q == vf)
			break;
	assert(q != nil);
	*qq = vf->next;

	vfMetaUnlock(vf);
	vfFree(vf);

	vfDecRef(p);
}

int
vfGetVtEntry(VacFile *vf, VtEntry *e)
{
	int res;

	if(!vfRLock(vf))
		return 0;
	res = sourceGetVtEntry(vf->source, e);
	vfRUnlock(vf);
	return res;
}

int
vfGetBlockScore(VacFile *vf, ulong bn, uchar score[VtScoreSize])
{
	Lump *u;
	int ret, off;
	Source *r;

	if(!vfRLock(vf))
		return 0;

	r = vf->source;

	u = sourceWalk(r, bn, 1, &off);
	if(u == nil){
		vfRUnlock(vf);
		return 0;
	}

	ret = lumpGetScore(u, off, score);
	lumpDecRef(u, 0);
	vfRUnlock(vf);

	return ret;
}

VacFile *
vfGetParent(VacFile *vf)
{
	if(vfIsRoot(vf))
		return vfIncRef(vf);
	return vfIncRef(vf->up);
}

static VacDirEnum *
vdeAlloc(VacFile *vf)
{
	VacDirEnum *ds;

	if(!(vf->dir.mode & ModeDir)) {
		vtSetError(ENotDir);
		vfDecRef(vf);
		return nil;
	}

	ds = vtMemAllocZ(sizeof(VacDirEnum));
	ds->file = vf;
	
	return ds;
}

VacDirEnum *
vdeOpen(VacFS *fs, char *path)
{
	VacFile *vf;

	vf = vfOpen(fs, path);
	if(vf == nil)
		return nil;

	return vdeAlloc(vf);
}

VacDirEnum *
vfDirEnum(VacFile *vf)
{
	return vdeAlloc(vfIncRef(vf));
}

static int
dirEntrySize(Source *s, ulong elem, ulong gen, uvlong *size)
{
	Lump *u;
	ulong bn;
	VtEntry e;

	bn = elem/s->epb;
	elem -= bn*s->epb;

	u = sourceGetLump(s, bn, 1, 1);
	if(u == nil)
		goto Err;
	if(u->asize < (elem+1)*VtEntrySize) {
		vtSetError(ENoDir);
		goto Err;
	}
	vtEntryUnpack(&e, u->data, elem);
	if(!(e.flags & VtEntryActive) || e.gen != gen) {
fprint(2, "gen mismatch\n");
		vtSetError(ENoDir);
		goto Err;
	}

	*size = e.size;
	lumpDecRef(u, 1);
	return 1;	

Err:
	lumpDecRef(u, 1);
	return 0;
}

int
vdeRead(VacDirEnum *ds, VacDir *dir, int n)
{
	ulong nb;
	int i;
	Source *meta, *source;
	MetaBlock mb;
	MetaEntry me;
	Lump *u;

	vfRAccess(ds->file);

	if(!vfRLock(ds->file))
		return -1;

	i = 0;
	u = nil;
	source = ds->file->source;
	meta = ds->file->msource;
	nb = (sourceGetSize(meta) + meta->dsize - 1)/meta->dsize;

	if(ds->block >= nb)
		goto Exit;
	u = sourceGetLump(meta, ds->block, 1, 1);
	if(u == nil)
		goto Err;
	if(!mbUnpack(&mb, u->data, u->asize))
		goto Err;

	for(i=0; i<n; i++) {
		while(ds->index >= mb.nindex) {
			lumpDecRef(u, 1);
			u = nil;
			ds->index = 0;
			ds->block++;
			if(ds->block >= nb)
				goto Exit;
			u = sourceGetLump(meta, ds->block, 1, 1);
			if(u == nil)
				goto Err;
			if(!mbUnpack(&mb, u->data, u->asize))
				goto Err;
		}
		if(!meUnpack(&me, &mb, ds->index))
			goto Err;
		if(dir != nil) {
			if(!vdUnpack(&dir[i], &me))
				goto Err;
			if(!(dir[i].mode & ModeDir))
			if(!dirEntrySize(source, dir[i].entry, dir[i].gen, &dir[i].size))
				goto Err;
		}
		ds->index++;
	}
Exit:
	lumpDecRef(u, 1);
	vfRUnlock(ds->file);
	return i;
Err:
	lumpDecRef(u, 1);
	vfRUnlock(ds->file);
	n = i;
	for(i=0; i<n ; i++)
		vdCleanup(&dir[i]);
	return -1;
}

void
vdeFree(VacDirEnum *ds)
{
	if(ds == nil)
		return;
	vfDecRef(ds->file);
	vtMemFree(ds);
}

static ulong
msAlloc(Source *ms, ulong start, int n)
{
	ulong nb, i;
	Lump *u;
	MetaBlock mb;

	nb = sourceGetNumBlocks(ms);
	u = nil;
	if(start > nb)
		start = nb;
	for(i=start; i<nb; i++) {
		u = sourceGetLump(ms, i, 1, 1);
		if(u == nil)
			goto Err;
		if(!mbUnpack(&mb, u->data, ms->dsize))
			goto Err;
		if(mb.maxsize - mb.size + mb.free >= n && mb.nindex < mb.maxindex)
			break;
		lumpDecRef(u, 1);
		u = nil;
	}
	/* add block to meta file */
	if(i == nb) {
		if(!sourceSetDepth(ms, (i+1)*ms->dsize))
			goto Err;
		u = sourceGetLump(ms, i, 0, 1);
		if(u == nil)
			goto Err;
		sourceSetSize(ms, (nb+1)*ms->dsize);
		mbInit(&mb, u->data, u->asize);
		mbPack(&mb);
	}
	lumpDecRef(u, 1);
	return i;
Err:
	lumpDecRef(u, 1);
	return NilBlock;
}

