#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

/*
 * locking order is upwards.  A thread can hold the lock for a VacFile
 * and then acquire the lock of its parent
 */
struct VacFile
{
	VacFs	*fs;	/* immutable */

	/* meta data for file: protected by the lk in the parent */
	int		ref;	/* holds this data structure up */

	int		partial;	/* file was never really open */
	int		removed;	/* file has been removed */
	int		dirty;	/* dir is dirty with respect to meta data in block */
	u32int	boff;		/* block offset within msource for this file's metadata */
	VacDir	dir;		/* metadata for this file */
	VacFile	*up;		/* parent file */
	VacFile	*next;	/* sibling */

	RWLock	lk;		/* lock for the following */
	VtFile	*source;	/* actual data */
	VtFile	*msource;	/* metadata for children in a directory */
	VacFile	*down;	/* children */
	int		mode;
};

static int		filelock(VacFile*);
static u32int	filemetaalloc(VacFile*, VacDir*, u32int);
static int		filemetaflush2(VacFile*, char*);
static void		filemetalock(VacFile*);
static void		filemetaunlock(VacFile*);
static void		fileraccess(VacFile*);
static int		filerlock(VacFile*);
static void		filerunlock(VacFile*);
static void		fileunlock(VacFile*);
static void		filewaccess(VacFile*, char*);

void mbinit(MetaBlock*, u8int*, uint, uint);
int mbsearch(MetaBlock*, char*, int*, MetaEntry*);
int mbresize(MetaBlock*, MetaEntry*, int);
VacFile *vdlookup(VacFile*, char*);

static VacFile*
filealloc(VacFs *fs)
{
	VacFile *f;

	f = vtmallocz(sizeof(VacFile));
	f->ref = 1;
	f->fs = fs;
	f->boff = NilBlock;
	f->mode = fs->mode;
	return f;
}

static void
filefree(VacFile *f)
{
	vtfileclose(f->source);
	vtfileclose(f->msource);
	vdcleanup(&f->dir);
	memset(f, ~0, sizeof *f);	/* paranoia */
	vtfree(f);
}

/*
 * the file is locked already
 * f->msource is unlocked
 */
static VacFile*
dirlookup(VacFile *f, char *elem)
{
	int i;
	MetaBlock mb;
	MetaEntry me;
	VtBlock *b;
	VtFile *meta;
	VacFile *ff;
	u32int bo, nb;

	meta = f->msource;
	b = nil;
	if(vtfilelock(meta, -1) < 0)
		return nil;
	nb = (vtfilegetsize(meta)+meta->dsize-1)/meta->dsize;
	for(bo=0; bo<nb; bo++){
		b = vtfileblock(meta, bo, VtOREAD);
		if(b == nil)
			goto Err;
		if(mbunpack(&mb, b->data, meta->dsize) < 0)
			goto Err;
		if(mbsearch(&mb, elem, &i, &me) >= 0){
			ff = filealloc(f->fs);
			if(vdunpack(&ff->dir, &me) < 0){
				filefree(ff);
				goto Err;
			}
			vtfileunlock(meta);
			vtblockput(b);
			ff->boff = bo;
			ff->mode = f->mode;
			return ff;
		}

		vtblockput(b);
		b = nil;
	}
	werrstr(ENoFile);
	/* fall through */
Err:
	vtfileunlock(meta);
	vtblockput(b);
	return nil;
}

VacFile*
_vacfileroot(VacFs *fs, VtFile *r)
{
	VtBlock *b;
	VtFile *r0, *r1, *r2;
	MetaBlock mb;
	MetaEntry me;
	VacFile *root, *mr;

	b = nil;
	root = nil;
	mr = nil;
	r1 = nil;
	r2 = nil;

	if(vtfilelock(r, -1) < 0)
		return nil;
	r0 = vtfileopen(r, 0, fs->mode);
	if(r0 == nil)
		goto Err;
	r1 = vtfileopen(r, 1, fs->mode);
	if(r1 == nil)
		goto Err;
	r2 = vtfileopen(r, 2, fs->mode);
	if(r2 == nil)
		goto Err;

	mr = filealloc(fs);
	mr->msource = r2;
	r2 = nil;

	root = filealloc(fs);
	root->boff = 0;
	root->up = mr;
	root->source = r0;
	r0 = nil;
	root->msource = r1;
	r1 = nil;

	mr->down = root;

	if(vtfilelock(mr->msource, -1) < 0)
		goto Err;
	b = vtfileblock(mr->msource, 0, VtOREAD);
	vtfileunlock(mr->msource);
	if(b == nil)
		goto Err;

	if(mbunpack(&mb, b->data, mr->msource->dsize) < 0)
		goto Err;

	meunpack(&me, &mb, 0);
	if(vdunpack(&root->dir, &me) < 0)
		goto Err;
	vtblockput(b);
	vtfileunlock(r);
	fileraccess(root);

	return root;
Err:
	vtblockput(b);
	if(r0)
		vtfileclose(r0);
	if(r1)
		vtfileclose(r1);
	if(r2)
		vtfileclose(r2);
	if(mr)
		filefree(mr);
	if(root)
		filefree(root);
	vtfileunlock(r);

	return nil;
}

static VtFile *
fileopensource(VacFile *f, u32int offset, u32int gen, int dir, uint mode)
{
	VtFile *r;

	if(vtfilelock(f->source, mode) < 0)
		return nil;
	r = vtfileopen(f->source, offset, mode);
	vtfileunlock(f->source);
	if(r == nil)
		return nil;
	if(r->gen != gen){
		werrstr(ERemoved);
		goto Err;
	}
	if(r->dir != dir && r->mode != -1){
fprint(2, "fileopensource: dir mismatch %d %d\n", r->dir, dir);
		werrstr(EBadMeta);
		goto Err;
	}
	return r;
Err:
	vtfileclose(r);
	return nil;
}

VacFile*
_filewalk(VacFile *f, char *elem, int partial)
{
	VacFile *ff;

	fileraccess(f);

	if(elem[0] == 0){
		werrstr(EBadPath);
		return nil;
	}

	if(!vacfileisdir(f)){
		werrstr(ENotDir);
		return nil;
	}

	if(strcmp(elem, ".") == 0){
		return vacfileincref(f);
	}

	if(strcmp(elem, "..") == 0){
		if(vacfileisroot(f))
			return vacfileincref(f);
		return vacfileincref(f->up);
	}

	if(filelock(f) < 0)
		return nil;

	for(ff = f->down; ff; ff=ff->next){
		if(strcmp(elem, ff->dir.elem) == 0 && !ff->removed){
			ff->ref++;
			goto Exit;
		}
	}

	ff = dirlookup(f, elem);
	if(ff == nil)
		goto Err;

	if(ff->dir.mode & ModeSnapshot)
		ff->mode = VtOREAD;

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
		ff->source = fileopensource(f, ff->dir.entry, ff->dir.gen, 1, ff->mode);
		ff->msource = fileopensource(f, ff->dir.mentry, ff->dir.mgen, 0, ff->mode);
		if(ff->source == nil || ff->msource == nil)
			goto Err;
	}else{
		ff->source = fileopensource(f, ff->dir.entry, ff->dir.gen, 0, ff->mode);
		if(ff->source == nil)
			goto Err;
	}

	/* link in and up parent ref count */
	ff->next = f->down;
	f->down = ff;
	ff->up = f;
	vacfileincref(f);
Exit:
	fileunlock(f);
	return ff;
Err:
	fileunlock(f);
	if(ff != nil)
		vacfiledecref(ff);
	return nil;
}

VacFile*
vacfilewalk(VacFile *f, char *elem)
{
	return _filewalk(f, elem, 0);
}

VacFile*
_fileopen(VacFs *fs, char *path, int partial)
{
	VacFile *f, *ff;
	char *p, elem[VtMaxStringSize], *opath;
	int n;

	f = fs->root;
	vacfileincref(f);
	opath = path;
	while(*path != 0){
		for(p = path; *p && *p != '/'; p++)
			;
		n = p - path;
		if(n > 0){
			if(n > VtMaxStringSize){
				werrstr("%s: element too long", EBadPath);
				goto Err;
			}
			memmove(elem, path, n);
			elem[n] = 0;
			ff = _filewalk(f, elem, partial && *p=='\0');
			if(ff == nil){
				werrstr("%.*s: %R", utfnlen(opath, p-opath), opath);
				goto Err;
			}
			vacfiledecref(f);
			f = ff;
		}
		if(*p == '/')
			p++;
		path = p;
	}
	return f;
Err:
	vacfiledecref(f);
	return nil;
}

VacFile*
vacfileopen(VacFs *fs, char *path)
{
	return _fileopen(fs, path, 0);
}

#if 0
static void
filesettmp(VacFile *f, int istmp)
{
	int i;
	VtEntry e;
	VtFile *r;

	for(i=0; i<2; i++){
		if(i==0)
			r = f->source;
		else
			r = f->msource;
		if(r == nil)
			continue;
		if(vtfilegetentry(r, &e) < 0){
			fprint(2, "vtfilegetentry failed (cannot happen): %r\n");
			continue;
		}
		if(istmp)
			e.flags |= VtEntryNoArchive;
		else
			e.flags &= ~VtEntryNoArchive;
		if(vtfilesetentry(r, &e) < 0){
			fprint(2, "vtfilesetentry failed (cannot happen): %r\n");
			continue;
		}
	}
}
#endif

VacFile*
vacfilecreate(VacFile *f, char *elem, ulong mode, char *uid)
{
	VacFile *ff;
	VacDir *dir;
	VtFile *pr, *r, *mr;
	int isdir;

	if(filelock(f) < 0)
		return nil;

	r = nil;
	mr = nil;
	for(ff = f->down; ff; ff=ff->next){
		if(strcmp(elem, ff->dir.elem) == 0 && !ff->removed){
			ff = nil;
			werrstr(EExists);
			goto Err1;
		}
	}

	ff = dirlookup(f, elem);
	if(ff != nil){
		werrstr(EExists);
		goto Err1;
	}

	pr = f->source;
	if(pr->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err1;
	}

	if(vtfilelock2(f->source, f->msource, -1) < 0)
		goto Err1;

	ff = filealloc(f->fs);
	isdir = mode & ModeDir;

	r = vtfilecreate(pr, pr->psize, pr->dsize, isdir ? VtDirType : VtDataType);
	if(r == nil)
		goto Err;
	if(isdir){
		mr = vtfilecreate(pr, pr->psize, pr->dsize, VtDataType);
		if(mr == nil)
			goto Err;
	}

	dir = &ff->dir;
	dir->elem = vtstrdup(elem);
	dir->entry = r->offset;
	dir->gen = r->gen;
	if(isdir){
		dir->mentry = mr->offset;
		dir->mgen = mr->gen;
	}
	dir->size = 0;
	if(_vacfsnextqid(f->fs, &dir->qid) < 0)
		goto Err;
	dir->uid = vtstrdup(uid);
	dir->gid = vtstrdup(f->dir.gid);
	dir->mid = vtstrdup(uid);
	dir->mtime = time(0L);
	dir->mcount = 0;
	dir->ctime = dir->mtime;
	dir->atime = dir->mtime;
	dir->mode = mode;

	ff->boff = filemetaalloc(f, dir, 0);
	if(ff->boff == NilBlock)
		goto Err;

	vtfileunlock(f->source);
	vtfileunlock(f->msource);

	ff->source = r;
	ff->msource = mr;

#if 0
	if(mode&ModeTemporary){
		if(vtfilelock2(r, mr, -1) < 0)
			goto Err1;
		filesettmp(ff, 1);
		vtfileunlock(r);
		if(mr)
			vtfileunlock(mr);
	}
#endif

	/* committed */

	/* link in and up parent ref count */
	ff->next = f->down;
	f->down = ff;
	ff->up = f;
	vacfileincref(f);

	filewaccess(f, uid);

	fileunlock(f);
	return ff;

Err:
	vtfileunlock(f->source);
	vtfileunlock(f->msource);
Err1:
	if(r){
		vtfilelock(r, -1);
		vtfileremove(r);
	}
	if(mr){
		vtfilelock(mr, -1);
		vtfileremove(mr);
	}
	if(ff)
		vacfiledecref(ff);
	fileunlock(f);
	return nil;
}

int
vacfileblockscore(VacFile *f, u32int bn, u8int *score)
{
	VtFile *s;
	uvlong size;
	int dsize, ret;

	ret = -1;
	if(filerlock(f) < 0)
		return -1;
	fileraccess(f);
	if(vtfilelock(f->source, VtOREAD) < 0)
		goto out;

	s = f->source;
	dsize = s->dsize;
	size = vtfilegetsize(s);
	if((uvlong)bn*dsize >= size)
		goto out;
	ret = vtfileblockscore(f->source, bn, score);

out:
	vtfileunlock(f->source);
	filerunlock(f);
	return ret;
}

int
vacfileread(VacFile *f, void *buf, int cnt, vlong offset)
{
	VtFile *s;
	uvlong size;
	u32int bn;
	int off, dsize, n, nn;
	VtBlock *b;
	uchar *p;

if(0)fprint(2, "fileRead: %s %d, %lld\n", f->dir.elem, cnt, offset);

	if(filerlock(f) < 0)
		return -1;

	if(offset < 0){
		werrstr(EBadOffset);
		goto Err1;
	}

	fileraccess(f);

	if(vtfilelock(f->source, VtOREAD) < 0)
		goto Err1;

	s = f->source;
	dsize = s->dsize;
	size = vtfilegetsize(s);

	if(offset >= size)
		offset = size;

	if(cnt > size-offset)
		cnt = size-offset;
	bn = offset/dsize;
	off = offset%dsize;
	p = buf;
	while(cnt > 0){
		b = vtfileblock(s, bn, OREAD);
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
		vtblockput(b);
	}
	vtfileunlock(s);
	filerunlock(f);
	return p-(uchar*)buf;

Err:
	vtfileunlock(s);
Err1:
	filerunlock(f);
	return -1;
}

#if 0
/* 
 * Changes the file block bn to be the given block score.
 * Very sneaky.  Only used by flfmt.
 */
int
filemapblock(VacFile *f, ulong bn, uchar score[VtScoreSize], ulong tag)
{
	VtBlock *b;
	VtEntry e;
	VtFile *s;

	if(filelock(f) < 0)
		return -1;

	s = nil;
	if(f->dir.mode & ModeDir){
		werrstr(ENotFile);
		goto Err;
	}

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}

	if(vtfilelock(f->source, -1) < 0)
		goto Err;

	s = f->source;
	b = _vtfileblock(s, bn, VtORDWR, 1, tag);
	if(b == nil)
		goto Err;

	if(vtfilegetentry(s, &e) < 0)
		goto Err;
	if(b->l.type == BtDir){
		memmove(e.score, score, VtScoreSize);
		assert(e.tag == tag || e.tag == 0);
		e.tag = tag;
		e.flags |= VtEntryLocal;
		vtentrypack(&e, b->data, f->source->offset % f->source->epb);
	}else
		memmove(b->data + (bn%(e.psize/VtScoreSize))*VtScoreSize, score, VtScoreSize);
	/* vtblockdirty(b); */
	vtblockput(b);
	vtfileunlock(s);
	fileunlock(f);
	return 0;

Err:
	if(s)
		vtfileunlock(s);
	fileunlock(f);
	return -1;
}
#endif

int
vacfilesetsize(VacFile *f, uvlong size)
{
	int r;

	if(filelock(f) < 0)
		return -1;
	r = 0;
	if(f->dir.mode & ModeDir){
		werrstr(ENotFile);
		goto Err;
	}
	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}
	if(vtfilelock(f->source, -1) < 0)
		goto Err;
	r = vtfilesetsize(f->source, size);
	vtfileunlock(f->source);
Err:
	fileunlock(f);
	return r;
}

int
filewrite(VacFile *f, void *buf, int cnt, vlong offset, char *uid)
{
	VtFile *s;
	ulong bn;
	int off, dsize, n;
	VtBlock *b;
	uchar *p;
	vlong eof;

if(0)fprint(2, "fileWrite: %s %d, %lld\n", f->dir.elem, cnt, offset);

	if(filelock(f) < 0)
		return -1;

	s = nil;
	if(f->dir.mode & ModeDir){
		werrstr(ENotFile);
		goto Err;
	}

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}
	if(offset < 0){
		werrstr(EBadOffset);
		goto Err;
	}

	filewaccess(f, uid);

	if(vtfilelock(f->source, -1) < 0)
		goto Err;
	s = f->source;
	dsize = s->dsize;

	eof = vtfilegetsize(s);
	if(f->dir.mode & ModeAppend)
		offset = eof;
	bn = offset/dsize;
	off = offset%dsize;
	p = buf;
	while(cnt > 0){
		n = cnt;
		if(n > dsize-off)
			n = dsize-off;
		b = vtfileblock(s, bn, n<dsize?VtORDWR:VtOWRITE);
		if(b == nil){
			if(offset > eof)
				vtfilesetsize(s, offset);
			goto Err;
		}
		memmove(b->data+off, p, n);
		off = 0;
		cnt -= n;
		p += n;
		offset += n;
		bn++;
		/* vtblockdirty(b); */
		vtblockput(b);
	}
	if(offset > eof && vtfilesetsize(s, offset) < 0)
		goto Err;
	vtfileunlock(s);
	fileunlock(f);
	return p-(uchar*)buf;
Err:
	if(s)
		vtfileunlock(s);
	fileunlock(f);
	return -1;
}

int
vacfilegetdir(VacFile *f, VacDir *dir)
{
	if(filerlock(f) < 0)
		return -1;

	filemetalock(f);
	vdcopy(dir, &f->dir);
	filemetaunlock(f);

	if(vacfileisdir(f) < 0){
		if(vtfilelock(f->source, VtOREAD) < 0){
			filerunlock(f);
			return -1;
		}
		dir->size = vtfilegetsize(f->source);
		vtfileunlock(f->source);
	}
	filerunlock(f);

	return 0;
}

int
vacfiletruncate(VacFile *f, char *uid)
{
	if(vacfileisdir(f)){
		werrstr(ENotFile);
		return -1;
	}

	if(filelock(f) < 0)
		return -1;

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		fileunlock(f);
		return -1;
	}
	if(vtfilelock(f->source, -1) < 0){
		fileunlock(f);
		return -1;
	}
	if(vtfiletruncate(f->source) < 0){
		vtfileunlock(f->source);
		fileunlock(f);
		return -1;
	}
	vtfileunlock(f->source);
	fileunlock(f);

	filewaccess(f, uid);

	return 0;
}

int
vacfilesetdir(VacFile *f, VacDir *dir, char *uid)
{
	VacFile *ff;
	char *oelem;
	u32int mask;
	u64int size;

	/* can not set permissions for the root */
	if(vacfileisroot(f)){
		werrstr(ERoot);
		return -1;
	}

	if(filelock(f) < 0)
		return -1;

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		fileunlock(f);
		return -1;
	}

	filemetalock(f);

	/* check new name does not already exist */
	if(strcmp(f->dir.elem, dir->elem) != 0){
		for(ff = f->up->down; ff; ff=ff->next){
			if(strcmp(dir->elem, ff->dir.elem) == 0 && !ff->removed){
				werrstr(EExists);
				goto Err;
			}
		}

		ff = dirlookup(f->up, dir->elem);
		if(ff != nil){
			vacfiledecref(ff);
			werrstr(EExists);
			goto Err;
		}
	}

	if(vtfilelock2(f->source, f->msource, -1) < 0)
		goto Err;
	if(!vacfileisdir(f)){
		size = vtfilegetsize(f->source);
		if(size != dir->size){
			if(vtfilesetsize(f->source, dir->size) < 0){
				vtfileunlock(f->source);
				if(f->msource)
					vtfileunlock(f->msource);
				goto Err;
			}
			/* commited to changing it now */
		}
	}
	/* commited to changing it now */
#if 0
	if((f->dir.mode&ModeTemporary) != (dir->mode&ModeTemporary))
		filesettmp(f, dir->mode&ModeTemporary);
#endif
	vtfileunlock(f->source);
	if(f->msource)
		vtfileunlock(f->msource);

	oelem = nil;
	if(strcmp(f->dir.elem, dir->elem) != 0){
		oelem = f->dir.elem;
		f->dir.elem = vtstrdup(dir->elem);
	}

	if(strcmp(f->dir.uid, dir->uid) != 0){
		vtfree(f->dir.uid);
		f->dir.uid = vtstrdup(dir->uid);
	}

	if(strcmp(f->dir.gid, dir->gid) != 0){
		vtfree(f->dir.gid);
		f->dir.gid = vtstrdup(dir->gid);
	}

	f->dir.mtime = dir->mtime;
	f->dir.atime = dir->atime;

//fprint(2, "mode %x %x ", f->dir.mode, dir->mode);
	mask = ~(ModeDir|ModeSnapshot);
	f->dir.mode &= ~mask;
	f->dir.mode |= mask & dir->mode;
	f->dirty = 1;
//fprint(2, "->%x\n", f->dir.mode);

	filemetaflush2(f, oelem);
	vtfree(oelem);

	filemetaunlock(f);
	fileunlock(f);

	filewaccess(f->up, uid);

	return 0;
Err:
	filemetaunlock(f);
	fileunlock(f);
	return -1;
}

int
vacfilesetqidspace(VacFile *f, u64int offset, u64int max)
{
	int ret;

	if(filelock(f) < 0)
		return -1;
	filemetalock(f);
	f->dir.qidspace = 1;
	f->dir.qidoffset = offset;
	f->dir.qidmax = max;
	ret = filemetaflush2(f, nil);
	filemetaunlock(f);
	fileunlock(f);
	return ret;
}

uvlong
vacfilegetid(VacFile *f)
{
	/* immutable */
	return f->dir.qid;
}

ulong
vacfilegetmcount(VacFile *f)
{
	ulong mcount;

	filemetalock(f);
	mcount = f->dir.mcount;
	filemetaunlock(f);
	return mcount;
}

ulong
vacfilegetmode(VacFile *f)
{
	ulong mode;

	filemetalock(f);
	mode = f->dir.mode;
	filemetaunlock(f);
	return mode;
}

int
vacfileisdir(VacFile *f)
{
	/* immutable */
	return (f->dir.mode & ModeDir) != 0;
}

int
vacfileisroot(VacFile *f)
{
	return f == f->fs->root;
}

int
vacfilegetsize(VacFile *f, uvlong *size)
{
	if(filerlock(f) < 0)
		return 0;
	if(vtfilelock(f->source, VtOREAD) < 0){
		filerunlock(f);
		return -1;
	}
	*size = vtfilegetsize(f->source);
	vtfileunlock(f->source);
	filerunlock(f);

	return 0;
}

int
vacfilegetvtentry(VacFile *f, VtEntry *e)
{
	if(filerlock(f) < 0)
		return 0;
	if(vtfilelock(f->source, VtOREAD) < 0){
		filerunlock(f);
		return -1;
	}
	vtfilegetentry(f->source, e);
	vtfileunlock(f->source);
	filerunlock(f);

	return 0;
}

void
vacfilemetaflush(VacFile *f, int rec)
{
	VacFile **kids, *p;
	int nkids;
	int i;

	filemetalock(f);
	filemetaflush2(f, nil);
	filemetaunlock(f);

	if(!rec || !vacfileisdir(f))
		return;

	if(filelock(f) < 0)
		return;
	nkids = 0;
	for(p=f->down; p; p=p->next)
		nkids++;
	kids = vtmalloc(nkids*sizeof(VacFile*));
	i = 0;
	for(p=f->down; p; p=p->next){
		kids[i++] = p;
		p->ref++;
	}
	fileunlock(f);

	for(i=0; i<nkids; i++){
		vacfilemetaflush(kids[i], 1);
		vacfiledecref(kids[i]);
	}
	vtfree(kids);
}

/* assumes metaLock is held */
static int
filemetaflush2(VacFile *f, char *oelem)
{
	VacFile *fp;
	VtBlock *b, *bb;
	MetaBlock mb;
	MetaEntry me, me2;
	int i, n;
	u32int boff;

	if(!f->dirty)
		return 0;

	if(oelem == nil)
		oelem = f->dir.elem;

	fp = f->up;

	if(vtfilelock(fp->msource, -1) < 0)
		return 0;
	/* can happen if source is clri'ed out from under us */
	if(f->boff == NilBlock)
		goto Err1;
	b = vtfileblock(fp->msource, f->boff, VtORDWR);
	if(b == nil)
		goto Err1;

	if(mbunpack(&mb, b->data, fp->msource->dsize) < 0)
		goto Err;
	if(mbsearch(&mb, oelem, &i, &me) < 0)
		goto Err;

	n = vdsize(&f->dir);
if(0)fprint(2, "old size %d new size %d\n", me.size, n);

	if(mbresize(&mb, &me, n) >= 0){
		/* fits in the block */
		mbdelete(&mb, i, &me);
		if(strcmp(f->dir.elem, oelem) != 0)
			mbsearch(&mb, f->dir.elem, &i, &me2);
		vdpack(&f->dir, &me);
		mbinsert(&mb, i, &me);
		mbpack(&mb);
		/* vtblockdirty(b); */
		vtblockput(b);
		vtfileunlock(fp->msource);
		f->dirty = 0;
		return -1;
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
	boff = filemetaalloc(fp, &f->dir, f->boff+1);
	if(boff == NilBlock){
		/* mbResize might have modified block */
		mbpack(&mb);
		/* vtblockdirty(b); */
		goto Err;
	}
fprint(2, "fileMetaFlush moving entry from %ud -> %ud\n", f->boff, boff);
	f->boff = boff;

	/* make sure deletion goes to disk after new entry */
	bb = vtfileblock(fp->msource, f->boff, VtORDWR);
	mbdelete(&mb, i, &me);
	mbpack(&mb);
//	blockDependency(b, bb, -1, nil, nil);
	vtblockput(bb);
	/* vtblockdirty(b); */
	vtblockput(b);
	vtfileunlock(fp->msource);

	f->dirty = 0;

	return 0;

Err:
	vtblockput(b);
Err1:
	vtfileunlock(fp->msource);
	return -1;
}

static int
filemetaremove(VacFile *f, char *uid)
{
	VtBlock *b;
	MetaBlock mb;
	MetaEntry me;
	int i;
	VacFile *up;

	b = nil;
	up = f->up;
	filewaccess(up, uid);
	filemetalock(f);

	if(vtfilelock(up->msource, VtORDWR) < 0)
		goto Err;
	b = vtfileblock(up->msource, f->boff, VtORDWR);
	if(b == nil)
		goto Err;

	if(mbunpack(&mb, b->data, up->msource->dsize) < 0)
		goto Err;
	if(mbsearch(&mb, f->dir.elem, &i, &me) < 0)
		goto Err;
	mbdelete(&mb, i, &me);
	mbpack(&mb);
	vtfileunlock(up->msource);

	/* vtblockdirty(b); */
	vtblockput(b);

	f->removed = 1;
	f->boff = NilBlock;
	f->dirty = 0;

	filemetaunlock(f);
	return 0;

Err:
	vtfileunlock(up->msource);
	vtblockput(b);
	filemetaunlock(f);
	return -1;
}

/* assume file is locked, assume f->msource is locked */
static int
filecheckempty(VacFile *f)
{
	u32int i, n;
	VtBlock *b;
	MetaBlock mb;
	VtFile *r;

	r = f->msource;
	n = (vtfilegetsize(r)+r->dsize-1)/r->dsize;
	for(i=0; i<n; i++){
		b = vtfileblock(r, i, VtORDWR);
		if(b == nil)
			goto Err;
		if(mbunpack(&mb, b->data, r->dsize) < 0)
			goto Err;
		if(mb.nindex > 0){
			werrstr(ENotEmpty);
			goto Err;
		}
		vtblockput(b);
	}
	return 0;
Err:
	vtblockput(b);
	return -1;
}

int
vacfileremove(VacFile *f, char *muid)
{
	VacFile *ff;

	/* can not remove the root */
	if(vacfileisroot(f)){
		werrstr(ERoot);
		return -1;
	}

	if(filelock(f) < 0)
		return -1;

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err1;
	}
	if(vtfilelock2(f->source, f->msource, -1) < 0)
		goto Err1;
	if(vacfileisdir(f) && filecheckempty(f)<0)
		goto Err;

	for(ff=f->down; ff; ff=ff->next)
		assert(ff->removed);

	vtfileremove(f->source);
	f->source = nil;
	if(f->msource){
		vtfileremove(f->msource);
		f->msource = nil;
	}

	fileunlock(f);

	if(filemetaremove(f, muid) < 0)
		return -1;

	return 0;

Err:
	vtfileunlock(f->source);
	if(f->msource)
		vtfileunlock(f->msource);
Err1:
	fileunlock(f);
	return -1;
}

static int
clri(VacFile *f, char *uid)
{
	int r;

	if(f == nil)
		return -1;
	if(f->up->source->mode != VtORDWR){
		werrstr(EReadOnly);
		vacfiledecref(f);
		return -1;
	}
	r = filemetaremove(f, uid);
	vacfiledecref(f);
	return r;
}

int
vacfileclripath(VacFs *fs, char *path, char *uid)
{
	return clri(_fileopen(fs, path, 1), uid);
}

int
vacfileclri(VacFile *dir, char *elem, char *uid)
{
	return clri(_filewalk(dir, elem, 1), uid);
}

VacFile*
vacfileincref(VacFile *vf)
{
	filemetalock(vf);
	assert(vf->ref > 0);
	vf->ref++;
	filemetaunlock(vf);
	return vf;
}

int
vacfiledecref(VacFile *f)
{
	VacFile *p, *q, **qq;

	if(f->up == nil){
		/* never linked in */
		assert(f->ref == 1);
		filefree(f);
		return 0;
	}

	filemetalock(f);
	f->ref--;
	if(f->ref > 0){
		filemetaunlock(f);
		return -1;
	}
	assert(f->ref == 0);
	assert(f->down == nil);

	filemetaflush2(f, nil);

	p = f->up;
	qq = &p->down;
	for(q = *qq; q; q = *qq){
		if(q == f)
			break;
		qq = &q->next;
	}
	assert(q != nil);
	*qq = f->next;

	filemetaunlock(f);
	filefree(f);
	vacfiledecref(p);
	return 0;
}

VacFile*
vacfilegetparent(VacFile *f)
{
	if(vacfileisroot(f))
		return vacfileincref(f);
	return vacfileincref(f->up);
}

int
vacfilewrite(VacFile *file, void *buf, int n, vlong offset, char *muid)
{
       werrstr("read only file system");
       return -1;
}

VacDirEnum*
vdeopen(VacFile *f)
{
	VacDirEnum *vde;
	VacFile *p;

	if(!vacfileisdir(f)){
		werrstr(ENotDir);
		return nil;
	}

	/* flush out meta data */
	if(filelock(f) < 0)
		return nil;
	for(p=f->down; p; p=p->next)
		filemetaflush2(p, nil);
	fileunlock(f);

	vde = vtmallocz(sizeof(VacDirEnum));
	vde->file = vacfileincref(f);

	return vde;
}

static int
direntrysize(VtFile *s, ulong elem, ulong gen, uvlong *size)
{
	VtBlock *b;
	ulong bn;
	VtEntry e;
	int epb;

	epb = s->dsize/VtEntrySize;
	bn = elem/epb;
	elem -= bn*epb;

	b = vtfileblock(s, bn, VtOREAD);
	if(b == nil)
		goto Err;
	if(vtentryunpack(&e, b->data, elem) < 0)
		goto Err;

	/* hanging entries are returned as zero size */
	if(!(e.flags & VtEntryActive) || e.gen != gen)
		*size = 0;
	else
		*size = e.size;
	vtblockput(b);
	return 0;

Err:
	vtblockput(b);
	return -1;
}

static int
vdefill(VacDirEnum *vde)
{
	int i, n;
	VtFile *meta, *source;
	MetaBlock mb;
	MetaEntry me;
	VacFile *f;
	VtBlock *b;
	VacDir *de;

	/* clean up first */
	for(i=vde->i; i<vde->n; i++)
		vdcleanup(vde->buf+i);
	vtfree(vde->buf);
	vde->buf = nil;
	vde->i = 0;
	vde->n = 0;

	f = vde->file;

	source = f->source;
	meta = f->msource;

	b = vtfileblock(meta, vde->boff, VtOREAD);
	if(b == nil)
		goto Err;
	if(mbunpack(&mb, b->data, meta->dsize) < 0)
		goto Err;

	n = mb.nindex;
	vde->buf = vtmalloc(n * sizeof(VacDir));

	for(i=0; i<n; i++){
		de = vde->buf + i;
		meunpack(&me, &mb, i);
		if(vdunpack(de, &me) < 0)
			goto Err;
		vde->n++;
		if(!(de->mode & ModeDir))
		if(direntrysize(source, de->entry, de->gen, &de->size) < 0)
			goto Err;
	}
	vde->boff++;
	vtblockput(b);
	return 0;
Err:
	vtblockput(b);
	return -1;
}

int
vderead(VacDirEnum *vde, VacDir *de)
{
	int ret, didread;
	VacFile *f;
	u32int nb;

	f = vde->file;
	if(filerlock(f) < 0)
		return -1;

	if(vtfilelock2(f->source, f->msource, VtOREAD) < 0){
		filerunlock(f);
		return -1;
	}

	nb = (vtfilegetsize(f->msource)+f->msource->dsize-1)/f->msource->dsize;

	didread = 0;
	while(vde->i >= vde->n){
		if(vde->boff >= nb){
			ret = 0;
			goto Return;
		}
		didread = 1;
		if(vdefill(vde) < 0){
			ret = -1;
			goto Return;
		}
	}

	memmove(de, vde->buf + vde->i, sizeof(VacDir));
	vde->i++;
	ret = 1;

Return:
	vtfileunlock(f->source);
	vtfileunlock(f->msource);
	filerunlock(f);

	if(didread)
		fileraccess(f);
	return ret;
}

void
vdeclose(VacDirEnum *vde)
{
	int i;
	if(vde == nil)
		return;
	for(i=vde->i; i<vde->n; i++)
		vdcleanup(vde->buf+i);
	vtfree(vde->buf);
	vacfiledecref(vde->file);
	vtfree(vde);
}

/*
 * caller must lock f->source and f->msource
 * caller must NOT lock the source and msource
 * referenced by dir.
 */
static u32int
filemetaalloc(VacFile *f, VacDir *dir, u32int start)
{
	u32int nb, bo;
	VtBlock *b;
	MetaBlock mb;
	int nn;
	uchar *p;
	int i, n;
	MetaEntry me;
	VtFile *s, *ms;

	s = f->source;
	ms = f->msource;

	n = vdsize(dir);
	nb = (vtfilegetsize(ms)+ms->dsize-1)/ms->dsize;
	b = nil;
	if(start > nb)
		start = nb;
	for(bo=start; bo<nb; bo++){
		b = vtfileblock(ms, bo, VtORDWR);
		if(b == nil)
			goto Err;
		if(mbunpack(&mb, b->data, ms->dsize) < 0)
			goto Err;
		nn = (mb.maxsize*FullPercentage/100) - mb.size + mb.free;
		if(n <= nn && mb.nindex < mb.maxindex)
			break;
		vtblockput(b);
		b = nil;
	}

	/* add block to meta file */
	if(b == nil){
		b = vtfileblock(ms, bo, VtORDWR);
		if(b == nil)
			goto Err;
		vtfilesetsize(ms, (nb+1)*ms->dsize);
		mbinit(&mb, b->data, ms->dsize, ms->dsize/BytesPerEntry);
	}

	p = mballoc(&mb, n);
	if(p == nil){
		/* mbAlloc might have changed block */
		mbpack(&mb);
		/* vtblockdirty(b); */
		werrstr(EBadMeta);
		goto Err;
	}

	mbsearch(&mb, dir->elem, &i, &me);
	assert(me.p == nil);
	me.p = p;
	me.size = n;
	vdpack(dir, &me);
	mbinsert(&mb, i, &me);
	mbpack(&mb);

#ifdef notdef /* XXX */
	/* meta block depends on super block for qid ... */
	bb = cacheLocal(b->c, PartSuper, 0, VtOREAD);
	blockDependency(b, bb, -1, nil, nil);
	vtblockput(bb);

	/* ... and one or two dir entries */
	epb = s->dsize/VtEntrySize;
	bb = vtfileblock(s, dir->entry/epb, VtOREAD);
	blockDependency(b, bb, -1, nil, nil);
	vtblockput(bb);
	if(dir->mode & ModeDir){
		bb = sourceBlock(s, dir->mentry/epb, VtOREAD);
		blockDependency(b, bb, -1, nil, nil);
		vtblockput(bb);
	}
#endif

	/* vtblockdirty(b); */
	vtblockput(b);
	return bo;
Err:
	vtblockput(b);
	return NilBlock;
}

static int
chksource(VacFile *f)
{
	if(f->partial)
		return 0;

	if(f->source == nil || (f->dir.mode & ModeDir) && f->msource == nil){
		werrstr(ERemoved);
		return -1;
	}
	return 0;
}

static int
filerlock(VacFile *f)
{
//	assert(!canwlock(&f->fs->elk));
	rlock(&f->lk);
	if(chksource(f) < 0){
		runlock(&f->lk);
		return -1;
	}
	return 0;
}

static void
filerunlock(VacFile *f)
{
	runlock(&f->lk);
}

static int
filelock(VacFile *f)
{
//	assert(!canwlock(&f->fs->elk));
	wlock(&f->lk);
	if(chksource(f) < 0){
		wunlock(&f->lk);
		return -1;
	}
	return 0;
}

static void
fileunlock(VacFile *f)
{
	wunlock(&f->lk);
}

/*
 * f->source and f->msource must NOT be locked.
 * fileMetaFlush locks the fileMeta and then the source (in fileMetaFlush2).
 * We have to respect that ordering.
 */
static void
filemetalock(VacFile *f)
{
	assert(f->up != nil);
//	assert(!canwlock(&f->fs->elk));
	wlock(&f->up->lk);
}

static void
filemetaunlock(VacFile *f)
{
	wunlock(&f->up->lk);
}

/*
 * f->source and f->msource must NOT be locked.
 * see filemetalock.
 */
static void
fileraccess(VacFile* f)
{
	if(f->mode == VtOREAD)
		return;

	filemetalock(f);
	f->dir.atime = time(0L);
	f->dirty = 1;
	filemetaunlock(f);
}

/*
 * f->source and f->msource must NOT be locked.
 * see filemetalock.
 */
static void
filewaccess(VacFile* f, char *mid)
{
	if(f->mode == VtOREAD)
		return;

	filemetalock(f);
	f->dir.atime = f->dir.mtime = time(0L);
	if(strcmp(f->dir.mid, mid) != 0){
		vtfree(f->dir.mid);
		f->dir.mid = vtstrdup(mid);
	}
	f->dir.mcount++;
	f->dirty = 1;
	filemetaunlock(f);

/*RSC: let's try this */
/*presotto - lets not
	if(f->up)
		filewaccess(f->up, mid);
*/
}

#if 0
static void
markCopied(Block *b)
{
	VtBlock *lb;
	Label l;

	if(globalToLocal(b->score) == NilBlock)
		return;

	if(!(b->l.state & BsCopied)){
		/*
		 * We need to record that there are now pointers in
		 * b that are not unique to b.  We do this by marking
		 * b as copied.  Since we don't return the label block,
		 * the caller can't get the dependencies right.  So we have
		 * to flush the block ourselves.  This is a rare occurrence.
		 */
		l = b->l;
		l.state |= BsCopied;
		lb = _blockSetLabel(b, &l);
	WriteAgain:
		while(!blockWrite(lb)){
			fprint(2, "getEntry: could not write label block\n");
			sleep(10*1000);
		}
		while(lb->iostate != BioClean && lb->iostate != BioDirty){
			assert(lb->iostate == BioWriting);
			vtSleep(lb->ioready);
		}
		if(lb->iostate == BioDirty)
			goto WriteAgain;
		vtblockput(lb);
	}
}

static int
getEntry(VtFile *r, Entry *e, int mark)
{
	Block *b;

	if(r == nil){
		memset(&e, 0, sizeof e);
		return 1;
	}

	b = cacheGlobal(r->fs->cache, r->score, BtDir, r->tag, VtOREAD);
	if(b == nil)
		return 0;
	if(!entryUnpack(e, b->data, r->offset % r->epb)){
		vtblockput(b);
		return 0;
	}

	if(mark)
		markCopied(b);
	vtblockput(b);
	return 1;
}

static int
setEntry(Source *r, Entry *e)
{
	Block *b;
	Entry oe;

	b = cacheGlobal(r->fs->cache, r->score, BtDir, r->tag, VtORDWR);
	if(0) fprint(2, "setEntry: b %#ux %d score=%V\n", b->addr, r->offset % r->epb, e->score);
	if(b == nil)
		return 0;
	if(!entryUnpack(&oe, b->data, r->offset % r->epb)){
		vtblockput(b);
		return 0;
	}
	e->gen = oe.gen;
	entryPack(e, b->data, r->offset % r->epb);

	/* BUG b should depend on the entry pointer */

	markCopied(b);
	/* vtblockdirty(b); */
	vtblockput(b);
	return 1;
}

/* assumes hold elk */
int
fileSnapshot(VacFile *dst, VacFile *src, u32int epoch, int doarchive)
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
fileGetSources(VacFile *f, Entry *e, Entry *ee, int mark)
{
	if(!getEntry(f->source, e, mark)
	|| !getEntry(f->msource, ee, mark))
		return 0;
	return 1;
}	

int
fileWalkSources(VacFile *f)
{
	if(f->mode == VtOREAD)
		return 1;
	if(!sourceLock2(f->source, f->msource, VtORDWR))
		return 0;
	vtfileunlock(f->source);
	vtfileunlock(f->msource);
	return 1;
}

#endif
