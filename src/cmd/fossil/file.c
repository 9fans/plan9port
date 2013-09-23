#include "stdinc.h"
#include "9.h"			/* for consPrint */
#include "dat.h"
#include "fns.h"
#include "error.h"

/*
 * locking order is upwards.  A thread can hold the lock for a File
 * and then acquire the lock of its parent
 */

struct File {
	Fs	*fs;		/* immutable */

	/* meta data for file: protected by the lk in the parent */
	int	ref;		/* holds this data structure up */

	int	partial;	/* file was never really open */
	int	removed;	/* file has been removed */
	int	dirty;	/* dir is dirty with respect to meta data in block */
	u32int	boff;	/* block offset within msource for this file's meta data */

	DirEntry dir;	/* meta data for this file, including component name */

	File	*up;		/* parent file (directory) */
	File	*next;		/* sibling */

	/* data for file */
	VtLock	*lk;		/* lock for the following */
	Source	*source;
	Source	*msource;	/* for directories: meta data for children */
	File	*down;		/* children */

	int	mode;
	int	issnapshot;
};

static int fileMetaFlush2(File*, char*);
static u32int fileMetaAlloc(File*, DirEntry*, u32int);
static int fileRLock(File*);
static void fileRUnlock(File*);
static int fileLock(File*);
static void fileUnlock(File*);
static void fileMetaLock(File*);
static void fileMetaUnlock(File*);
static void fileRAccess(File*);
static void fileWAccess(File*, char*);

static File *
fileAlloc(Fs *fs)
{
	File *f;

	f = vtMemAllocZ(sizeof(File));
	f->lk = vtLockAlloc();
	f->ref = 1;
	f->fs = fs;
	f->boff = NilBlock;
	f->mode = fs->mode;
	return f;
}

static void
fileFree(File *f)
{
	sourceClose(f->source);
	vtLockFree(f->lk);
	sourceClose(f->msource);
	deCleanup(&f->dir);

	memset(f, ~0, sizeof(File));
	vtMemFree(f);
}

/*
 * the file is locked already
 * f->msource is unlocked
 */
static File *
dirLookup(File *f, char *elem)
{
	int i;
	MetaBlock mb;
	MetaEntry me;
	Block *b;
	Source *meta;
	File *ff;
	u32int bo, nb;

	meta = f->msource;
	b = nil;
	if(!sourceLock(meta, -1))
		return nil;
	nb = (sourceGetSize(meta)+meta->dsize-1)/meta->dsize;
	for(bo=0; bo<nb; bo++){
		b = sourceBlock(meta, bo, OReadOnly);
		if(b == nil)
			goto Err;
		if(!mbUnpack(&mb, b->data, meta->dsize))
			goto Err;
		if(mbSearch(&mb, elem, &i, &me)){
			ff = fileAlloc(f->fs);
			if(!deUnpack(&ff->dir, &me)){
				fileFree(ff);
				goto Err;
			}
			sourceUnlock(meta);
			blockPut(b);
			ff->boff = bo;
			ff->mode = f->mode;
			ff->issnapshot = f->issnapshot;
			return ff;
		}

		blockPut(b);
		b = nil;
	}
	vtSetError(ENoFile);
	/* fall through */
Err:
	sourceUnlock(meta);
	blockPut(b);
	return nil;
}

File *
fileRoot(Source *r)
{
	Block *b;
	Source *r0, *r1, *r2;
	MetaBlock mb;
	MetaEntry me;
	File *root, *mr;
	Fs *fs;

	b = nil;
	root = nil;
	mr = nil;
	r1 = nil;
	r2 = nil;

	fs = r->fs;
	if(!sourceLock(r, -1))
		return nil;
	r0 = sourceOpen(r, 0, fs->mode, 0);
	if(r0 == nil)
		goto Err;
	r1 = sourceOpen(r, 1, fs->mode, 0);
	if(r1 == nil)
		goto Err;
	r2 = sourceOpen(r, 2, fs->mode, 0);
	if(r2 == nil)
		goto Err;

	mr = fileAlloc(fs);
	mr->msource = r2;
	r2 = nil;

	root = fileAlloc(fs);
	root->boff = 0;
	root->up = mr;
	root->source = r0;
	r0->file = root;			/* point back to source */
	r0 = nil;
	root->msource = r1;
	r1 = nil;

	mr->down = root;

	if(!sourceLock(mr->msource, -1))
		goto Err;
	b = sourceBlock(mr->msource, 0, OReadOnly);
	sourceUnlock(mr->msource);
	if(b == nil)
		goto Err;

	if(!mbUnpack(&mb, b->data, mr->msource->dsize))
		goto Err;

	meUnpack(&me, &mb, 0);
	if(!deUnpack(&root->dir, &me))
		goto Err;
	blockPut(b);
	sourceUnlock(r);
	fileRAccess(root);

	return root;
Err:
	blockPut(b);
	if(r0)
		sourceClose(r0);
	if(r1)
		sourceClose(r1);
	if(r2)
		sourceClose(r2);
	if(mr)
		fileFree(mr);
	if(root)
		fileFree(root);
	sourceUnlock(r);

	return nil;
}

static Source *
fileOpenSource(File *f, u32int offset, u32int gen, int dir, uint mode,
	int issnapshot)
{
	char *rname, *fname;
	Source *r;

	if(!sourceLock(f->source, mode))
		return nil;
	r = sourceOpen(f->source, offset, mode, issnapshot);
	sourceUnlock(f->source);
	if(r == nil)
		return nil;
	if(r->gen != gen){
		vtSetError(ERemoved);
		goto Err;
	}
	if(r->dir != dir && r->mode != -1){
		/* this hasn't been as useful as we hoped it would be. */
		rname = sourceName(r);
		fname = fileName(f);
		consPrint("%s: source %s for file %s: fileOpenSource: "
			"dir mismatch %d %d\n",
			f->source->fs->name, rname, fname, r->dir, dir);
		free(rname);
		free(fname);

		vtSetError(EBadMeta);
		goto Err;
	}
	return r;
Err:
	sourceClose(r);
	return nil;
}

File *
_fileWalk(File *f, char *elem, int partial)
{
	File *ff;

	fileRAccess(f);

	if(elem[0] == 0){
		vtSetError(EBadPath);
		return nil;
	}

	if(!fileIsDir(f)){
		vtSetError(ENotDir);
		return nil;
	}

	if(strcmp(elem, ".") == 0){
		return fileIncRef(f);
	}

	if(strcmp(elem, "..") == 0){
		if(fileIsRoot(f))
			return fileIncRef(f);
		return fileIncRef(f->up);
	}

	if(!fileLock(f))
		return nil;

	for(ff = f->down; ff; ff=ff->next){
		if(strcmp(elem, ff->dir.elem) == 0 && !ff->removed){
			ff->ref++;
			goto Exit;
		}
	}

	ff = dirLookup(f, elem);
	if(ff == nil)
		goto Err;

	if(ff->dir.mode & ModeSnapshot){
		ff->mode = OReadOnly;
		ff->issnapshot = 1;
	}

	if(partial){
		/*
		 * Do nothing.  We're opening this file only so we can clri it.
		 * Usually the sources can't be opened, hence we won't even bother.
		 * Be VERY careful with the returned file.  If you hand it to a routine
		 * expecting ff->source and/or ff->msource to be non-nil, we're
		 * likely to dereference nil.  FileClri should be the only routine
		 * setting partial.
		 */
		ff->partial = 1;
	}else if(ff->dir.mode & ModeDir){
		ff->source = fileOpenSource(f, ff->dir.entry, ff->dir.gen,
			1, ff->mode, ff->issnapshot);
		ff->msource = fileOpenSource(f, ff->dir.mentry, ff->dir.mgen,
			0, ff->mode, ff->issnapshot);
		if(ff->source == nil || ff->msource == nil)
			goto Err;
	}else{
		ff->source = fileOpenSource(f, ff->dir.entry, ff->dir.gen,
			0, ff->mode, ff->issnapshot);
		if(ff->source == nil)
			goto Err;
	}

	/* link in and up parent ref count */
	if (ff->source)
		ff->source->file = ff;		/* point back */
	ff->next = f->down;
	f->down = ff;
	ff->up = f;
	fileIncRef(f);
Exit:
	fileUnlock(f);
	return ff;
Err:
	fileUnlock(f);
	if(ff != nil)
		fileDecRef(ff);
	return nil;
}

File *
fileWalk(File *f, char *elem)
{
	return _fileWalk(f, elem, 0);
}

File *
_fileOpen(Fs *fs, char *path, int partial)
{
	File *f, *ff;
	char *p, elem[VtMaxStringSize], *opath;
	int n;

	f = fs->file;
	fileIncRef(f);
	opath = path;
	while(*path != 0){
		for(p = path; *p && *p != '/'; p++)
			;
		n = p - path;
		if(n > 0){
			if(n > VtMaxStringSize){
				vtSetError("%s: element too long", EBadPath);
				goto Err;
			}
			memmove(elem, path, n);
			elem[n] = 0;
			ff = _fileWalk(f, elem, partial && *p=='\0');
			if(ff == nil){
				vtSetError("%.*s: %R", utfnlen(opath, p-opath),
					opath);
				goto Err;
			}
			fileDecRef(f);
			f = ff;
		}
		if(*p == '/')
			p++;
		path = p;
	}
	return f;
Err:
	fileDecRef(f);
	return nil;
}

File*
fileOpen(Fs *fs, char *path)
{
	return _fileOpen(fs, path, 0);
}

static void
fileSetTmp(File *f, int istmp)
{
	int i;
	Entry e;
	Source *r;

	for(i=0; i<2; i++){
		if(i==0)
			r = f->source;
		else
			r = f->msource;
		if(r == nil)
			continue;
		if(!sourceGetEntry(r, &e)){
			fprint(2, "sourceGetEntry failed (cannot happen): %r\n");
			continue;
		}
		if(istmp)
			e.flags |= VtEntryNoArchive;
		else
			e.flags &= ~VtEntryNoArchive;
		if(!sourceSetEntry(r, &e)){
			fprint(2, "sourceSetEntry failed (cannot happen): %r\n");
			continue;
		}
	}
}

File *
fileCreate(File *f, char *elem, ulong mode, char *uid)
{
	File *ff;
	DirEntry *dir;
	Source *pr, *r, *mr;
	int isdir;

	if(!fileLock(f))
		return nil;

	r = nil;
	mr = nil;
	for(ff = f->down; ff; ff=ff->next){
		if(strcmp(elem, ff->dir.elem) == 0 && !ff->removed){
			ff = nil;
			vtSetError(EExists);
			goto Err1;
		}
	}

	ff = dirLookup(f, elem);
	if(ff != nil){
		vtSetError(EExists);
		goto Err1;
	}

	pr = f->source;
	if(pr->mode != OReadWrite){
		vtSetError(EReadOnly);
		goto Err1;
	}

	if(!sourceLock2(f->source, f->msource, -1))
		goto Err1;

	ff = fileAlloc(f->fs);
	isdir = mode & ModeDir;

	r = sourceCreate(pr, pr->dsize, isdir, 0);
	if(r == nil)
		goto Err;
	if(isdir){
		mr = sourceCreate(pr, pr->dsize, 0, r->offset);
		if(mr == nil)
			goto Err;
	}

	dir = &ff->dir;
	dir->elem = vtStrDup(elem);
	dir->entry = r->offset;
	dir->gen = r->gen;
	if(isdir){
		dir->mentry = mr->offset;
		dir->mgen = mr->gen;
	}
	dir->size = 0;
	if(!fsNextQid(f->fs, &dir->qid))
		goto Err;
	dir->uid = vtStrDup(uid);
	dir->gid = vtStrDup(f->dir.gid);
	dir->mid = vtStrDup(uid);
	dir->mtime = time(0L);
	dir->mcount = 0;
	dir->ctime = dir->mtime;
	dir->atime = dir->mtime;
	dir->mode = mode;

	ff->boff = fileMetaAlloc(f, dir, 0);
	if(ff->boff == NilBlock)
		goto Err;

	sourceUnlock(f->source);
	sourceUnlock(f->msource);

	ff->source = r;
	r->file = ff;			/* point back */
	ff->msource = mr;

	if(mode&ModeTemporary){
		if(!sourceLock2(r, mr, -1))
			goto Err1;
		fileSetTmp(ff, 1);
		sourceUnlock(r);
		if(mr)
			sourceUnlock(mr);
	}

	/* committed */

	/* link in and up parent ref count */
	ff->next = f->down;
	f->down = ff;
	ff->up = f;
	fileIncRef(f);

	fileWAccess(f, uid);

	fileUnlock(f);
	return ff;

Err:
	sourceUnlock(f->source);
	sourceUnlock(f->msource);
Err1:
	if(r){
		sourceLock(r, -1);
		sourceRemove(r);
	}
	if(mr){
		sourceLock(mr, -1);
		sourceRemove(mr);
	}
	if(ff)
		fileDecRef(ff);
	fileUnlock(f);
	return 0;
}

int
fileRead(File *f, void *buf, int cnt, vlong offset)
{
	Source *s;
	uvlong size;
	u32int bn;
	int off, dsize, n, nn;
	Block *b;
	uchar *p;

if(0)fprint(2, "fileRead: %s %d, %lld\n", f->dir.elem, cnt, offset);

	if(!fileRLock(f))
		return -1;

	if(offset < 0){
		vtSetError(EBadOffset);
		goto Err1;
	}

	fileRAccess(f);

	if(!sourceLock(f->source, OReadOnly))
		goto Err1;

	s = f->source;
	dsize = s->dsize;
	size = sourceGetSize(s);

	if(offset >= size)
		offset = size;

	if(cnt > size-offset)
		cnt = size-offset;
	bn = offset/dsize;
	off = offset%dsize;
	p = buf;
	while(cnt > 0){
		b = sourceBlock(s, bn, OReadOnly);
		if(b == nil)
			goto Err;
		n = cnt;
		if(n > dsize-off)
			n = dsize-off;
		nn = dsize-off;
		if(nn > n)
			nn = n;
		memmove(p, b->data+off, nn);
		memset(p+nn, 0, nn-n);
		off = 0;
		bn++;
		cnt -= n;
		p += n;
		blockPut(b);
	}
	sourceUnlock(s);
	fileRUnlock(f);
	return p-(uchar*)buf;

Err:
	sourceUnlock(s);
Err1:
	fileRUnlock(f);
	return -1;
}

/*
 * Changes the file block bn to be the given block score.
 * Very sneaky.  Only used by flfmt.
 */
int
fileMapBlock(File *f, ulong bn, uchar score[VtScoreSize], ulong tag)
{
	Block *b;
	Entry e;
	Source *s;

	if(!fileLock(f))
		return 0;

	s = nil;
	if(f->dir.mode & ModeDir){
		vtSetError(ENotFile);
		goto Err;
	}

	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		goto Err;
	}

	if(!sourceLock(f->source, -1))
		goto Err;

	s = f->source;
	b = _sourceBlock(s, bn, OReadWrite, 1, tag);
	if(b == nil)
		goto Err;

	if(!sourceGetEntry(s, &e))
		goto Err;
	if(b->l.type == BtDir){
		memmove(e.score, score, VtScoreSize);
		assert(e.tag == tag || e.tag == 0);
		e.tag = tag;
		e.flags |= VtEntryLocal;
		entryPack(&e, b->data, f->source->offset % f->source->epb);
	}else
		memmove(b->data + (bn%(e.psize/VtScoreSize))*VtScoreSize, score, VtScoreSize);
	blockDirty(b);
	blockPut(b);
	sourceUnlock(s);
	fileUnlock(f);
	return 1;

Err:
	if(s)
		sourceUnlock(s);
	fileUnlock(f);
	return 0;
}

int
fileSetSize(File *f, uvlong size)
{
	int r;

	if(!fileLock(f))
		return 0;
	r = 0;
	if(f->dir.mode & ModeDir){
		vtSetError(ENotFile);
		goto Err;
	}
	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		goto Err;
	}
	if(!sourceLock(f->source, -1))
		goto Err;
	r = sourceSetSize(f->source, size);
	sourceUnlock(f->source);
Err:
	fileUnlock(f);
	return r;
}

int
fileWrite(File *f, void *buf, int cnt, vlong offset, char *uid)
{
	Source *s;
	ulong bn;
	int off, dsize, n;
	Block *b;
	uchar *p;
	vlong eof;

if(0)fprint(2, "fileWrite: %s %d, %lld\n", f->dir.elem, cnt, offset);

	if(!fileLock(f))
		return -1;

	s = nil;
	if(f->dir.mode & ModeDir){
		vtSetError(ENotFile);
		goto Err;
	}

	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		goto Err;
	}
	if(offset < 0){
		vtSetError(EBadOffset);
		goto Err;
	}

	fileWAccess(f, uid);

	if(!sourceLock(f->source, -1))
		goto Err;
	s = f->source;
	dsize = s->dsize;

	eof = sourceGetSize(s);
	if(f->dir.mode & ModeAppend)
		offset = eof;
	bn = offset/dsize;
	off = offset%dsize;
	p = buf;
	while(cnt > 0){
		n = cnt;
		if(n > dsize-off)
			n = dsize-off;
		b = sourceBlock(s, bn, n<dsize?OReadWrite:OOverWrite);
		if(b == nil){
			if(offset > eof)
				sourceSetSize(s, offset);
			goto Err;
		}
		memmove(b->data+off, p, n);
		off = 0;
		cnt -= n;
		p += n;
		offset += n;
		bn++;
		blockDirty(b);
		blockPut(b);
	}
	if(offset > eof && !sourceSetSize(s, offset))
		goto Err;
	sourceUnlock(s);
	fileUnlock(f);
	return p-(uchar*)buf;
Err:
	if(s)
		sourceUnlock(s);
	fileUnlock(f);
	return -1;
}

int
fileGetDir(File *f, DirEntry *dir)
{
	if(!fileRLock(f))
		return 0;

	fileMetaLock(f);
	deCopy(dir, &f->dir);
	fileMetaUnlock(f);

	if(!fileIsDir(f)){
		if(!sourceLock(f->source, OReadOnly)){
			fileRUnlock(f);
			return 0;
		}
		dir->size = sourceGetSize(f->source);
		sourceUnlock(f->source);
	}
	fileRUnlock(f);

	return 1;
}

int
fileTruncate(File *f, char *uid)
{
	if(fileIsDir(f)){
		vtSetError(ENotFile);
		return 0;
	}

	if(!fileLock(f))
		return 0;

	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		fileUnlock(f);
		return 0;
	}
	if(!sourceLock(f->source, -1)){
		fileUnlock(f);
		return 0;
	}
	if(!sourceTruncate(f->source)){
		sourceUnlock(f->source);
		fileUnlock(f);
		return 0;
	}
	sourceUnlock(f->source);
	fileUnlock(f);

	fileWAccess(f, uid);

	return 1;
}

int
fileSetDir(File *f, DirEntry *dir, char *uid)
{
	File *ff;
	char *oelem;
	u32int mask;
	u64int size;

	/* can not set permissions for the root */
	if(fileIsRoot(f)){
		vtSetError(ERoot);
		return 0;
	}

	if(!fileLock(f))
		return 0;

	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		fileUnlock(f);
		return 0;
	}

	fileMetaLock(f);

	/* check new name does not already exist */
	if(strcmp(f->dir.elem, dir->elem) != 0){
		for(ff = f->up->down; ff; ff=ff->next){
			if(strcmp(dir->elem, ff->dir.elem) == 0 && !ff->removed){
				vtSetError(EExists);
				goto Err;
			}
		}

		ff = dirLookup(f->up, dir->elem);
		if(ff != nil){
			fileDecRef(ff);
			vtSetError(EExists);
			goto Err;
		}
	}

	if(!sourceLock2(f->source, f->msource, -1))
		goto Err;
	if(!fileIsDir(f)){
		size = sourceGetSize(f->source);
		if(size != dir->size){
			if(!sourceSetSize(f->source, dir->size)){
				sourceUnlock(f->source);
				if(f->msource)
					sourceUnlock(f->msource);
				goto Err;
			}
			/* commited to changing it now */
		}
	}
	/* commited to changing it now */
	if((f->dir.mode&ModeTemporary) != (dir->mode&ModeTemporary))
		fileSetTmp(f, dir->mode&ModeTemporary);
	sourceUnlock(f->source);
	if(f->msource)
		sourceUnlock(f->msource);

	oelem = nil;
	if(strcmp(f->dir.elem, dir->elem) != 0){
		oelem = f->dir.elem;
		f->dir.elem = vtStrDup(dir->elem);
	}

	if(strcmp(f->dir.uid, dir->uid) != 0){
		vtMemFree(f->dir.uid);
		f->dir.uid = vtStrDup(dir->uid);
	}

	if(strcmp(f->dir.gid, dir->gid) != 0){
		vtMemFree(f->dir.gid);
		f->dir.gid = vtStrDup(dir->gid);
	}

	f->dir.mtime = dir->mtime;
	f->dir.atime = dir->atime;

//fprint(2, "mode %x %x ", f->dir.mode, dir->mode);
	mask = ~(ModeDir|ModeSnapshot);
	f->dir.mode &= ~mask;
	f->dir.mode |= mask & dir->mode;
	f->dirty = 1;
//fprint(2, "->%x\n", f->dir.mode);

	fileMetaFlush2(f, oelem);
	vtMemFree(oelem);

	fileMetaUnlock(f);
	fileUnlock(f);

	fileWAccess(f->up, uid);

	return 1;
Err:
	fileMetaUnlock(f);
	fileUnlock(f);
	return 0;
}

int
fileSetQidSpace(File *f, u64int offset, u64int max)
{
	int ret;

	if(!fileLock(f))
		return 0;
	fileMetaLock(f);
	f->dir.qidSpace = 1;
	f->dir.qidOffset = offset;
	f->dir.qidMax = max;
	ret = fileMetaFlush2(f, nil)>=0;
	fileMetaUnlock(f);
	fileUnlock(f);
	return ret;
}


uvlong
fileGetId(File *f)
{
	/* immutable */
	return f->dir.qid;
}

ulong
fileGetMcount(File *f)
{
	ulong mcount;

	fileMetaLock(f);
	mcount = f->dir.mcount;
	fileMetaUnlock(f);
	return mcount;
}

ulong
fileGetMode(File *f)
{
	ulong mode;

	fileMetaLock(f);
	mode = f->dir.mode;
	fileMetaUnlock(f);
	return mode;
}

int
fileIsDir(File *f)
{
	/* immutable */
	return (f->dir.mode & ModeDir) != 0;
}

int
fileIsAppend(File *f)
{
	return (f->dir.mode & ModeAppend) != 0;
}

int
fileIsExclusive(File *f)
{
	return (f->dir.mode & ModeExclusive) != 0;
}

int
fileIsTemporary(File *f)
{
	return (f->dir.mode & ModeTemporary) != 0;
}

int
fileIsRoot(File *f)
{
	return f == f->fs->file;
}

int
fileIsRoFs(File *f)
{
	return f->fs->mode == OReadOnly;
}

int
fileGetSize(File *f, uvlong *size)
{
	if(!fileRLock(f))
		return 0;
	if(!sourceLock(f->source, OReadOnly)){
		fileRUnlock(f);
		return 0;
	}
	*size = sourceGetSize(f->source);
	sourceUnlock(f->source);
	fileRUnlock(f);

	return 1;
}

int
fileMetaFlush(File *f, int rec)
{
	File **kids, *p;
	int nkids;
	int i, rv;

	fileMetaLock(f);
	rv = fileMetaFlush2(f, nil);
	fileMetaUnlock(f);

	if(!rec || !fileIsDir(f))
		return rv;

	if(!fileLock(f))
		return rv;
	nkids = 0;
	for(p=f->down; p; p=p->next)
		nkids++;
	kids = vtMemAlloc(nkids*sizeof(File*));
	i = 0;
	for(p=f->down; p; p=p->next){
		kids[i++] = p;
		p->ref++;
	}
	fileUnlock(f);

	for(i=0; i<nkids; i++){
		rv |= fileMetaFlush(kids[i], 1);
		fileDecRef(kids[i]);
	}
	vtMemFree(kids);
	return rv;
}

/* assumes metaLock is held */
static int
fileMetaFlush2(File *f, char *oelem)
{
	File *fp;
	Block *b, *bb;
	MetaBlock mb;
	MetaEntry me, me2;
	int i, n;
	u32int boff;

	if(!f->dirty)
		return 0;

	if(oelem == nil)
		oelem = f->dir.elem;

//print("fileMetaFlush %s->%s\n", oelem, f->dir.elem);

	fp = f->up;

	if(!sourceLock(fp->msource, -1))
		return -1;
	/* can happen if source is clri'ed out from under us */
	if(f->boff == NilBlock)
		goto Err1;
	b = sourceBlock(fp->msource, f->boff, OReadWrite);
	if(b == nil)
		goto Err1;

	if(!mbUnpack(&mb, b->data, fp->msource->dsize))
		goto Err;
	if(!mbSearch(&mb, oelem, &i, &me))
		goto Err;

	n = deSize(&f->dir);
if(0)fprint(2, "old size %d new size %d\n", me.size, n);

	if(mbResize(&mb, &me, n)){
		/* fits in the block */
		mbDelete(&mb, i);
		if(strcmp(f->dir.elem, oelem) != 0)
			mbSearch(&mb, f->dir.elem, &i, &me2);
		dePack(&f->dir, &me);
		mbInsert(&mb, i, &me);
		mbPack(&mb);
		blockDirty(b);
		blockPut(b);
		sourceUnlock(fp->msource);
		f->dirty = 0;

		return 1;
	}

	/*
	 * moving entry to another block
	 * it is feasible for the fs to crash leaving two copies
	 * of the directory entry.  This is just too much work to
	 * fix.  Given that entries are only allocated in a block that
	 * is less than PercentageFull, most modifications of meta data
	 * will fit within the block.  i.e. this code should almost
	 * never be executed.
	 */
	boff = fileMetaAlloc(fp, &f->dir, f->boff+1);
	if(boff == NilBlock){
		/* mbResize might have modified block */
		mbPack(&mb);
		blockDirty(b);
		goto Err;
	}
fprint(2, "fileMetaFlush moving entry from %ud -> %ud\n", f->boff, boff);
	f->boff = boff;

	/* make sure deletion goes to disk after new entry */
	bb = sourceBlock(fp->msource, f->boff, OReadWrite);
	mbDelete(&mb, i);
	mbPack(&mb);
	blockDependency(b, bb, -1, nil, nil);
	blockPut(bb);
	blockDirty(b);
	blockPut(b);
	sourceUnlock(fp->msource);

	f->dirty = 0;

	return 1;

Err:
	blockPut(b);
Err1:
	sourceUnlock(fp->msource);
	return -1;
}

static int
fileMetaRemove(File *f, char *uid)
{
	Block *b;
	MetaBlock mb;
	MetaEntry me;
	int i;
	File *up;

	up = f->up;

	fileWAccess(up, uid);

	fileMetaLock(f);

	sourceLock(up->msource, OReadWrite);
	b = sourceBlock(up->msource, f->boff, OReadWrite);
	if(b == nil)
		goto Err;

	if(!mbUnpack(&mb, b->data, up->msource->dsize))
{
fprint(2, "U\n");
		goto Err;
}
	if(!mbSearch(&mb, f->dir.elem, &i, &me))
{
fprint(2, "S\n");
		goto Err;
}
	mbDelete(&mb, i);
	mbPack(&mb);
	sourceUnlock(up->msource);

	blockDirty(b);
	blockPut(b);

	f->removed = 1;
	f->boff = NilBlock;
	f->dirty = 0;

	fileMetaUnlock(f);
	return 1;

Err:
	sourceUnlock(up->msource);
	blockPut(b);
	fileMetaUnlock(f);
	return 0;
}

/* assume file is locked, assume f->msource is locked */
static int
fileCheckEmpty(File *f)
{
	u32int i, n;
	Block *b;
	MetaBlock mb;
	Source *r;

	r = f->msource;
	n = (sourceGetSize(r)+r->dsize-1)/r->dsize;
	for(i=0; i<n; i++){
		b = sourceBlock(r, i, OReadOnly);
		if(b == nil)
			goto Err;
		if(!mbUnpack(&mb, b->data, r->dsize))
			goto Err;
		if(mb.nindex > 0){
			vtSetError(ENotEmpty);
			goto Err;
		}
		blockPut(b);
	}
	return 1;
Err:
	blockPut(b);
	return 0;
}

int
fileRemove(File *f, char *uid)
{
	File *ff;

	/* can not remove the root */
	if(fileIsRoot(f)){
		vtSetError(ERoot);
		return 0;
	}

	if(!fileLock(f))
		return 0;

	if(f->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		goto Err1;
	}
	if(!sourceLock2(f->source, f->msource, -1))
		goto Err1;
	if(fileIsDir(f) && !fileCheckEmpty(f))
		goto Err;

	for(ff=f->down; ff; ff=ff->next)
		assert(ff->removed);

	sourceRemove(f->source);
	f->source->file = nil;		/* erase back pointer */
	f->source = nil;
	if(f->msource){
		sourceRemove(f->msource);
		f->msource = nil;
	}

	fileUnlock(f);

	if(!fileMetaRemove(f, uid))
		return 0;

	return 1;

Err:
	sourceUnlock(f->source);
	if(f->msource)
		sourceUnlock(f->msource);
Err1:
	fileUnlock(f);
	return 0;
}

static int
clri(File *f, char *uid)
{
	int r;

	if(f == nil)
		return 0;
	if(f->up->source->mode != OReadWrite){
		vtSetError(EReadOnly);
		fileDecRef(f);
		return 0;
	}
	r = fileMetaRemove(f, uid);
	fileDecRef(f);
	return r;
}

int
fileClriPath(Fs *fs, char *path, char *uid)
{
	return clri(_fileOpen(fs, path, 1), uid);
}

int
fileClri(File *dir, char *elem, char *uid)
{
	return clri(_fileWalk(dir, elem, 1), uid);
}

File *
fileIncRef(File *vf)
{
	fileMetaLock(vf);
	assert(vf->ref > 0);
	vf->ref++;
	fileMetaUnlock(vf);
	return vf;
}

int
fileDecRef(File *f)
{
	File *p, *q, **qq;

	if(f->up == nil){
		/* never linked in */
		assert(f->ref == 1);
		fileFree(f);
		return 1;
	}

	fileMetaLock(f);
	f->ref--;
	if(f->ref > 0){
		fileMetaUnlock(f);
		return 0;
	}
	assert(f->ref == 0);
	assert(f->down == nil);

	fileMetaFlush2(f, nil);

	p = f->up;
	qq = &p->down;
	for(q = *qq; q; q = *qq){
		if(q == f)
			break;
		qq = &q->next;
	}
	assert(q != nil);
	*qq = f->next;

	fileMetaUnlock(f);
	fileFree(f);

	fileDecRef(p);
	return 1;
}

File *
fileGetParent(File *f)
{
	if(fileIsRoot(f))
		return fileIncRef(f);
	return fileIncRef(f->up);
}

DirEntryEnum *
deeOpen(File *f)
{
	DirEntryEnum *dee;
	File *p;

	if(!fileIsDir(f)){
		vtSetError(ENotDir);
		fileDecRef(f);
		return nil;
	}

	/* flush out meta data */
	if(!fileLock(f))
		return nil;
	for(p=f->down; p; p=p->next)
		fileMetaFlush2(p, nil);
	fileUnlock(f);

	dee = vtMemAllocZ(sizeof(DirEntryEnum));
	dee->file = fileIncRef(f);

	return dee;
}

static int
dirEntrySize(Source *s, ulong elem, ulong gen, uvlong *size)
{
	Block *b;
	ulong bn;
	Entry e;
	int epb;

	epb = s->dsize/VtEntrySize;
	bn = elem/epb;
	elem -= bn*epb;

	b = sourceBlock(s, bn, OReadOnly);
	if(b == nil)
		goto Err;
	if(!entryUnpack(&e, b->data, elem))
		goto Err;

	/* hanging entries are returned as zero size */
	if(!(e.flags & VtEntryActive) || e.gen != gen)
		*size = 0;
	else
		*size = e.size;
	blockPut(b);
	return 1;

Err:
	blockPut(b);
	return 0;
}

static int
deeFill(DirEntryEnum *dee)
{
	int i, n;
	Source *meta, *source;
	MetaBlock mb;
	MetaEntry me;
	File *f;
	Block *b;
	DirEntry *de;

	/* clean up first */
	for(i=dee->i; i<dee->n; i++)
		deCleanup(dee->buf+i);
	vtMemFree(dee->buf);
	dee->buf = nil;
	dee->i = 0;
	dee->n = 0;

	f = dee->file;

	source = f->source;
	meta = f->msource;

	b = sourceBlock(meta, dee->boff, OReadOnly);
	if(b == nil)
		goto Err;
	if(!mbUnpack(&mb, b->data, meta->dsize))
		goto Err;

	n = mb.nindex;
	dee->buf = vtMemAlloc(n * sizeof(DirEntry));

	for(i=0; i<n; i++){
		de = dee->buf + i;
		meUnpack(&me, &mb, i);
		if(!deUnpack(de, &me))
			goto Err;
		dee->n++;
		if(!(de->mode & ModeDir))
		if(!dirEntrySize(source, de->entry, de->gen, &de->size))
			goto Err;
	}
	dee->boff++;
	blockPut(b);
	return 1;
Err:
	blockPut(b);
	return 0;
}

int
deeRead(DirEntryEnum *dee, DirEntry *de)
{
	int ret, didread;
	File *f;
	u32int nb;

	if(dee == nil){
		vtSetError("cannot happen in deeRead");
		return -1;
	}

	f = dee->file;
	if(!fileRLock(f))
		return -1;

	if(!sourceLock2(f->source, f->msource, OReadOnly)){
		fileRUnlock(f);
		return -1;
	}

	nb = (sourceGetSize(f->msource)+f->msource->dsize-1)/f->msource->dsize;

	didread = 0;
	while(dee->i >= dee->n){
		if(dee->boff >= nb){
			ret = 0;
			goto Return;
		}
		didread = 1;
		if(!deeFill(dee)){
			ret = -1;
			goto Return;
		}
	}

	memmove(de, dee->buf + dee->i, sizeof(DirEntry));
	dee->i++;
	ret = 1;

Return:
	sourceUnlock(f->source);
	sourceUnlock(f->msource);
	fileRUnlock(f);

	if(didread)
		fileRAccess(f);
	return ret;
}

void
deeClose(DirEntryEnum *dee)
{
	int i;
	if(dee == nil)
		return;
	for(i=dee->i; i<dee->n; i++)
		deCleanup(dee->buf+i);
	vtMemFree(dee->buf);
	fileDecRef(dee->file);
	vtMemFree(dee);
}

/*
 * caller must lock f->source and f->msource
 * caller must NOT lock the source and msource
 * referenced by dir.
 */
static u32int
fileMetaAlloc(File *f, DirEntry *dir, u32int start)
{
	u32int nb, bo;
	Block *b, *bb;
	MetaBlock mb;
	int nn;
	uchar *p;
	int i, n, epb;
	MetaEntry me;
	Source *s, *ms;

	s = f->source;
	ms = f->msource;

	n = deSize(dir);
	nb = (sourceGetSize(ms)+ms->dsize-1)/ms->dsize;
	b = nil;
	if(start > nb)
		start = nb;
	for(bo=start; bo<nb; bo++){
		b = sourceBlock(ms, bo, OReadWrite);
		if(b == nil)
			goto Err;
		if(!mbUnpack(&mb, b->data, ms->dsize))
			goto Err;
		nn = (mb.maxsize*FullPercentage/100) - mb.size + mb.free;
		if(n <= nn && mb.nindex < mb.maxindex)
			break;
		blockPut(b);
		b = nil;
	}

	/* add block to meta file */
	if(b == nil){
		b = sourceBlock(ms, bo, OReadWrite);
		if(b == nil)
			goto Err;
		sourceSetSize(ms, (nb+1)*ms->dsize);
		mbInit(&mb, b->data, ms->dsize, ms->dsize/BytesPerEntry);
	}

	p = mbAlloc(&mb, n);
	if(p == nil){
		/* mbAlloc might have changed block */
		mbPack(&mb);
		blockDirty(b);
		vtSetError(EBadMeta);
		goto Err;
	}

	mbSearch(&mb, dir->elem, &i, &me);
	assert(me.p == nil);
	me.p = p;
	me.size = n;
	dePack(dir, &me);
	mbInsert(&mb, i, &me);
	mbPack(&mb);

	/* meta block depends on super block for qid ... */
	bb = cacheLocal(b->c, PartSuper, 0, OReadOnly);
	blockDependency(b, bb, -1, nil, nil);
	blockPut(bb);

	/* ... and one or two dir entries */
	epb = s->dsize/VtEntrySize;
	bb = sourceBlock(s, dir->entry/epb, OReadOnly);
	blockDependency(b, bb, -1, nil, nil);
	blockPut(bb);
	if(dir->mode & ModeDir){
		bb = sourceBlock(s, dir->mentry/epb, OReadOnly);
		blockDependency(b, bb, -1, nil, nil);
		blockPut(bb);
	}

	blockDirty(b);
	blockPut(b);
	return bo;
Err:
	blockPut(b);
	return NilBlock;
}

static int
chkSource(File *f)
{
	if(f->partial)
		return 1;

	if(f->source == nil || (f->dir.mode & ModeDir) && f->msource == nil){
		vtSetError(ERemoved);
		return 0;
	}
	return 1;
}

static int
fileRLock(File *f)
{
	assert(!vtCanLock(f->fs->elk));
	vtRLock(f->lk);
	if(!chkSource(f)){
		fileRUnlock(f);
		return 0;
	}
	return 1;
}

static void
fileRUnlock(File *f)
{
	vtRUnlock(f->lk);
}

static int
fileLock(File *f)
{
	assert(!vtCanLock(f->fs->elk));
	vtLock(f->lk);
	if(!chkSource(f)){
		fileUnlock(f);
		return 0;
	}
	return 1;
}

static void
fileUnlock(File *f)
{
	vtUnlock(f->lk);
}

/*
 * f->source and f->msource must NOT be locked.
 * fileMetaFlush locks the fileMeta and then the source (in fileMetaFlush2).
 * We have to respect that ordering.
 */
static void
fileMetaLock(File *f)
{
if(f->up == nil)
fprint(2, "f->elem = %s\n", f->dir.elem);
	assert(f->up != nil);
	assert(!vtCanLock(f->fs->elk));
	vtLock(f->up->lk);
}

static void
fileMetaUnlock(File *f)
{
	vtUnlock(f->up->lk);
}

/*
 * f->source and f->msource must NOT be locked.
 * see fileMetaLock.
 */
static void
fileRAccess(File* f)
{
	if(f->mode == OReadOnly || f->fs->noatimeupd)
		return;

	fileMetaLock(f);
	f->dir.atime = time(0L);
	f->dirty = 1;
	fileMetaUnlock(f);
}

/*
 * f->source and f->msource must NOT be locked.
 * see fileMetaLock.
 */
static void
fileWAccess(File* f, char *mid)
{
	if(f->mode == OReadOnly)
		return;

	fileMetaLock(f);
	f->dir.atime = f->dir.mtime = time(0L);
	if(strcmp(f->dir.mid, mid) != 0){
		vtMemFree(f->dir.mid);
		f->dir.mid = vtStrDup(mid);
	}
	f->dir.mcount++;
	f->dirty = 1;
	fileMetaUnlock(f);

/*RSC: let's try this */
/*presotto - lets not
	if(f->up)
		fileWAccess(f->up, mid);
*/
}

static int
getEntry(Source *r, Entry *e, int checkepoch)
{
	u32int epoch;
	Block *b;

	if(r == nil){
		memset(&e, 0, sizeof e);
		return 1;
	}

	b = cacheGlobal(r->fs->cache, r->score, BtDir, r->tag, OReadOnly);
	if(b == nil)
		return 0;
	if(!entryUnpack(e, b->data, r->offset % r->epb)){
		blockPut(b);
		return 0;
	}
	epoch = b->l.epoch;
	blockPut(b);

	if(checkepoch){
		b = cacheGlobal(r->fs->cache, e->score, entryType(e), e->tag, OReadOnly);
		if(b){
			if(b->l.epoch >= epoch)
				fprint(2, "warning: entry %p epoch not older %#.8ux/%d %V/%d in getEntry\n",
					r, b->addr, b->l.epoch, r->score, epoch);
			blockPut(b);
		}
	}

	return 1;
}

static int
setEntry(Source *r, Entry *e)
{
	Block *b;
	Entry oe;

	b = cacheGlobal(r->fs->cache, r->score, BtDir, r->tag, OReadWrite);
	if(0) fprint(2, "setEntry: b %#ux %d score=%V\n", b->addr, r->offset % r->epb, e->score);
	if(b == nil)
		return 0;
	if(!entryUnpack(&oe, b->data, r->offset % r->epb)){
		blockPut(b);
		return 0;
	}
	e->gen = oe.gen;
	entryPack(e, b->data, r->offset % r->epb);

	/* BUG b should depend on the entry pointer */

	blockDirty(b);
	blockPut(b);
	return 1;
}

/* assumes hold elk */
int
fileSnapshot(File *dst, File *src, u32int epoch, int doarchive)
{
	Entry e, ee;

	/* add link to snapshot */
	if(!getEntry(src->source, &e, 1) || !getEntry(src->msource, &ee, 1))
		return 0;

	e.snap = epoch;
	e.archive = doarchive;
	ee.snap = epoch;
	ee.archive = doarchive;

	if(!setEntry(dst->source, &e) || !setEntry(dst->msource, &ee))
		return 0;
	return 1;
}

int
fileGetSources(File *f, Entry *e, Entry *ee)
{
	if(!getEntry(f->source, e, 0)
	|| !getEntry(f->msource, ee, 0))
		return 0;
	return 1;
}

/*
 * Walk down to the block(s) containing the Entries
 * for f->source and f->msource, copying as we go.
 */
int
fileWalkSources(File *f)
{
	if(f->mode == OReadOnly){
		fprint(2, "readonly in fileWalkSources\n");
		return 1;
	}
	if(!sourceLock2(f->source, f->msource, OReadWrite)){
		fprint(2, "sourceLock2 failed in fileWalkSources\n");
		return 0;
	}
	sourceUnlock(f->source);
	sourceUnlock(f->msource);
	return 1;
}

/*
 * convert File* to full path name in malloced string.
 * this hasn't been as useful as we hoped it would be.
 */
char *
fileName(File *f)
{
	char *name, *pname;
	File *p;
	static char root[] = "/";

	if (f == nil)
		return vtStrDup("/**GOK**");

	p = fileGetParent(f);
	if (p == f)
		name = vtStrDup(root);
	else {
		pname = fileName(p);
		if (strcmp(pname, root) == 0)
			name = smprint("/%s", f->dir.elem);
		else
			name = smprint("%s/%s", pname, f->dir.elem);
		free(pname);
	}
	fileDecRef(p);
	return name;
}
