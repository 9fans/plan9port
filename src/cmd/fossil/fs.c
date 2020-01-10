#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

static void fsMetaFlush(void *a);
static Snap *snapInit(Fs*);
static void snapClose(Snap*);

Fs *
fsOpen(char *file, VtConn *z, long ncache, int mode)
{
	int fd, m;
	uchar oscore[VtScoreSize];
	Block *b, *bs;
	Disk *disk;
	Fs *fs;
	Super super;
	char e[ERRMAX];

	switch(mode){
	default:
		werrstr(EBadMode);
		return nil;
	case OReadOnly:
		m = OREAD;
		break;
	case OReadWrite:
		m = ORDWR;
		break;
	}
	fd = open(file, m);
	if(fd < 0){
		werrstr("open %s: %r", file);
		return nil;
	}

	bwatchInit();
	disk = diskAlloc(fd);
	if(disk == nil){
		werrstr("diskAlloc: %r");
		close(fd);
		return nil;
	}

	fs = vtmallocz(sizeof(Fs));
	fs->mode = mode;
	fs->name = vtstrdup(file);
	fs->blockSize = diskBlockSize(disk);
	fs->cache = cacheAlloc(disk, z, ncache, mode);
	if(mode == OReadWrite && z)
		fs->arch = archInit(fs->cache, disk, fs, z);
	fs->z = z;

	b = cacheLocal(fs->cache, PartSuper, 0, mode);
	if(b == nil)
		goto Err;
	if(!superUnpack(&super, b->data)){
		blockPut(b);
		werrstr("bad super block");
		goto Err;
	}
	blockPut(b);

	fs->ehi = super.epochHigh;
	fs->elo = super.epochLow;

//fprint(2, "%s: fs->ehi %d fs->elo %d active=%d\n", argv0, fs->ehi, fs->elo, super.active);

	fs->source = sourceRoot(fs, super.active, mode);
	if(fs->source == nil){
		/*
		 * Perhaps it failed because the block is copy-on-write.
		 * Do the copy and try again.
		 */
		rerrstr(e, sizeof e);
		if(mode == OReadOnly || strcmp(e, EBadRoot) != 0)
			goto Err;
		b = cacheLocalData(fs->cache, super.active, BtDir, RootTag,
			OReadWrite, 0);
		if(b == nil){
			werrstr("cacheLocalData: %r");
			goto Err;
		}
		if(b->l.epoch == fs->ehi){
			blockPut(b);
			werrstr("bad root source block");
			goto Err;
		}
		b = blockCopy(b, RootTag, fs->ehi, fs->elo);
		if(b == nil)
			goto Err;
		localToGlobal(super.active, oscore);
		super.active = b->addr;
		bs = cacheLocal(fs->cache, PartSuper, 0, OReadWrite);
		if(bs == nil){
			blockPut(b);
			werrstr("cacheLocal: %r");
			goto Err;
		}
		superPack(&super, bs->data);
		blockDependency(bs, b, 0, oscore, nil);
		blockPut(b);
		blockDirty(bs);
		blockRemoveLink(bs, globalToLocal(oscore), BtDir, RootTag, 0);
		blockPut(bs);
		fs->source = sourceRoot(fs, super.active, mode);
		if(fs->source == nil){
			werrstr("sourceRoot: %r");
			goto Err;
		}
	}

//fprint(2, "%s: got fs source\n", argv0);

	rlock(&fs->elk);
	fs->file = fileRoot(fs->source);
	fs->source->file = fs->file;		/* point back */
	runlock(&fs->elk);
	if(fs->file == nil){
		werrstr("fileRoot: %r");
		goto Err;
	}

//fprint(2, "%s: got file root\n", argv0);

	if(mode == OReadWrite){
		fs->metaFlush = periodicAlloc(fsMetaFlush, fs, 1000);
		fs->snap = snapInit(fs);
	}
	return fs;

Err:
fprint(2, "%s: fsOpen error\n", argv0);
	fsClose(fs);
	return nil;
}

void
fsClose(Fs *fs)
{
	rlock(&fs->elk);
	periodicKill(fs->metaFlush);
	snapClose(fs->snap);
	if(fs->file){
		fileMetaFlush(fs->file, 0);
		if(!fileDecRef(fs->file))
			sysfatal("fsClose: files still in use: %r");
	}
	fs->file = nil;
	sourceClose(fs->source);
	cacheFree(fs->cache);
	if(fs->arch)
		archFree(fs->arch);
	vtfree(fs->name);
	runlock(&fs->elk);
	memset(fs, ~0, sizeof(Fs));
	vtfree(fs);
}

int
fsRedial(Fs *fs, char *host)
{
	if(vtredial(fs->z, host) < 0)
		return 0;
	if(vtconnect(fs->z) < 0)
		return 0;
	return 1;
}

File *
fsGetRoot(Fs *fs)
{
	return fileIncRef(fs->file);
}

int
fsGetBlockSize(Fs *fs)
{
	return fs->blockSize;
}

Block*
superGet(Cache *c, Super* super)
{
	Block *b;

	if((b = cacheLocal(c, PartSuper, 0, OReadWrite)) == nil){
		fprint(2, "%s: superGet: cacheLocal failed: %r\n", argv0);
		return nil;
	}
	if(!superUnpack(super, b->data)){
		fprint(2, "%s: superGet: superUnpack failed: %r\n", argv0);
		blockPut(b);
		return nil;
	}

	return b;
}

void
superWrite(Block* b, Super* super, int forceWrite)
{
	superPack(super, b->data);
	blockDirty(b);
	if(forceWrite){
		while(!blockWrite(b, Waitlock)){
			/* this should no longer happen */
			fprint(2, "%s: could not write super block; "
				"waiting 10 seconds\n", argv0);
			sleep(10*1000);
		}
		while(b->iostate != BioClean && b->iostate != BioDirty){
			assert(b->iostate == BioWriting);
			rsleep(&b->ioready);
		}
		/*
		 * it's okay that b might still be dirty.
		 * that means it got written out but with an old root pointer,
		 * but the other fields went out, and those are the ones
		 * we really care about.  (specifically, epochHigh; see fsSnapshot).
		 */
	}
}

/*
 * Prepare the directory to store a snapshot.
 * Temporary snapshots go into /snapshot/yyyy/mmdd/hhmm[.#]
 * Archival snapshots go into /archive/yyyy/mmdd[.#].
 *
 * TODO This should be rewritten to eliminate most of the duplication.
 */
static File*
fileOpenSnapshot(Fs *fs, char *dstpath, int doarchive)
{
	int n;
	char buf[30], *s, *p, *elem;
	File *dir, *f;
	Tm now;

	if(dstpath){
		if((p = strrchr(dstpath, '/')) != nil){
			*p++ = '\0';
			elem = p;
			p = dstpath;
			if(*p == '\0')
				p = "/";
		}else{
			p = "/";
			elem = dstpath;
		}
		if((dir = fileOpen(fs, p)) == nil)
			return nil;
		f = fileCreate(dir, elem, ModeDir|ModeSnapshot|0555, "adm");
		fileDecRef(dir);
		return f;
	}else if(doarchive){
		/*
		 * a snapshot intended to be archived to venti.
		 */
		dir = fileOpen(fs, "/archive");
		if(dir == nil)
			return nil;
		now = *localtime(time(0));

		/* yyyy */
		snprint(buf, sizeof(buf), "%d", now.year+1900);
		f = fileWalk(dir, buf);
		if(f == nil)
			f = fileCreate(dir, buf, ModeDir|0555, "adm");
		fileDecRef(dir);
		if(f == nil)
			return nil;
		dir = f;

		/* mmdd[#] */
		snprint(buf, sizeof(buf), "%02d%02d", now.mon+1, now.mday);
		s = buf+strlen(buf);
		for(n=0;; n++){
			if(n)
				seprint(s, buf+sizeof(buf), ".%d", n);
			f = fileWalk(dir, buf);
			if(f != nil){
				fileDecRef(f);
				continue;
			}
			f = fileCreate(dir, buf, ModeDir|ModeSnapshot|0555, "adm");
			break;
		}
		fileDecRef(dir);
		return f;
	}else{
		/*
		 * Just a temporary snapshot
		 * We'll use /snapshot/yyyy/mmdd/hhmm.
		 * There may well be a better naming scheme.
		 * (I'd have used hh:mm but ':' is reserved in Microsoft file systems.)
		 */
		dir = fileOpen(fs, "/snapshot");
		if(dir == nil)
			return nil;

		now = *localtime(time(0));

		/* yyyy */
		snprint(buf, sizeof(buf), "%d", now.year+1900);
		f = fileWalk(dir, buf);
		if(f == nil)
			f = fileCreate(dir, buf, ModeDir|0555, "adm");
		fileDecRef(dir);
		if(f == nil)
			return nil;
		dir = f;

		/* mmdd */
		snprint(buf, sizeof(buf), "%02d%02d", now.mon+1, now.mday);
		f = fileWalk(dir, buf);
		if(f == nil)
			f = fileCreate(dir, buf, ModeDir|0555, "adm");
		fileDecRef(dir);
		if(f == nil)
			return nil;
		dir = f;

		/* hhmm[.#] */
		snprint(buf, sizeof buf, "%02d%02d", now.hour, now.min);
		s = buf+strlen(buf);
		for(n=0;; n++){
			if(n)
				seprint(s, buf+sizeof(buf), ".%d", n);
			f = fileWalk(dir, buf);
			if(f != nil){
				fileDecRef(f);
				continue;
			}
			f = fileCreate(dir, buf, ModeDir|ModeSnapshot|0555, "adm");
			break;
		}
		fileDecRef(dir);
		return f;
	}
}

static int
fsNeedArch(Fs *fs, uint archMinute)
{
	int need;
	File *f;
	char buf[100];
	Tm now;
	ulong then;

	then = time(0);
	now = *localtime(then);

	/* back up to yesterday if necessary */
	if(now.hour < archMinute/60
	|| now.hour == archMinute/60 && now.min < archMinute%60)
		now = *localtime(then-86400);

	snprint(buf, sizeof buf, "/archive/%d/%02d%02d",
		now.year+1900, now.mon+1, now.mday);
	need = 1;
	rlock(&fs->elk);
	f = fileOpen(fs, buf);
	if(f){
		need = 0;
		fileDecRef(f);
	}
	runlock(&fs->elk);
	return need;
}

int
fsEpochLow(Fs *fs, u32int low)
{
	Block *bs;
	Super super;

	wlock(&fs->elk);
	if(low > fs->ehi){
		werrstr("bad low epoch (must be <= %ud)", fs->ehi);
		wunlock(&fs->elk);
		return 0;
	}

	if((bs = superGet(fs->cache, &super)) == nil){
		wunlock(&fs->elk);
		return 0;
	}

	super.epochLow = low;
	fs->elo = low;
	superWrite(bs, &super, 1);
	blockPut(bs);
	wunlock(&fs->elk);

	return 1;
}

static int
bumpEpoch(Fs *fs, int doarchive)
{
	uchar oscore[VtScoreSize];
	u32int oldaddr;
	Block *b, *bs;
	Entry e;
	Source *r;
	Super super;

	/*
	 * Duplicate the root block.
	 *
	 * As a hint to flchk, the garbage collector,
	 * and any (human) debuggers, store a pointer
	 * to the old root block in entry 1 of the new root block.
	 */
	r = fs->source;
	b = cacheGlobal(fs->cache, r->score, BtDir, RootTag, OReadOnly);
	if(b == nil)
		return 0;

	memset(&e, 0, sizeof e);
	e.flags = VtEntryActive | VtEntryLocal | _VtEntryDir;
	memmove(e.score, b->score, VtScoreSize);
	e.tag = RootTag;
	e.snap = b->l.epoch;

	b = blockCopy(b, RootTag, fs->ehi+1, fs->elo);
	if(b == nil){
		fprint(2, "%s: bumpEpoch: blockCopy: %r\n", argv0);
		return 0;
	}

	if(0) fprint(2, "%s: snapshot root from %d to %d\n", argv0, oldaddr, b->addr);
	entryPack(&e, b->data, 1);
	blockDirty(b);

	/*
	 * Update the superblock with the new root and epoch.
	 */
	if((bs = superGet(fs->cache, &super)) == nil)
		return 0;

	fs->ehi++;
	memmove(r->score, b->score, VtScoreSize);
	r->epoch = fs->ehi;

	super.epochHigh = fs->ehi;
	oldaddr = super.active;
	super.active = b->addr;
	if(doarchive)
		super.next = oldaddr;

	/*
	 * Record that the new super.active can't get written out until
	 * the new b gets written out.  Until then, use the old value.
	 */
	localToGlobal(oldaddr, oscore);
	blockDependency(bs, b, 0, oscore, nil);
	blockPut(b);

	/*
	 * We force the super block to disk so that super.epochHigh gets updated.
	 * Otherwise, if we crash and come back, we might incorrectly treat as active
	 * some of the blocks that making up the snapshot we just created.
	 * Basically every block in the active file system and all the blocks in
	 * the recently-created snapshot depend on the super block now.
	 * Rather than record all those dependencies, we just force the block to disk.
	 *
	 * Note that blockWrite might actually (will probably) send a slightly outdated
	 * super.active to disk.  It will be the address of the most recent root that has
	 * gone to disk.
	 */
	superWrite(bs, &super, 1);
	blockRemoveLink(bs, globalToLocal(oscore), BtDir, RootTag, 0);
	blockPut(bs);

	return 1;
}

int
saveQid(Fs *fs)
{
	Block *b;
	Super super;
	u64int qidMax;

	if((b = superGet(fs->cache, &super)) == nil)
		return 0;
	qidMax = super.qid;
	blockPut(b);

	if(!fileSetQidSpace(fs->file, 0, qidMax))
		return 0;

	return 1;
}

int
fsSnapshot(Fs *fs, char *srcpath, char *dstpath, int doarchive)
{
	File *src, *dst;

	assert(fs->mode == OReadWrite);

	dst = nil;

	if(fs->halted){
		werrstr("file system is halted");
		return 0;
	}

	/*
	 * Freeze file system activity.
	 */
	wlock(&fs->elk);

	/*
	 * Get the root of the directory we're going to save.
	 */
	if(srcpath == nil)
		srcpath = "/active";
	src = fileOpen(fs, srcpath);
	if(src == nil)
		goto Err;

	/*
	 * It is important that we maintain the invariant that:
	 *	if both b and bb are marked as Active with start epoch e
	 *	and b points at bb, then no other pointers to bb exist.
	 *
	 * When bb is unlinked from b, its close epoch is set to b's epoch.
	 * A block with epoch == close epoch is
	 * treated as free by cacheAllocBlock; this aggressively
	 * reclaims blocks after they have been stored to Venti.
	 *
	 * Let's say src->source is block sb, and src->msource is block
	 * mb.  Let's also say that block b holds the Entry structures for
	 * both src->source and src->msource (their Entry structures might
	 * be in different blocks, but the argument is the same).
	 * That is, right now we have:
	 *
	 *	b	Active w/ epoch e, holds ptrs to sb and mb.
	 *	sb	Active w/ epoch e.
	 *	mb	Active w/ epoch e.
	 *
	 * With things as they are now, the invariant requires that
	 * b holds the only pointers to sb and mb.  We want to record
	 * pointers to sb and mb in new Entries corresponding to dst,
	 * which breaks the invariant.  Thus we need to do something
	 * about b.  Specifically, we bump the file system's epoch and
	 * then rewalk the path from the root down to and including b.
	 * This will copy-on-write as we walk, so now the state will be:
	 *
	 *	b	Snap w/ epoch e, holds ptrs to sb and mb.
	 *	new-b	Active w/ epoch e+1, holds ptrs to sb and mb.
	 *	sb	Active w/ epoch e.
	 *	mb	Active w/ epoch e.
	 *
	 * In this state, it's perfectly okay to make more pointers to sb and mb.
	 */
	if(!bumpEpoch(fs, 0) || !fileWalkSources(src))
		goto Err;

	/*
	 * Sync to disk.  I'm not sure this is necessary, but better safe than sorry.
	 */
	cacheFlush(fs->cache, 1);

	/*
	 * Create the directory where we will store the copy of src.
	 */
	dst = fileOpenSnapshot(fs, dstpath, doarchive);
	if(dst == nil)
		goto Err;

	/*
	 * Actually make the copy by setting dst's source and msource
	 * to be src's.
	 */
	if(!fileSnapshot(dst, src, fs->ehi-1, doarchive))
		goto Err;

	fileDecRef(src);
	fileDecRef(dst);
	src = nil;
	dst = nil;

	/*
	 * Make another copy of the file system.  This one is for the
	 * archiver, so that the file system we archive has the recently
	 * added snapshot both in /active and in /archive/yyyy/mmdd[.#].
	 */
	if(doarchive){
		if(!saveQid(fs))
			goto Err;
		if(!bumpEpoch(fs, 1))
			goto Err;
	}

	wunlock(&fs->elk);

	/* BUG? can fs->arch fall out from under us here? */
	if(doarchive && fs->arch)
		archKick(fs->arch);

	return 1;

Err:
	fprint(2, "%s: fsSnapshot: %r\n", argv0);
	if(src)
		fileDecRef(src);
	if(dst)
		fileDecRef(dst);
	wunlock(&fs->elk);
	return 0;
}

int
fsVac(Fs *fs, char *name, uchar score[VtScoreSize])
{
	int r;
	DirEntry de;
	Entry e, ee;
	File *f;

	rlock(&fs->elk);
	f = fileOpen(fs, name);
	if(f == nil){
		runlock(&fs->elk);
		return 0;
	}

	if(!fileGetSources(f, &e, &ee) || !fileGetDir(f, &de)){
		fileDecRef(f);
		runlock(&fs->elk);
		return 0;
	}
	fileDecRef(f);

	r = mkVac(fs->z, fs->blockSize, &e, &ee, &de, score);
	runlock(&fs->elk);
	return r;
}

static int
vtWriteBlock(VtConn *z, uchar *buf, uint n, uint type, uchar score[VtScoreSize])
{
	if(vtwrite(z, score, type, buf, n) < 0)
		return 0;
	if(vtsha1check(score, buf, n) < 0)
		return 0;
	return 1;
}

int
mkVac(VtConn *z, uint blockSize, Entry *pe, Entry *pee, DirEntry *pde, uchar score[VtScoreSize])
{
	uchar buf[8192];
	int i;
	uchar *p;
	uint n;
	DirEntry de;
	Entry e, ee, eee;
	MetaBlock mb;
	MetaEntry me;
	VtRoot root;

	e = *pe;
	ee = *pee;
	de = *pde;

	if(globalToLocal(e.score) != NilBlock
	|| (ee.flags&VtEntryActive && globalToLocal(ee.score) != NilBlock)){
		werrstr("can only vac paths already stored on venti");
		return 0;
	}

	/*
	 * Build metadata source for root.
	 */
	n = deSize(&de);
	if(n+MetaHeaderSize+MetaIndexSize > sizeof buf){
		werrstr("DirEntry too big");
		return 0;
	}
	memset(buf, 0, sizeof buf);
	mbInit(&mb, buf, n+MetaHeaderSize+MetaIndexSize, 1);
	p = mbAlloc(&mb, n);
	if(p == nil)
		abort();
	mbSearch(&mb, de.elem, &i, &me);
	assert(me.p == nil);
	me.p = p;
	me.size = n;
	dePack(&de, &me);
	mbInsert(&mb, i, &me);
	mbPack(&mb);

	eee.size = n+MetaHeaderSize+MetaIndexSize;
	if(!vtWriteBlock(z, buf, eee.size, VtDataType, eee.score))
		return 0;
	eee.psize = 8192;
	eee.dsize = 8192;
	eee.depth = 0;
	eee.flags = VtEntryActive;

	/*
	 * Build root source with three entries in it.
	 */
	entryPack(&e, buf, 0);
	entryPack(&ee, buf, 1);
	entryPack(&eee, buf, 2);

	n = VtEntrySize*3;
	memset(&root, 0, sizeof root);
	if(!vtWriteBlock(z, buf, n, VtDirType, root.score))
		return 0;

	/*
	 * Save root.
	 */
	strecpy(root.type, root.type+sizeof root.type, "vac");
	strecpy(root.name, root.name+sizeof root.name, de.elem);
	root.blocksize = blockSize;
	vtrootpack(&root, buf);
	if(!vtWriteBlock(z, buf, VtRootSize, VtRootType, score))
		return 0;

	return 1;
}

int
fsSync(Fs *fs)
{
	wlock(&fs->elk);
	fileMetaFlush(fs->file, 1);
	cacheFlush(fs->cache, 1);
	wunlock(&fs->elk);
	return 1;
}

int
fsHalt(Fs *fs)
{
	wlock(&fs->elk);
	fs->halted = 1;
	fileMetaFlush(fs->file, 1);
	cacheFlush(fs->cache, 1);
	return 1;
}

int
fsUnhalt(Fs *fs)
{
	if(!fs->halted)
		return 0;
	fs->halted = 0;
	wunlock(&fs->elk);
	return 1;
}

int
fsNextQid(Fs *fs, u64int *qid)
{
	Block *b;
	Super super;

	if((b = superGet(fs->cache, &super)) == nil)
		return 0;

	*qid = super.qid++;

	/*
	 * It's okay if the super block doesn't go to disk immediately,
	 * since fileMetaAlloc will record a dependency between the
	 * block holding this qid and the super block.  See file.c:/^fileMetaAlloc.
	 */
	superWrite(b, &super, 0);
	blockPut(b);
	return 1;
}

static void
fsMetaFlush(void *a)
{
	int rv;
	Fs *fs = a;

	rlock(&fs->elk);
	rv = fileMetaFlush(fs->file, 1);
	runlock(&fs->elk);
	if(rv > 0)
		cacheFlush(fs->cache, 0);
}

static int
fsEsearch1(File *f, char *path, u32int savetime, u32int *plo)
{
	int n, r;
	DirEntry de;
	DirEntryEnum *dee;
	File *ff;
	Entry e, ee;
	char *t;

	dee = deeOpen(f);
	if(dee == nil)
		return 0;

	n = 0;
	for(;;){
		r = deeRead(dee, &de);
		if(r <= 0)
			break;
		if(de.mode & ModeSnapshot){
			if((ff = fileWalk(f, de.elem)) != nil){
				if(fileGetSources(ff, &e, &ee))
					if(de.mtime >= savetime && e.snap != 0)
						if(e.snap < *plo)
							*plo = e.snap;
				fileDecRef(ff);
			}
		}
		else if(de.mode & ModeDir){
			if((ff = fileWalk(f, de.elem)) != nil){
				t = smprint("%s/%s", path, de.elem);
				n += fsEsearch1(ff, t, savetime, plo);
				vtfree(t);
				fileDecRef(ff);
			}
		}
		deCleanup(&de);
		if(r < 0)
			break;
	}
	deeClose(dee);

	return n;
}

static int
fsEsearch(Fs *fs, char *path, u32int savetime, u32int *plo)
{
	int n;
	File *f;
	DirEntry de;

	f = fileOpen(fs, path);
	if(f == nil)
		return 0;
	if(!fileGetDir(f, &de)){
		fileDecRef(f);
		return 0;
	}
	if((de.mode & ModeDir) == 0){
		fileDecRef(f);
		deCleanup(&de);
		return 0;
	}
	deCleanup(&de);
	n = fsEsearch1(f, path, savetime, plo);
	fileDecRef(f);
	return n;
}

void
fsSnapshotCleanup(Fs *fs, u32int age)
{
	u32int lo;

	/*
	 * Find the best low epoch we can use,
	 * given that we need to save all the unventied archives
	 * and all the snapshots younger than age.
	 */
	rlock(&fs->elk);
	lo = fs->ehi;
	fsEsearch(fs, "/archive", 0, &lo);
	fsEsearch(fs, "/snapshot", time(0)-age*60, &lo);
	runlock(&fs->elk);

	fsEpochLow(fs, lo);
	fsSnapshotRemove(fs);
}

/* remove all snapshots that have expired */
/* return number of directory entries remaining */
static int
fsRsearch1(File *f, char *s)
{
	int n, r;
	DirEntry de;
	DirEntryEnum *dee;
	File *ff;
	char *t, e[ERRMAX];

	dee = deeOpen(f);
	if(dee == nil)
		return 0;

	n = 0;
	for(;;){
		r = deeRead(dee, &de);
		if(r <= 0)
			break;
		n++;
		if(de.mode & ModeSnapshot){
			rerrstr(e, sizeof e);
			if((ff = fileWalk(f, de.elem)) != nil)
				fileDecRef(ff);
			else if(strcmp(e, ESnapOld) == 0){
				if(fileClri(f, de.elem, "adm"))
					n--;
			}
		}
		else if(de.mode & ModeDir){
			if((ff = fileWalk(f, de.elem)) != nil){
				t = smprint("%s/%s", s, de.elem);
				if(fsRsearch1(ff, t) == 0)
					if(fileRemove(ff, "adm"))
						n--;
				vtfree(t);
				fileDecRef(ff);
			}
		}
		deCleanup(&de);
		if(r < 0)
			break;
	}
	deeClose(dee);

	return n;
}

static int
fsRsearch(Fs *fs, char *path)
{
	File *f;
	DirEntry de;

	f = fileOpen(fs, path);
	if(f == nil)
		return 0;
	if(!fileGetDir(f, &de)){
		fileDecRef(f);
		return 0;
	}
	if((de.mode & ModeDir) == 0){
		fileDecRef(f);
		deCleanup(&de);
		return 0;
	}
	deCleanup(&de);
	fsRsearch1(f, path);
	fileDecRef(f);
	return 1;
}

void
fsSnapshotRemove(Fs *fs)
{
	rlock(&fs->elk);
	fsRsearch(fs, "/snapshot");
	runlock(&fs->elk);
}

struct Snap
{
	Fs	*fs;
	Periodic*tick;
	QLock	lk;
	uint	snapMinutes;
	uint	archMinute;
	uint	snapLife;
	u32int	lastSnap;
	u32int	lastArch;
	u32int	lastCleanup;
	uint	ignore;
};

static void
snapEvent(void *v)
{
	Snap *s;
	u32int now, min;
	Tm tm;
	int need;
	u32int snaplife;

	s = v;

	now = time(0)/60;
	qlock(&s->lk);

	/*
	 * Snapshots happen every snapMinutes minutes.
	 * If we miss a snapshot (for example, because we
	 * were down), we wait for the next one.
	 */
	if(s->snapMinutes != ~0 && s->snapMinutes != 0
	&& now%s->snapMinutes==0 && now != s->lastSnap){
		if(!fsSnapshot(s->fs, nil, nil, 0))
			fprint(2, "%s: fsSnapshot snap: %r\n", argv0);
		s->lastSnap = now;
	}

	/*
	 * Archival snapshots happen at archMinute.
	 * If we miss an archive (for example, because we
	 * were down), we do it as soon as possible.
	 */
	tm = *localtime(now*60);
	min = tm.hour*60+tm.min;
	if(s->archMinute != ~0){
		need = 0;
		if(min == s->archMinute && now != s->lastArch)
			need = 1;
		if(s->lastArch == 0){
			s->lastArch = 1;
			if(fsNeedArch(s->fs, s->archMinute))
				need = 1;
		}
		if(need){
			fsSnapshot(s->fs, nil, nil, 1);
			s->lastArch = now;
		}
	}

	/*
	 * Snapshot cleanup happens every snaplife or every day.
	 */
	snaplife = s->snapLife;
	if(snaplife == ~0)
		snaplife = 24*60;
	if(s->lastCleanup+snaplife < now){
		fsSnapshotCleanup(s->fs, s->snapLife);
		s->lastCleanup = now;
	}
	qunlock(&s->lk);
}

static Snap*
snapInit(Fs *fs)
{
	Snap *s;

	s = vtmallocz(sizeof(Snap));
	s->fs = fs;
	s->tick = periodicAlloc(snapEvent, s, 10*1000);
	s->snapMinutes = -1;
	s->archMinute = -1;
	s->snapLife = -1;
	s->ignore = 5*2;	/* wait five minutes for clock to stabilize */
	return s;
}

void
snapGetTimes(Snap *s, u32int *arch, u32int *snap, u32int *snaplen)
{
	if(s == nil){
		*snap = -1;
		*arch = -1;
		*snaplen = -1;
		return;
	}

	qlock(&s->lk);
	*snap = s->snapMinutes;
	*arch = s->archMinute;
	*snaplen = s->snapLife;
	qunlock(&s->lk);
}

void
snapSetTimes(Snap *s, u32int arch, u32int snap, u32int snaplen)
{
	if(s == nil)
		return;

	qlock(&s->lk);
	s->snapMinutes = snap;
	s->archMinute = arch;
	s->snapLife = snaplen;
	qunlock(&s->lk);
}

static void
snapClose(Snap *s)
{
	if(s == nil)
		return;

	periodicKill(s->tick);
	vtfree(s);
}
