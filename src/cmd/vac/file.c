#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#define debug 0

/*
 * Vac file system.  This is a simplified version of the same code in Fossil.
 *
 * The locking order in the tree is upward: a thread can hold the lock
 * for a VacFile and then acquire the lock of f->up (the parent),
 * but not vice-versa.
 *
 * A vac file is one or two venti files.  Plain data files are one venti file,
 * while directores are two: a venti data file containing traditional
 * directory entries, and a venti directory file containing venti
 * directory entries.  The traditional directory entries in the data file
 * contain integers indexing into the venti directory entry file.
 * It's a little complicated, but it makes the data usable by standard
 * tools like venti/copy.
 *
 */

static int filemetaflush(VacFile*, char*);

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

	uvlong	qidoffset;	/* qid offset */
};

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

static int
chksource(VacFile *f)
{
	if(f->partial)
		return 0;

	if(f->source == nil
	|| ((f->dir.mode & ModeDir) && f->msource == nil)){
		werrstr(ERemoved);
		return -1;
	}
	return 0;
}

static int
filelock(VacFile *f)
{
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

static int
filerlock(VacFile *f)
{
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

/*
 * The file metadata, like f->dir and f->ref,
 * are synchronized via the parent's lock.
 * This is why locking order goes up.
 */
static void
filemetalock(VacFile *f)
{
	assert(f->up != nil);
	wlock(&f->up->lk);
}

static void
filemetaunlock(VacFile *f)
{
	wunlock(&f->up->lk);
}

uvlong
vacfilegetid(VacFile *f)
{
	/* immutable */
	return f->qidoffset + f->dir.qid;
}

uvlong
vacfilegetqidoffset(VacFile *f)
{
	return f->qidoffset;
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

/*
 * The files are reference counted, and while the reference
 * is bigger than zero, each file can be found in its parent's
 * f->down list (chains via f->next), so that multiple threads
 * end up sharing a VacFile* when referring to the same file.
 *
 * Each VacFile holds a reference to its parent.
 */
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

	if(f->source && vtfilelock(f->source, -1) >= 0){
		vtfileflush(f->source);
		vtfileunlock(f->source);
	}
	if(f->msource && vtfilelock(f->msource, -1) >= 0){
		vtfileflush(f->msource);
		vtfileunlock(f->msource);
	}

	/*
	 * Flush f's directory information to the cache.
	 */
	filemetaflush(f, nil);

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


/*
 * Construct a vacfile for the root of a vac tree, given the
 * venti file for the root information.  That venti file is a
 * directory file containing VtEntries for three more venti files:
 * the two venti files making up the root directory, and a
 * third venti file that would be the metadata half of the
 * "root's parent".
 *
 * Fossil generates slightly different vac files, due to a now
 * impossible-to-change bug, which contain a VtEntry
 * for just one venti file, that itself contains the expected
 * three directory entries.  Sigh.
 */
VacFile*
_vacfileroot(VacFs *fs, VtFile *r)
{
	int redirected;
	char err[ERRMAX];
	VtBlock *b;
	VtFile *r0, *r1, *r2;
	MetaBlock mb;
	MetaEntry me;
	VacFile *root, *mr;

	redirected = 0;
Top:
	b = nil;
	root = nil;
	mr = nil;
	r1 = nil;
	r2 = nil;

	if(vtfilelock(r, -1) < 0)
		return nil;
	r0 = vtfileopen(r, 0, fs->mode);
	if(debug)
		fprint(2, "r0 %p\n", r0);
	if(r0 == nil)
		goto Err;
	r2 = vtfileopen(r, 2, fs->mode);
	if(debug)
		fprint(2, "r2 %p\n", r2);
	if(r2 == nil){
		/*
		 * some vac files (e.g., from fossil)
		 * have an extra layer of indirection.
		 */
		rerrstr(err, sizeof err);
		if(!redirected && strstr(err, "not active")){
			redirected = 1;
			vtfileunlock(r);
			r = r0;
			goto Top;
		}
		goto Err;
	}
	r1 = vtfileopen(r, 1, fs->mode);
	if(debug)
		fprint(2, "r1 %p\n", r1);
	if(r1 == nil)
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
	vtfileunlock(r);

	if(vtfilelock(mr->msource, VtOREAD) < 0)
		goto Err1;
	b = vtfileblock(mr->msource, 0, VtOREAD);
	vtfileunlock(mr->msource);
	if(b == nil)
		goto Err1;

	if(mbunpack(&mb, b->data, mr->msource->dsize) < 0)
		goto Err1;

	meunpack(&me, &mb, 0);
	if(vdunpack(&root->dir, &me) < 0)
		goto Err1;
	vtblockput(b);

	return root;
Err:
	vtfileunlock(r);
Err1:
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

	return nil;
}

/*
 * Vac directories are a sequence of metablocks, each of which
 * contains a bunch of metaentries sorted by file name.
 * The whole sequence isn't sorted, though, so you still have
 * to look at every block to find a given name.
 * Dirlookup looks in f for an element name elem.
 * It returns a new VacFile with the dir, boff, and mode
 * filled in, but the sources (venti files) are not, and f is
 * not yet linked into the tree.  These details must be taken
 * care of by the caller.
 *
 * f must be locked, f->msource must not.
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
			ff->qidoffset = f->qidoffset + ff->dir.qidoffset;
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

/*
 * Open the venti file at offset in the directory f->source.
 * f is locked.
 */
static VtFile *
fileopensource(VacFile *f, u32int offset, u32int gen, int dir, uint mode)
{
	VtFile *r;

	if((r = vtfileopen(f->source, offset, mode)) == nil)
		return nil;
	if(r == nil)
		return nil;
	if(r->gen != gen){
		werrstr(ERemoved);
		vtfileclose(r);
		return nil;
	}
	if(r->dir != dir && r->mode != -1){
		werrstr(EBadMeta);
		vtfileclose(r);
		return nil;
	}
	return r;
}

VacFile*
vacfilegetparent(VacFile *f)
{
	if(vacfileisroot(f))
		return vacfileincref(f);
	return vacfileincref(f->up);
}

/*
 * Given an unlocked vacfile (directory) f,
 * return the vacfile named elem in f.
 * Interprets . and .. as a convenience to callers.
 */
VacFile*
vacfilewalk(VacFile *f, char *elem)
{
	VacFile *ff;

	if(elem[0] == 0){
		werrstr(EBadPath);
		return nil;
	}

	if(!vacfileisdir(f)){
		werrstr(ENotDir);
		return nil;
	}

	if(strcmp(elem, ".") == 0)
		return vacfileincref(f);

	if(strcmp(elem, "..") == 0)
		return vacfilegetparent(f);

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

	if(vtfilelock(f->source, f->mode) < 0)
		goto Err;
	if(ff->dir.mode & ModeDir){
		ff->source = fileopensource(f, ff->dir.entry, ff->dir.gen, 1, ff->mode);
		ff->msource = fileopensource(f, ff->dir.mentry, ff->dir.mgen, 0, ff->mode);
		if(ff->source == nil || ff->msource == nil)
			goto Err1;
	}else{
		ff->source = fileopensource(f, ff->dir.entry, ff->dir.gen, 0, ff->mode);
		if(ff->source == nil)
			goto Err1;
	}
	vtfileunlock(f->source);

	/* link in and up parent ref count */
	ff->next = f->down;
	f->down = ff;
	ff->up = f;
	vacfileincref(f);
Exit:
	fileunlock(f);
	return ff;

Err1:
	vtfileunlock(f->source);
Err:
	fileunlock(f);
	if(ff != nil)
		vacfiledecref(ff);
	return nil;
}

/*
 * Open a path in the vac file system:
 * just walk each element one at a time.
 */
VacFile*
vacfileopen(VacFs *fs, char *path)
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
			ff = vacfilewalk(f, elem);
			if(ff == nil){
				werrstr("%.*s: %r", utfnlen(opath, p-opath), opath);
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

/*
 * Extract the score for the bn'th block in f.
 */
int
vacfileblockscore(VacFile *f, u32int bn, u8int *score)
{
	VtFile *s;
	uvlong size;
	int dsize, ret;

	ret = -1;
	if(filerlock(f) < 0)
		return -1;
	if(vtfilelock(f->source, VtOREAD) < 0)
		goto out;

	s = f->source;
	dsize = s->dsize;
	size = vtfilegetsize(s);
	if((uvlong)bn*dsize >= size)
		goto out1;
	ret = vtfileblockscore(f->source, bn, score);

out1:
	vtfileunlock(f->source);
out:
	filerunlock(f);
	return ret;
}

/*
 * Read data from f.
 */
int
vacfileread(VacFile *f, void *buf, int cnt, vlong offset)
{
	int n;

	if(offset < 0){
		werrstr(EBadOffset);
		return -1;
	}
	if(filerlock(f) < 0)
		return -1;
	if(vtfilelock(f->source, VtOREAD) < 0){
		filerunlock(f);
		return -1;
	}
	n = vtfileread(f->source, buf, cnt, offset);
	vtfileunlock(f->source);
	filerunlock(f);
	return n;
}

static int
getentry(VtFile *f, VtEntry *e)
{
	if(vtfilelock(f, VtOREAD) < 0)
		return -1;
	if(vtfilegetentry(f, e) < 0){
		vtfileunlock(f);
		return -1;
	}
	vtfileunlock(f);
	if(vtglobaltolocal(e->score) != NilBlock){
		werrstr("internal error - data not on venti");
		return -1;
	}
	return 0;
}

/*
 * Get the VtEntries for the data contained in f.
 */
int
vacfilegetentries(VacFile *f, VtEntry *e, VtEntry *me)
{
	if(filerlock(f) < 0)
		return -1;
	if(e && getentry(f->source, e) < 0){
		filerunlock(f);
		return -1;
	}
	if(me){
		if(f->msource == nil)
			memset(me, 0, sizeof *me);
		else if(getentry(f->msource, me) < 0){
			filerunlock(f);
			return -1;
		}
	}
	filerunlock(f);
	return 0;
}

/*
 * Get the file's size.
 */
int
vacfilegetsize(VacFile *f, uvlong *size)
{
	if(filerlock(f) < 0)
		return -1;
	if(vtfilelock(f->source, VtOREAD) < 0){
		filerunlock(f);
		return -1;
	}
	*size = vtfilegetsize(f->source);
	vtfileunlock(f->source);
	filerunlock(f);

	return 0;
}

/*
 * Directory reading.
 *
 * A VacDirEnum is a buffer containing directory entries.
 * Directory entries contain malloced strings and need to
 * be cleaned up with vdcleanup.  The invariant in the
 * VacDirEnum is that the directory entries between
 * vde->i and vde->n are owned by the vde and need to
 * be cleaned up if it is closed.  Those from 0 up to vde->i
 * have been handed to the reader, and the reader must
 * take care of calling vdcleanup as appropriate.
 */
VacDirEnum*
vdeopen(VacFile *f)
{
	VacDirEnum *vde;
	VacFile *p;

	if(!vacfileisdir(f)){
		werrstr(ENotDir);
		return nil;
	}

	/*
	 * There might be changes to this directory's children
	 * that have not been flushed out into the cache yet.
	 * Those changes are only available if we look at the
	 * VacFile structures directory.  But the directory reader
	 * is going to read the cache blocks directly, so update them.
	 */
	if(filelock(f) < 0)
		return nil;
	for(p=f->down; p; p=p->next)
		filemetaflush(p, nil);
	fileunlock(f);

	vde = vtmallocz(sizeof(VacDirEnum));
	vde->file = vacfileincref(f);

	return vde;
}

/*
 * Figure out the size of the directory entry at offset.
 * The rest of the metadata is kept in the data half,
 * but since venti has to track the data size anyway,
 * we just use that one and avoid updating the directory
 * each time the file size changes.
 */
static int
direntrysize(VtFile *s, ulong offset, ulong gen, uvlong *size)
{
	VtBlock *b;
	ulong bn;
	VtEntry e;
	int epb;

	epb = s->dsize/VtEntrySize;
	bn = offset/epb;
	offset -= bn*epb;

	b = vtfileblock(s, bn, VtOREAD);
	if(b == nil)
		goto Err;
	if(vtentryunpack(&e, b->data, offset) < 0)
		goto Err;

	/* dangling entries are returned as zero size */
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

/*
 * Fill in vde with a new batch of directory entries.
 */
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

/*
 * Read a single directory entry from vde into de.
 * Returns -1 on error, 0 on EOF, and 1 on success.
 * When it returns 1, it becomes the caller's responsibility
 * to call vdcleanup(de) to free the strings contained
 * inside, or else to call vdunread to give it back.
 */
int
vderead(VacDirEnum *vde, VacDir *de)
{
	int ret;
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

	while(vde->i >= vde->n){
		if(vde->boff >= nb){
			ret = 0;
			goto Return;
		}
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

	return ret;
}

/*
 * "Unread" the last directory entry that was read,
 * so that the next vderead will return the same one.
 * If the caller calls vdeunread(vde) it should not call
 * vdcleanup on the entry being "unread".
 */
int
vdeunread(VacDirEnum *vde)
{
	if(vde->i > 0){
		vde->i--;
		return 0;
	}
	return -1;
}

/*
 * Close the enumerator.
 */
void
vdeclose(VacDirEnum *vde)
{
	int i;
	if(vde == nil)
		return;
	/* free the strings */
	for(i=vde->i; i<vde->n; i++)
		vdcleanup(vde->buf+i);
	vtfree(vde->buf);
	vacfiledecref(vde->file);
	vtfree(vde);
}


/*
 * On to mutation.  If the vac file system has been opened
 * read-write, then the files and directories can all be edited.
 * Changes are kept in the in-memory cache until flushed out
 * to venti, so we must be careful to explicitly flush data
 * that we're not likely to modify again.
 *
 * Each VacFile has its own copy of its VacDir directory entry
 * in f->dir, but otherwise the cache is the authoratative source
 * for data.  Thus, for the most part, it suffices if we just
 * call vtfileflushbefore and vtfileflush when we modify things.
 * There are a few places where we have to remember to write
 * changed VacDirs back into the cache.  If f->dir *is* out of sync,
 * then f->dirty should be set.
 *
 * The metadata in a directory is, to venti, a plain data file,
 * but as mentioned above it is actually a sequence of
 * MetaBlocks that contain sorted lists of VacDir entries.
 * The filemetaxxx routines manipulate that stream.
 */

/*
 * Find space in fp for the directory entry dir (not yet written to disk)
 * and write it to disk, returning NilBlock on failure,
 * or the block number on success.
 *
 * Start is a suggested block number to try.
 * The caller must have filemetalock'ed f and have
 * vtfilelock'ed f->up->msource.
 */
static u32int
filemetaalloc(VacFile *fp, VacDir *dir, u32int start)
{
	u32int nb, bo;
	VtBlock *b;
	MetaBlock mb;
	int nn;
	uchar *p;
	int i, n;
	MetaEntry me;
	VtFile *ms;

	ms = fp->msource;
	n = vdsize(dir, VacDirVersion);

	/* Look for a block with room for a new entry of size n. */
	nb = (vtfilegetsize(ms)+ms->dsize-1)/ms->dsize;
	if(start == NilBlock){
		if(nb > 0)
			start = nb - 1;
		else
			start = 0;
	}

	if(start > nb)
		start = nb;
	for(bo=start; bo<nb; bo++){
		if((b = vtfileblock(ms, bo, VtOREAD)) == nil)
			goto Err;
		if(mbunpack(&mb, b->data, ms->dsize) < 0)
			goto Err;
		nn = (mb.maxsize*FullPercentage/100) - mb.size + mb.free;
		if(n <= nn && mb.nindex < mb.maxindex){
			/* reopen for writing */
			vtblockput(b);
			if((b = vtfileblock(ms, bo, VtORDWR)) == nil)
				goto Err;
			mbunpack(&mb, b->data, ms->dsize);
			goto Found;
		}
		vtblockput(b);
	}

	/* No block found, extend the file by one metablock. */
	vtfileflushbefore(ms, nb*(uvlong)ms->dsize);
	if((b = vtfileblock(ms, nb, VtORDWR)) == nil)
		goto Err;
	vtfilesetsize(ms, (nb+1)*ms->dsize);
	mbinit(&mb, b->data, ms->dsize, ms->dsize/BytesPerEntry);

Found:
	/* Now we have a block; allocate space to write the entry. */
	p = mballoc(&mb, n);
	if(p == nil){
		/* mballoc might have changed block */
		mbpack(&mb);
		werrstr(EBadMeta);
		goto Err;
	}

	/* Figure out where to put the index entry, and write it. */
	mbsearch(&mb, dir->elem, &i, &me);
	assert(me.p == nil);	/* not already there */
	me.p = p;
	me.size = n;
	vdpack(dir, &me, VacDirVersion);
	mbinsert(&mb, i, &me);
	mbpack(&mb);
	vtblockput(b);
	return bo;

Err:
	vtblockput(b);
	return NilBlock;
}

/*
 * Update f's directory entry in the block cache.
 * We look for the directory entry by name;
 * if we're trying to rename the file, oelem is the old name.
 *
 * Assumes caller has filemetalock'ed f.
 */
static int
filemetaflush(VacFile *f, char *oelem)
{
	int i, n;
	MetaBlock mb;
	MetaEntry me, me2;
	VacFile *fp;
	VtBlock *b;
	u32int bo;

	if(!f->dirty)
		return 0;

	if(oelem == nil)
		oelem = f->dir.elem;

	/*
	 * Locate f's old metadata in the parent's metadata file.
	 * We know which block it was in, but not exactly where
	 * in the block.
	 */
	fp = f->up;
	if(vtfilelock(fp->msource, -1) < 0)
		return -1;
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

	/*
	 * Check whether we can resize the entry and keep it
	 * in this block.
	 */
	n = vdsize(&f->dir, VacDirVersion);
	if(mbresize(&mb, &me, n) >= 0){
		/* Okay, can be done without moving to another block. */

		/* Remove old data */
		mbdelete(&mb, i, &me);

		/* Find new location if renaming */
		if(strcmp(f->dir.elem, oelem) != 0)
			mbsearch(&mb, f->dir.elem, &i, &me2);

		/* Pack new data into new location. */
		vdpack(&f->dir, &me, VacDirVersion);
vdunpack(&f->dir, &me);
		mbinsert(&mb, i, &me);
		mbpack(&mb);

		/* Done */
		vtblockput(b);
		vtfileunlock(fp->msource);
		f->dirty = 0;
		return 0;
	}

	/*
	 * The entry must be moved to another block.
	 * This can only really happen on renames that
	 * make the name very long.
	 */

	/* Allocate a spot in a new block. */
	if((bo = filemetaalloc(fp, &f->dir, f->boff+1)) == NilBlock){
		/* mbresize above might have modified block */
		mbpack(&mb);
		goto Err;
	}
	f->boff = bo;

	/* Now we're committed.  Delete entry in old block. */
	mbdelete(&mb, i, &me);
	mbpack(&mb);
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

/*
 * Remove the directory entry for f.
 */
static int
filemetaremove(VacFile *f)
{
	VtBlock *b;
	MetaBlock mb;
	MetaEntry me;
	int i;
	VacFile *fp;

	b = nil;
	fp = f->up;
	filemetalock(f);

	if(vtfilelock(fp->msource, VtORDWR) < 0)
		goto Err;
	b = vtfileblock(fp->msource, f->boff, VtORDWR);
	if(b == nil)
		goto Err;

	if(mbunpack(&mb, b->data, fp->msource->dsize) < 0)
		goto Err;
	if(mbsearch(&mb, f->dir.elem, &i, &me) < 0)
		goto Err;
	mbdelete(&mb, i, &me);
	mbpack(&mb);
	vtblockput(b);
	vtfileunlock(fp->msource);

	f->removed = 1;
	f->boff = NilBlock;
	f->dirty = 0;

	filemetaunlock(f);
	return 0;

Err:
	vtfileunlock(fp->msource);
	vtblockput(b);
	filemetaunlock(f);
	return -1;
}

/*
 * That was far too much effort for directory entries.
 * Now we can write code that *does* things.
 */

/*
 * Flush all data associated with f out of the cache and onto venti.
 * If recursive is set, flush f's children too.
 * Vacfiledecref knows how to flush source and msource too.
 */
int
vacfileflush(VacFile *f, int recursive)
{
	int ret;
	VacFile **kids, *p;
	int i, nkids;

	if(f->mode == VtOREAD)
		return 0;

	ret = 0;
	filemetalock(f);
	if(filemetaflush(f, nil) < 0)
		ret = -1;
	filemetaunlock(f);

	if(filelock(f) < 0)
		return -1;

	/*
	 * Lock order prevents us from flushing kids while holding
	 * lock, so make a list and then flush without the lock.
	 */
	nkids = 0;
	kids = nil;
	if(recursive){
		nkids = 0;
		for(p=f->down; p; p=p->next)
			nkids++;
		kids = vtmalloc(nkids*sizeof(VacFile*));
		i = 0;
		for(p=f->down; p; p=p->next){
			kids[i++] = p;
			p->ref++;
		}
	}
	if(nkids > 0){
		fileunlock(f);
		for(i=0; i<nkids; i++){
			if(vacfileflush(kids[i], 1) < 0)
				ret = -1;
			vacfiledecref(kids[i]);
		}
		filelock(f);
	}
	free(kids);

	/*
	 * Now we can flush our own data.
	 */
	vtfilelock(f->source, -1);
	if(vtfileflush(f->source) < 0)
		ret = -1;
	vtfileunlock(f->source);
	if(f->msource){
		vtfilelock(f->msource, -1);
		if(vtfileflush(f->msource) < 0)
			ret = -1;
		vtfileunlock(f->msource);
	}
	fileunlock(f);

	return ret;
}

/*
 * Create a new file named elem in fp with the given mode.
 * The mode can be changed later except for the ModeDir bit.
 */
VacFile*
vacfilecreate(VacFile *fp, char *elem, ulong mode)
{
	VacFile *ff;
	VacDir *dir;
	VtFile *pr, *r, *mr;
	int type;
	u32int bo;

	if(filelock(fp) < 0)
		return nil;

	/*
	 * First, look to see that there's not a file in memory
	 * with the same name.
	 */
	for(ff = fp->down; ff; ff=ff->next){
		if(strcmp(elem, ff->dir.elem) == 0 && !ff->removed){
			ff = nil;
			werrstr(EExists);
			goto Err1;
		}
	}

	/*
	 * Next check the venti blocks.
	 */
	ff = dirlookup(fp, elem);
	if(ff != nil){
		werrstr(EExists);
		goto Err1;
	}

	/*
	 * By the way, you can't create in a read-only file system.
	 */
	pr = fp->source;
	if(pr->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err1;
	}

	/*
	 * Okay, time to actually create something.  Lock the two
	 * halves of the directory and create a file.
	 */
	if(vtfilelock2(fp->source, fp->msource, -1) < 0)
		goto Err1;
	ff = filealloc(fp->fs);
	ff->qidoffset = fp->qidoffset;	/* hopefully fp->qidoffset == 0 */
	type = VtDataType;
	if(mode & ModeDir)
		type = VtDirType;
	mr = nil;
	if((r = vtfilecreate(pr, pr->psize, pr->dsize, type)) == nil)
		goto Err;
	if(mode & ModeDir)
	if((mr = vtfilecreate(pr, pr->psize, pr->dsize, VtDataType)) == nil)
		goto Err;

	/*
	 * Fill in the directory entry and write it to disk.
	 */
	dir = &ff->dir;
	dir->elem = vtstrdup(elem);
	dir->entry = r->offset;
	dir->gen = r->gen;
	if(mode & ModeDir){
		dir->mentry = mr->offset;
		dir->mgen = mr->gen;
	}
	dir->size = 0;
	if(_vacfsnextqid(fp->fs, &dir->qid) < 0)
		goto Err;
	dir->uid = vtstrdup(fp->dir.uid);
	dir->gid = vtstrdup(fp->dir.gid);
	dir->mid = vtstrdup("");
	dir->mtime = time(0L);
	dir->mcount = 0;
	dir->ctime = dir->mtime;
	dir->atime = dir->mtime;
	dir->mode = mode;
	if((bo = filemetaalloc(fp, &ff->dir, NilBlock)) == NilBlock)
		goto Err;

	/*
	 * Now we're committed.
	 */
	vtfileunlock(fp->source);
	vtfileunlock(fp->msource);
	ff->source = r;
	ff->msource = mr;
	ff->boff = bo;

	/* Link into tree. */
	ff->next = fp->down;
	fp->down = ff;
	ff->up = fp;
	vacfileincref(fp);

	fileunlock(fp);

	filelock(ff);
	vtfilelock(ff->source, -1);
	vtfileunlock(ff->source);
	fileunlock(ff);

	return ff;

Err:
	vtfileunlock(fp->source);
	vtfileunlock(fp->msource);
	if(r){
		vtfilelock(r, -1);
		vtfileremove(r);
	}
	if(mr){
		vtfilelock(mr, -1);
		vtfileremove(mr);
	}
Err1:
	if(ff)
		vacfiledecref(ff);
	fileunlock(fp);
	return nil;
}

/*
 * Change the size of the file f.
 */
int
vacfilesetsize(VacFile *f, uvlong size)
{
	if(vacfileisdir(f)){
		werrstr(ENotFile);
		return -1;
	}

	if(filelock(f) < 0)
		return -1;

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}
	if(vtfilelock(f->source, -1) < 0)
		goto Err;
	if(vtfilesetsize(f->source, size) < 0){
		vtfileunlock(f->source);
		goto Err;
	}
	vtfileunlock(f->source);
	fileunlock(f);
	return 0;

Err:
	fileunlock(f);
	return -1;
}

/*
 * Write data to f.
 */
int
vacfilewrite(VacFile *f, void *buf, int cnt, vlong offset)
{
	if(vacfileisdir(f)){
		werrstr(ENotFile);
		return -1;
	}
	if(filelock(f) < 0)
		return -1;
	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}
	if(offset < 0){
		werrstr(EBadOffset);
		goto Err;
	}

	if(vtfilelock(f->source, -1) < 0)
		goto Err;
	if(f->dir.mode & ModeAppend)
		offset = vtfilegetsize(f->source);
	if(vtfilewrite(f->source, buf, cnt, offset) != cnt
	|| vtfileflushbefore(f->source, offset) < 0){
		vtfileunlock(f->source);
		goto Err;
	}
	vtfileunlock(f->source);
	fileunlock(f);
	return cnt;

Err:
	fileunlock(f);
	return -1;
}

/*
 * Set (!) the VtEntry for the data contained in f.
 * This let's us efficiently copy data from one file to another.
 */
int
vacfilesetentries(VacFile *f, VtEntry *e, VtEntry *me)
{
	int ret;

	vacfileflush(f, 0);	/* flush blocks to venti, since we won't see them again */

	if(!(e->flags&VtEntryActive)){
		werrstr("missing entry for source");
		return -1;
	}
	if(me && !(me->flags&VtEntryActive))
		me = nil;
	if(f->msource && !me){
		werrstr("missing entry for msource");
		return -1;
	}
	if(me && !f->msource){
		werrstr("no msource to set");
		return -1;
	}

	if(filelock(f) < 0)
		return -1;
	if(f->source->mode != VtORDWR
	|| (f->msource && f->msource->mode != VtORDWR)){
		werrstr(EReadOnly);
		fileunlock(f);
		return -1;
	}
	if(vtfilelock2(f->source, f->msource, -1) < 0){
		fileunlock(f);
		return -1;
	}
	ret = 0;
	if(vtfilesetentry(f->source, e) < 0)
		ret = -1;
	else if(me && vtfilesetentry(f->msource, me) < 0)
		ret = -1;

	vtfileunlock(f->source);
	if(f->msource)
		vtfileunlock(f->msource);
	fileunlock(f);
	return ret;
}

/*
 * Get the directory entry for f.
 */
int
vacfilegetdir(VacFile *f, VacDir *dir)
{
	if(filerlock(f) < 0)
		return -1;

	filemetalock(f);
	vdcopy(dir, &f->dir);
	filemetaunlock(f);

	if(!vacfileisdir(f)){
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

/*
 * Set the directory entry for f.
 */
int
vacfilesetdir(VacFile *f, VacDir *dir)
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
	filemetalock(f);

	if(f->source->mode != VtORDWR){
		werrstr(EReadOnly);
		goto Err;
	}

	/* On rename, check new name does not already exist */
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
		werrstr("");	/* "failed" dirlookup poisoned it */
	}

	/* Get ready... */
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
		}
	}
	/* ... now commited to changing it. */
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

	if(strcmp(f->dir.mid, dir->mid) != 0){
		vtfree(f->dir.mid);
		f->dir.mid = vtstrdup(dir->mid);
	}

	f->dir.mtime = dir->mtime;
	f->dir.atime = dir->atime;

	mask = ~(ModeDir|ModeSnapshot);
	f->dir.mode &= ~mask;
	f->dir.mode |= mask & dir->mode;
	f->dirty = 1;

	if(filemetaflush(f, oelem) < 0){
		vtfree(oelem);
		goto Err;	/* that sucks */
	}
	vtfree(oelem);

	filemetaunlock(f);
	fileunlock(f);
	return 0;

Err:
	filemetaunlock(f);
	fileunlock(f);
	return -1;
}

/*
 * Set the qid space.
 */
int
vacfilesetqidspace(VacFile *f, u64int offset, u64int max)
{
	int ret;

	if(filelock(f) < 0)
		return -1;
	if(f->source->mode != VtORDWR){
		fileunlock(f);
		werrstr(EReadOnly);
		return -1;
	}
	filemetalock(f);
	f->dir.qidspace = 1;
	f->dir.qidoffset = offset;
	f->dir.qidmax = max;
	f->dirty = 1;
	ret = filemetaflush(f, nil);
	filemetaunlock(f);
	fileunlock(f);
	return ret;
}

/*
 * Check that the file is empty, returning 0 if it is.
 * Returns -1 on error (and not being empty is an error).
 */
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
		b = vtfileblock(r, i, VtOREAD);
		if(b == nil)
			return -1;
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

/*
 * Remove the vac file f.
 */
int
vacfileremove(VacFile *f)
{
	VacFile *ff;

	/* Cannot remove the root */
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

	if(filemetaremove(f) < 0)
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

/*
 * Vac file system format.
 */
static char EBadVacFormat[] = "bad format for vac file";

static VacFs *
vacfsalloc(VtConn *z, int bsize, ulong cachemem, int mode)
{
	VacFs *fs;

	fs = vtmallocz(sizeof(VacFs));
	fs->z = z;
	fs->bsize = bsize;
	fs->mode = mode;
	fs->cache = vtcachealloc(z, cachemem);
	return fs;
}

static int
readscore(int fd, uchar score[VtScoreSize])
{
	char buf[45], *pref;
	int n;

	n = readn(fd, buf, sizeof(buf)-1);
	if(n < sizeof(buf)-1) {
		werrstr("short read");
		return -1;
	}
	buf[n] = 0;

	if(vtparsescore(buf, &pref, score) < 0){
		werrstr(EBadVacFormat);
		return -1;
	}
	if(pref==nil || strcmp(pref, "vac") != 0) {
		werrstr("not a vac file");
		return -1;
	}
	return 0;
}

VacFs*
vacfsopen(VtConn *z, char *file, int mode, ulong cachemem)
{
	int fd;
	uchar score[VtScoreSize];
	char *prefix;

	if(vtparsescore(file, &prefix, score) >= 0){
		if(prefix == nil || strcmp(prefix, "vac") != 0){
			werrstr("not a vac file");
			return nil;
		}
	}else{
		fd = open(file, OREAD);
		if(fd < 0)
			return nil;
		if(readscore(fd, score) < 0){
			close(fd);
			return nil;
		}
		close(fd);
	}
if(debug) fprint(2, "vacfsopen %V\n", score);
	return vacfsopenscore(z, score, mode, cachemem);
}

VacFs*
vacfsopenscore(VtConn *z, u8int *score, int mode, ulong cachemem)
{
	VacFs *fs;
	int n;
	VtRoot rt;
	uchar buf[VtRootSize];
	VacFile *root;
	VtFile *r;
	VtEntry e;

	n = vtread(z, score, VtRootType, buf, VtRootSize);
	if(n < 0) {
if(debug) fprint(2, "read %r\n");
		return nil;
	}
	if(n != VtRootSize){
		werrstr("vtread on root too short");
if(debug) fprint(2, "size %d\n", n);
		return nil;
	}

	if(vtrootunpack(&rt, buf) < 0) {
if(debug) fprint(2, "unpack: %r\n");
		return nil;
	}

	if(strcmp(rt.type, "vac") != 0) {
if(debug) fprint(2, "bad type %s\n", rt.type);
		werrstr("not a vac root");
		return nil;
	}

	fs = vacfsalloc(z, rt.blocksize, cachemem, mode);
	memmove(fs->score, score, VtScoreSize);
	fs->mode = mode;

	memmove(e.score, rt.score, VtScoreSize);
	e.gen = 0;

	// Don't waste cache memory on pointer blocks
	// when rt.blocksize is large.
	e.psize = (rt.blocksize/VtEntrySize)*VtEntrySize;
	if(e.psize > 60000)
		e.psize = (60000/VtEntrySize)*VtEntrySize;

	e.dsize = rt.blocksize;
if(debug) fprint(2, "openscore %d psize %d dsize %d\n", (int)rt.blocksize, (int)e.psize, (int)e.dsize);
	e.type = VtDirType;
	e.flags = VtEntryActive;
	e.size = 3*VtEntrySize;

	root = nil;
	if((r = vtfileopenroot(fs->cache, &e)) == nil)
		goto Err;
	if(debug)
		fprint(2, "r %p\n", r);
	root = _vacfileroot(fs, r);
	if(debug)
		fprint(2, "root %p\n", root);
	vtfileclose(r);
	if(root == nil)
		goto Err;
	fs->root = root;
	return fs;
Err:
	if(root)
		vacfiledecref(root);
	vacfsclose(fs);
	return nil;
}

int
vacfsmode(VacFs *fs)
{
	return fs->mode;
}

VacFile*
vacfsgetroot(VacFs *fs)
{
	return vacfileincref(fs->root);
}

int
vacfsgetblocksize(VacFs *fs)
{
	return fs->bsize;
}

int
vacfsgetscore(VacFs *fs, u8int *score)
{
	memmove(score, fs->score, VtScoreSize);
	return 0;
}

int
_vacfsnextqid(VacFs *fs, uvlong *qid)
{
	++fs->qid;
	*qid = fs->qid;
	return 0;
}

void
vacfsjumpqid(VacFs *fs, uvlong step)
{
	fs->qid += step;
}

/*
 * Set *maxqid to the maximum qid expected in this file system.
 * In newer vac archives, the maximum qid is stored in the
 * qidspace VacDir annotation.  In older vac archives, the root
 * got created last, so it had the maximum qid.
 */
int
vacfsgetmaxqid(VacFs *fs, uvlong *maxqid)
{
	VacDir vd;

	if(vacfilegetdir(fs->root, &vd) < 0)
		return -1;
	if(vd.qidspace)
		*maxqid = vd.qidmax;
	else
		*maxqid = vd.qid;
	vdcleanup(&vd);
	return 0;
}


void
vacfsclose(VacFs *fs)
{
	if(fs->root)
		vacfiledecref(fs->root);
	fs->root = nil;
	vtcachefree(fs->cache);
	vtfree(fs);
}

/*
 * Create a fresh vac fs.
 */
VacFs *
vacfscreate(VtConn *z, int bsize, ulong cachemem)
{
	VacFs *fs;
	VtFile *f;
	uchar buf[VtEntrySize], metascore[VtScoreSize];
	VtEntry e;
	VtBlock *b;
	MetaBlock mb;
	VacDir vd;
	MetaEntry me;
	int psize;

	if((fs = vacfsalloc(z, bsize, cachemem, VtORDWR)) == nil)
		return nil;

	/*
	 * Fake up an empty vac fs.
	 */
	psize = bsize/VtScoreSize*VtScoreSize;
	if(psize > 60000)
		psize = 60000/VtScoreSize*VtScoreSize;
if(debug) fprint(2, "create bsize %d psize %d\n", bsize, psize);

	f = vtfilecreateroot(fs->cache, psize, bsize, VtDirType);
	if(f == nil)
		sysfatal("vtfilecreateroot: %r");
	vtfilelock(f, VtORDWR);

	/* Write metablock containing root directory VacDir. */
	b = vtcacheallocblock(fs->cache, VtDataType, bsize);
	mbinit(&mb, b->data, bsize, bsize/BytesPerEntry);
	memset(&vd, 0, sizeof vd);
	vd.elem = "/";
	vd.mode = 0777|ModeDir;
	vd.uid = "vac";
	vd.gid = "vac";
	vd.mid = "";
	me.size = vdsize(&vd, VacDirVersion);
	me.p = mballoc(&mb, me.size);
	vdpack(&vd, &me, VacDirVersion);
	mbinsert(&mb, 0, &me);
	mbpack(&mb);
	vtblockwrite(b);
	memmove(metascore, b->score, VtScoreSize);
	vtblockput(b);

	/* First entry: empty venti directory stream. */
	memset(&e, 0, sizeof e);
	e.flags = VtEntryActive;
	e.psize = psize;
	e.dsize = bsize;
	e.type = VtDirType;
	memmove(e.score, vtzeroscore, VtScoreSize);
	vtentrypack(&e, buf, 0);
	vtfilewrite(f, buf, VtEntrySize, 0);

	/* Second entry: empty metadata stream. */
	e.type = VtDataType;
	vtentrypack(&e, buf, 0);
	vtfilewrite(f, buf, VtEntrySize, VtEntrySize);

	/* Third entry: metadata stream with root directory. */
	memmove(e.score, metascore, VtScoreSize);
	e.size = bsize;
	vtentrypack(&e, buf, 0);
	vtfilewrite(f, buf, VtEntrySize, VtEntrySize*2);

	vtfileflush(f);
	vtfileunlock(f);

	/* Now open it as a vac fs. */
	fs->root = _vacfileroot(fs, f);
	if(fs->root == nil){
		werrstr("vacfileroot: %r");
		vacfsclose(fs);
		return nil;
	}

	return fs;
}

int
vacfssync(VacFs *fs)
{
	uchar buf[1024];
	VtEntry e;
	VtFile *f;
	VtRoot root;

	/* Sync the entire vacfs to disk. */
	if(vacfileflush(fs->root, 1) < 0)
		return -1;
	if(vtfilelock(fs->root->up->msource, -1) < 0)
		return -1;
	if(vtfileflush(fs->root->up->msource) < 0){
		vtfileunlock(fs->root->up->msource);
		return -1;
	}
	vtfileunlock(fs->root->up->msource);

	/* Prepare the dir stream for the root block. */
	if(getentry(fs->root->source, &e) < 0)
		return -1;
	vtentrypack(&e, buf, 0);
	if(getentry(fs->root->msource, &e) < 0)
		return -1;
	vtentrypack(&e, buf, 1);
	if(getentry(fs->root->up->msource, &e) < 0)
		return -1;
	vtentrypack(&e, buf, 2);

	f = vtfilecreateroot(fs->cache, fs->bsize, fs->bsize, VtDirType);
	vtfilelock(f, VtORDWR);
	if(vtfilewrite(f, buf, 3*VtEntrySize, 0) < 0
	|| vtfileflush(f) < 0){
		vtfileunlock(f);
		vtfileclose(f);
		return -1;
	}
	vtfileunlock(f);
	if(getentry(f, &e) < 0){
		vtfileclose(f);
		return -1;
	}
	vtfileclose(f);

	/* Build a root block. */
	memset(&root, 0, sizeof root);
	strcpy(root.type, "vac");
	strcpy(root.name, fs->name);
	memmove(root.score, e.score, VtScoreSize);
	root.blocksize = fs->bsize;
	memmove(root.prev, fs->score, VtScoreSize);
	vtrootpack(&root, buf);
	if(vtwrite(fs->z, fs->score, VtRootType, buf, VtRootSize) < 0){
		werrstr("writing root: %r");
		return -1;
	}
	if(vtsync(fs->z) < 0)
		return -1;
	return 0;
}

int
vacfiledsize(VacFile *f)
{
	VtEntry e;

	if(vacfilegetentries(f,&e,nil) < 0)
		return -1;
	return e.dsize;
}

/*
 * Does block b of f have the same SHA1 hash as the n bytes at buf?
 */
int
sha1matches(VacFile *f, ulong b, uchar *buf, int n)
{
	uchar fscore[VtScoreSize];
	uchar bufscore[VtScoreSize];

	if(vacfileblockscore(f, b, fscore) < 0)
		return 0;
	n = vtzerotruncate(VtDataType, buf, n);
	sha1(buf, n, bufscore, nil);
	if(memcmp(bufscore, fscore, VtScoreSize) == 0)
		return 1;
	return 0;
}
