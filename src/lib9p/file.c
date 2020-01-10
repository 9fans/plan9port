#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

/*
 * To avoid deadlock, the following rules must be followed.
 * Always lock child then parent, never parent then child.
 * If holding the free file lock, do not lock any Files.
 */
struct Filelist {
	File *f;
	Filelist *link;
};

static QLock filelk;
static File *freefilelist;

static File*
allocfile(void)
{
	int i, a;
	File *f;
	enum { N = 16 };

	qlock(&filelk);
	if(freefilelist == nil){
		f = emalloc9p(N*sizeof(*f));
		for(i=0; i<N-1; i++)
			f[i].aux = &f[i+1];
		f[N-1].aux = nil;
		f[0].allocd = 1;
		freefilelist = f;
	}

	f = freefilelist;
	freefilelist = f->aux;
	qunlock(&filelk);

	a = f->allocd;
	memset(f, 0, sizeof *f);
	f->allocd = a;
	return f;
}

static void
freefile(File *f)
{
	Filelist *fl, *flnext;

	for(fl=f->filelist; fl; fl=flnext){
		flnext = fl->link;
		assert(fl->f == nil);
		free(fl);
	}

	free(f->dir.name);
	free(f->dir.uid);
	free(f->dir.gid);
	free(f->dir.muid);
	qlock(&filelk);
	assert(f->ref.ref == 0);
	f->aux = freefilelist;
	freefilelist = f;
	qunlock(&filelk);
}

void
closefile(File *f)
{
	if(decref(&f->ref) == 0){
		f->tree->destroy(f);
		freefile(f);
	}
}

static void
nop(File *f)
{
	USED(f);
}

int
removefile(File *f)
{
	File *fp;
	Filelist *fl;

	fp = f->parent;
	if(fp == nil){
		werrstr("no parent");
		closefile(f);
		return -1;
	}

	if(fp == f){
		werrstr("cannot remove root");
		closefile(f);
		return -1;
	}

	wlock(&fp->rwlock);
	wlock(&f->rwlock);
	if(f->nchild != 0){
		werrstr("has children");
		wunlock(&f->rwlock);
		wunlock(&fp->rwlock);
		closefile(f);
		return -1;
	}

	if(f->parent != fp){
		werrstr("parent changed underfoot");
		wunlock(&f->rwlock);
		wunlock(&fp->rwlock);
		closefile(f);
		return -1;
	}

	for(fl=fp->filelist; fl; fl=fl->link)
		if(fl->f == f)
			break;
	assert(fl != nil && fl->f == f);

	fl->f = nil;
	fp->nchild--;
	f->parent = nil;
	wunlock(&fp->rwlock);
	wunlock(&f->rwlock);

	closefile(fp);	/* reference from child */
	closefile(f);	/* reference from tree */
	closefile(f);
	return 0;
}

File*
createfile(File *fp, char *name, char *uid, ulong perm, void *aux)
{
	File *f;
	Filelist *fl, *freel;
	Tree *t;

	if((fp->dir.qid.type&QTDIR) == 0){
		werrstr("create in non-directory");
		return nil;
	}

	freel = nil;
	wlock(&fp->rwlock);
	for(fl=fp->filelist; fl; fl=fl->link){
		if(fl->f == nil)
			freel = fl;
		else if(strcmp(fl->f->dir.name, name) == 0){
			wunlock(&fp->rwlock);
			werrstr("file already exists");
			return nil;
		}
	}

	if(freel == nil){
		freel = emalloc9p(sizeof *freel);
		freel->link = fp->filelist;
		fp->filelist = freel;
	}

	f = allocfile();
	f->dir.name = estrdup9p(name);
	f->dir.uid = estrdup9p(uid ? uid : fp->dir.uid);
	f->dir.gid = estrdup9p(fp->dir.gid);
	f->dir.muid = estrdup9p(uid ? uid : "unknown");
	f->aux = aux;
	f->dir.mode = perm;

	t = fp->tree;
	lock(&t->genlock);
	f->dir.qid.path = t->qidgen++;
	unlock(&t->genlock);
	if(perm & DMDIR)
		f->dir.qid.type |= QTDIR;
	if(perm & DMAPPEND)
		f->dir.qid.type |= QTAPPEND;
	if(perm & DMEXCL)
		f->dir.qid.type |= QTEXCL;

	f->dir.mode = perm;
	f->dir.atime = f->dir.mtime = time(0);
	f->dir.length = 0;
	f->parent = fp;
	incref(&fp->ref);
	f->tree = fp->tree;

	incref(&f->ref);	/* being returned */
	incref(&f->ref);	/* for the tree */
	freel->f = f;
	fp->nchild++;
	wunlock(&fp->rwlock);

	return f;
}

static File*
walkfile1(File *dir, char *elem)
{
	File *fp;
	Filelist *fl;

	rlock(&dir->rwlock);
	if(strcmp(elem, "..") == 0){
		fp = dir->parent;
		incref(&fp->ref);
		runlock(&dir->rwlock);
		closefile(dir);
		return fp;
	}

	fp = nil;
	for(fl=dir->filelist; fl; fl=fl->link)
		if(fl->f && strcmp(fl->f->dir.name, elem)==0){
			fp = fl->f;
			incref(&fp->ref);
			break;
		}

	runlock(&dir->rwlock);
	closefile(dir);
	return fp;
}

File*
walkfile(File *f, char *path)
{
	char *os, *s, *nexts;

	if(strchr(path, '/') == nil)
		return walkfile1(f, path);	/* avoid malloc */

	os = s = estrdup9p(path);
	for(; *s; s=nexts){
		if(nexts = strchr(s, '/'))
			*nexts++ = '\0';
		else
			nexts = s+strlen(s);
		f = walkfile1(f, s);
		if(f == nil)
			break;
	}
	free(os);
	return f;
}

static Qid
mkqid(vlong path, long vers, int type)
{
	Qid q;

	q.path = path;
	q.vers = vers;
	q.type = type;
	return q;
}


Tree*
alloctree(char *uid, char *gid, ulong mode, void (*destroy)(File*))
{
	char *muid;
	Tree *t;
	File *f;

	t = emalloc9p(sizeof *t);
	f = allocfile();
	f->dir.name = estrdup9p("/");
	if(uid == nil){
		if(uid = getuser())
			uid = estrdup9p(uid);
	}
	if(uid == nil)
		uid = estrdup9p("none");
	else
		uid = estrdup9p(uid);

	if(gid == nil)
		gid = estrdup9p(uid);
	else
		gid = estrdup9p(gid);

	muid = estrdup9p(uid);

	f->dir.qid = mkqid(0, 0, QTDIR);
	f->dir.length = 0;
	f->dir.atime = f->dir.mtime = time(0);
	f->dir.mode = DMDIR | mode;
	f->tree = t;
	f->parent = f;
	f->dir.uid = uid;
	f->dir.gid = gid;
	f->dir.muid = muid;

	incref(&f->ref);
	t->root = f;
	t->qidgen = 0;
	t->dirqidgen = 1;
	if(destroy == nil)
		destroy = nop;
	t->destroy = destroy;

	return t;
}

static void
_freefiles(File *f)
{
	Filelist *fl, *flnext;

	for(fl=f->filelist; fl; fl=flnext){
		flnext = fl->link;
		_freefiles(fl->f);
		free(fl);
	}

	f->tree->destroy(f);
	freefile(f);
}

void
freetree(Tree *t)
{
	_freefiles(t->root);
	free(t);
}

struct Readdir {
	Filelist *fl;
};

Readdir*
opendirfile(File *dir)
{
	Readdir *r;

	rlock(&dir->rwlock);
	if((dir->dir.mode & DMDIR)==0){
		runlock(&dir->rwlock);
		return nil;
	}
	r = emalloc9p(sizeof(*r));

	/*
	 * This reference won't go away while we're using it
	 * since we are dir->rdir.
	 */
	r->fl = dir->filelist;
	runlock(&dir->rwlock);
	return r;
}

long
readdirfile(Readdir *r, uchar *buf, long n)
{
	long x, m;
	Filelist *fl;

	for(fl=r->fl, m=0; fl && m+2<=n; fl=fl->link, m+=x){
		if(fl->f == nil)
			x = 0;
		else if((x=convD2M(&fl->f->dir, buf+m, n-m)) <= BIT16SZ)
			break;
	}
	r->fl = fl;
	return m;
}

void
closedirfile(Readdir *r)
{
	free(r);
}
