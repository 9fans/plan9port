#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

static int	sizeToDepth(uvlong s, int psize, int dsize);

static int
sizeToDepth(uvlong s, int psize, int dsize)
{
	int np;
	int d;
	
	/* determine pointer depth */
	np = psize/VtScoreSize;
	s = (s + dsize - 1)/dsize;
	for(d = 0; s > 1; d++)
		s = (s + np - 1)/np;
	return d;
}

/* assumes u is lock? */
Source *
sourceAlloc(Cache *c, Lump *u, ulong block, int entry, int readOnly)
{
	Source *r;
	VtEntry d;

	if(u->asize < (entry+1)*VtEntrySize) {
		vtSetError(ENoDir);
		return nil;
	}

	if(!vtEntryUnpack(&d, u->data, entry))
		return nil;
	
	if(!(d.flags & VtEntryActive)) {
fprint(2, "bad flags %#ux %V\n", d.flags, d.score);
		vtSetError(ENoDir);
		return nil;
	}
	
	/* HACK for backwards compatiblity - should go away at some point */
	if(d.depth == 0) {
if(d.size > d.dsize) fprint(2, "depth == 0! size = %ulld\n", d.size);
		d.depth = sizeToDepth(d.size, d.psize, d.dsize);
	}

	if(d.depth < sizeToDepth(d.size, d.psize, d.dsize)) {
		vtSetError(EBadDir);
		return nil;
	}

	r = vtMemAllocZ(sizeof(Source));
	r->lk = vtLockAlloc();
	r->cache = c;
	r->readOnly = readOnly;
	r->lump = lumpIncRef(u);
	r->block = block;
	r->entry = entry;
	r->gen = d.gen;
	r->dir = (d.flags & VtEntryDir) != 0;
	r->depth = d.depth;
	r->psize = d.psize;
	r->dsize = d.dsize;
	r->size = d.size;

	r->epb = r->dsize/VtEntrySize;

	return r;
}

Source *
sourceOpen(Source *r, ulong entry, int readOnly)
{
	ulong bn;
	Lump *u;

if(0)fprint(2, "sourceOpen: %V:%d: %lud\n", r->lump->score, r->entry, entry);
	if(r->readOnly && !readOnly) {
		vtSetError(EReadOnly);
		return nil;
	}

	bn = entry/r->epb;

	u = sourceGetLump(r, bn, readOnly, 1);
	if(u == nil)
		return nil;

	r = sourceAlloc(r->cache, u, bn, entry%r->epb, readOnly);
	lumpDecRef(u, 1);
	return r;
}

Source *
sourceCreate(Source *r, int psize, int dsize, int isdir, ulong entry)
{
	Source *rr;
	int i;
	Lump *u;
	ulong bn;
	VtEntry dir;

	if(r->readOnly) {
		vtSetError(EReadOnly);
		return nil;
	}

	if(entry == 0) {
		/*
		 * look at a random block to see if we can find an empty entry
		 */
		entry = sourceGetDirSize(r);
		entry = r->epb*lnrand(entry/r->epb+1);
	}

	/*
	 * need to loop since multiple threads could be trying to allocate
	 */
	for(;;) {
		bn = entry/r->epb;
		sourceSetDepth(r, (uvlong)(bn+1)*r->dsize);
		u = sourceGetLump(r, bn, 0, 1);
		if(u == nil)
			return nil;
		for(i=entry%r->epb; i<r->epb; i++) {
			vtEntryUnpack(&dir, u->data, i);
			if((dir.flags&VtEntryActive) == 0 && dir.gen != ~0)
				goto Found;
		}
		lumpDecRef(u, 1);
		entry = sourceGetDirSize(r);
	}
Found:
	/* found an entry */
	dir.psize = psize;
	dir.dsize = dsize;
	dir.flags = VtEntryActive;
	if(isdir)
		dir.flags |= VtEntryDir;
	dir.depth = 0;
	dir.size = 0;
	memmove(dir.score, vtZeroScore, VtScoreSize);
	vtEntryPack(&dir, u->data, i);

	sourceSetDirSize(r, bn*r->epb + i + 1);
	rr = sourceAlloc(r->cache, u, bn, i, 0);
	
	lumpDecRef(u, 1);
	return rr;
}

void
sourceRemove(Source *r)
{
	lumpFreeEntry(r->lump, r->entry);
	sourceFree(r);
}

int
sourceSetDepth(Source *r, uvlong size)
{
	Lump *u, *v;
	VtEntry dir;
	int depth;

	if(r->readOnly){
		vtSetError(EReadOnly);
		return 0;
	}

	depth = sizeToDepth(size, r->psize, r->dsize);

	assert(depth >= 0);

	if(depth > VtPointerDepth) {
		vtSetError(ETooBig);
		return 0;
	}

	vtLock(r->lk);

	if(r->depth >= depth) {
		vtUnlock(r->lk);
		return 1;
	}
	
	u = r->lump;
	vtLock(u->lk);
	if(!vtEntryUnpack(&dir, u->data, r->entry)) {
		vtUnlock(u->lk);
		vtUnlock(r->lk);
		return 0;
	}
	while(dir.depth < depth) {
		v = cacheAllocLump(r->cache, VtPointerType0+r->depth, r->psize, r->dir);
		if(v == nil)
			break;
		memmove(v->data, dir.score, VtScoreSize);
		memmove(dir.score, v->score, VtScoreSize);
		dir.depth++;
		vtUnlock(v->lk);
	}
	vtEntryPack(&dir, u->data, r->entry);
	vtUnlock(u->lk);

	r->depth = dir.depth;
	vtUnlock(r->lk);

	return dir.depth == depth;
}

int
sourceGetVtEntry(Source *r, VtEntry *dir)
{
	Lump *u;

	u = r->lump;
	vtLock(u->lk);
	if(!vtEntryUnpack(dir, u->data, r->entry)) {
		vtUnlock(u->lk);
		return 0;
	}
	vtUnlock(u->lk);
	return 1;
}

uvlong
sourceGetSize(Source *r)
{
	uvlong size;

	vtLock(r->lk);
	size = r->size;
	vtUnlock(r->lk);

	return size;
}


int
sourceSetSize(Source *r, uvlong size)
{
	Lump *u;
	VtEntry dir;
	int depth;

	if(r->readOnly) {
		vtSetError(EReadOnly);
		return 0;
	}

	if(size > VtMaxFileSize || size > ((uvlong)MaxBlock)*r->dsize) {
		vtSetError(ETooBig);
		return 0;
	}

	vtLock(r->lk);
	depth = sizeToDepth(size, r->psize, r->dsize);
	if(size < r->size) {
		vtUnlock(r->lk);
		return 1;
	}
	if(depth > r->depth) {
		vtSetError(EBadDir);
		vtUnlock(r->lk);
		return 0;
	}
	
	u = r->lump;
	vtLock(u->lk);
	vtEntryUnpack(&dir, u->data, r->entry);
	dir.size = size;
	vtEntryPack(&dir, u->data, r->entry);
	vtUnlock(u->lk);
	r->size = size;
	vtUnlock(r->lk);
	return 1;
}

int
sourceSetDirSize(Source *r, ulong ds)
{
	uvlong size;

	size = (uvlong)r->dsize*(ds/r->epb);
	size += VtEntrySize*(ds%r->epb);
	return sourceSetSize(r, size);
}

ulong
sourceGetDirSize(Source *r)
{
	ulong ds;
	uvlong size;

	size = sourceGetSize(r);
	ds = r->epb*(size/r->dsize);
	ds += (size%r->dsize)/VtEntrySize;
	return ds;
}

ulong
sourceGetNumBlocks(Source *r)
{
	return (sourceGetSize(r)+r->dsize-1)/r->dsize;
}

Lump *
sourceWalk(Source *r, ulong block, int readOnly, int *off)
{
	int depth;
	int i, np;
	Lump *u, *v;
	int elem[VtPointerDepth+1];
	ulong b;

	if(r->readOnly && !readOnly) {
		vtSetError(EReadOnly);
		return nil;
	}

	vtLock(r->lk);
	np = r->psize/VtScoreSize;
	b = block;
	for(i=0; i<r->depth; i++) {
		elem[i] = b % np;
		b /= np;
	}
	if(b != 0) {
		vtUnlock(r->lk);
		vtSetError(EBadOffset);
		return nil;
	}
	elem[i] = r->entry;
	u = lumpIncRef(r->lump);
	depth = r->depth;
	*off = elem[0];
	vtUnlock(r->lk);

	for(i=depth; i>0; i--) {
		v = lumpWalk(u, elem[i], VtPointerType0+i-1, r->psize, readOnly, 0);
		lumpDecRef(u, 0);
		if(v == nil)
			return nil;
		u = v;
	}

	return u;
}

Lump *
sourceGetLump(Source *r, ulong block, int readOnly, int lock)
{
	int type, off;
	Lump *u, *v;

	if(r->readOnly && !readOnly) {
		vtSetError(EReadOnly);
		return nil;
	}
	if(block == NilBlock) {
		vtSetError(ENilBlock);
		return nil;
	}
if(0)fprint(2, "sourceGetLump: %V:%d %lud\n", r->lump->score, r->entry, block);
	u = sourceWalk(r, block, readOnly, &off);
	if(u == nil)
		return nil;
	if(r->dir)
		type = VtDirType;
	else
		type = VtDataType;
	v = lumpWalk(u, off, type, r->dsize, readOnly, lock);
	lumpDecRef(u, 0);
	return v;
}

void
sourceFree(Source *k)
{
	if(k == nil)
		return;
	lumpDecRef(k->lump, 0);
	vtLockFree(k->lk);
	memset(k, ~0, sizeof(*k));
	vtMemFree(k);
}
