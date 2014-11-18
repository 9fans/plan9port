/*
 * 9P to FUSE translator.  Acts as FUSE server, 9P client.
 * Mounts 9P servers via FUSE kernel module.
 *
 * There are four procs in this threaded program
 * (ignoring the one that runs main and then exits).
 * The first proc reads FUSE requests from /dev/fuse.
 * It sends the requests over a channel to a second proc,
 * which serves the requests.  Each request runs in a
 * thread in that second proc.  Those threads do write
 * FUSE replies, which in theory might block, but in practice don't.
 * The 9P interactions are handled by lib9pclient, which
 * allocates two more procs, one for reading and one for
 * writing the 9P connection.  Thus the many threads in the
 * request proc can do 9P interactions without blocking.
 */
 
#define _GNU_SOURCE 1	/* for O_DIRECTORY on Linux */
#include "a.h"

/* GNUisms */
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif

/*
 * Work around glibc's broken <bits/fcntl.h> which defines
 * O_LARGEFILE to 0 on 64 bit architectures.  But, on those same
 * architectures, linux _forces_ O_LARGEFILE (which is always
 * 0100000 in the kernel) at each file open. FUSE is all too
 * happy to pass the flag onto us, where we'd have no idea what
 * to do with it if we trusted glibc.
 *
 * On ARM however, the O_LARGEFILE is set correctly.
 */

#if defined(__linux__) && !defined(__arm__)
#  undef O_LARGEFILE
#  define O_LARGEFILE 0100000
#endif

#ifndef O_CLOEXEC
#  if defined(__linux__)
#    define O_CLOEXEC 02000000  /* Sigh */
#  else
#    define O_CLOEXEC 0
#  endif
#endif

int debug;
char *argv0;
char *aname = "";
void fusedispatch(void*);
Channel *fusechan;

enum
{
	STACK = 8192
};

/*
 * The number of seconds that the kernel can cache
 * returned file attributes.  FUSE's default is 1.0.
 * I haven't experimented with using 0.
 */
double attrtimeout = 1.0;

/*
 * The number of seconds that the kernel can cache
 * the returned entry nodeids returned by lookup.
 * I haven't experimented with other values.
 */
double entrytimeout = 1.0;

CFsys *fsys;
CFid *fsysroot;
void init9p(char*, char*);

void
usage(void)
{
	fprint(2, "usage: 9pfuse [-D] [-A attrtimeout] [-a aname] address mtpt\n");
	exit(1);
}

void fusereader(void*);
void watchfd(void*);

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	case 'D':
		chatty9pclient++;
		debug++;
		break;
	case 'A':
		attrtimeout = atof(EARGF(usage()));
		break;
	case 'a':
		aname = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 2)
		usage();

	quotefmtinstall();
	fmtinstall('F', fcallfmt);
	fmtinstall('M', dirmodefmt);
	fmtinstall('G', fusefmt);

	setsid();	/* won't be able to use console, but can't be interrupted */

	init9p(argv[0], aname);
	initfuse(argv[1]);

	fusechan = chancreate(sizeof(void*), 0);
	proccreate(fusedispatch, nil, STACK);
	sendp(fusechan, nil);	/* sync */

	proccreate(fusereader, nil, STACK);
	/*
	 * Now that we're serving FUSE, we can wait
	 * for the mount to finish and exit back to the user.
	 */
	waitfuse();
	threadexits(0);
}

void
fusereader(void *v)
{
	FuseMsg *m;

	while((m = readfusemsg()) != nil)
		sendp(fusechan, m);

	fusemtpt = nil;	/* no need to unmount */
	threadexitsall(0);
}

void
init9p(char *addr, char *spec)
{
	int fd;

	if(strcmp(addr, "-") == 0)
		fd = 0;
	else
		if((fd = dial(netmkaddr(addr, "tcp", "564"), nil, nil, nil)) < 0)
			sysfatal("dial %s: %r", addr);
	proccreate(watchfd, (void*)(uintptr)fd, STACK);
	if((fsys = fsmount(fd, spec)) == nil)
		sysfatal("fsmount: %r");
	fsysroot = fsroot(fsys);
}

/*
 * FUSE uses nodeids to refer to active "struct inodes"
 * (9P's unopened fids).  FUSE uses fhs to refer to active
 * "struct fuse_files" (9P's opened fids).  The choice of 
 * numbers is up to us except that nodeid 1 is the root directory.
 * We use the same number space for both and call the 
 * bookkeeping structure a FuseFid.
 *
 * FUSE requires nodeids to have associated generation 
 * numbers.  If we reuse a nodeid, we have to bump the 
 * generation number to guarantee that the nodeid,gen
 * combination is never reused.
 * 
 * There are also inode numbers returned in directory reads
 * and file attributes, but these do NOT need to match the nodeids.
 * We use a combination of qid.path and qid.type as the inode
 * number.
 */
/*
 * TO DO: reference count the fids.
 */
typedef struct Fusefid Fusefid;
struct Fusefid
{
	Fusefid *next;
	CFid *fid;
	int ref;
	int id;
	int gen;
	int isnodeid;
	
	/* directory read state */
	Dir *d0;
	Dir *d;
	int nd;
	int off;
};

Fusefid **fusefid;
int nfusefid;
Fusefid *freefusefidlist;

Fusefid*
allocfusefid(void)
{
	Fusefid *f;
	
	if((f = freefusefidlist) == nil){
		f = emalloc(sizeof *f);
		fusefid = erealloc(fusefid, (nfusefid+1)*sizeof *fusefid);
		f->id = nfusefid;
		fusefid[f->id] = f;
		nfusefid++;
	}else
		freefusefidlist = f->next;
	f->next = nil;
	f->ref = 1;
	f->isnodeid = -1;
	return f;
}

void
freefusefid(Fusefid *f)
{
	if(--f->ref > 0)
		return;
	assert(f->ref == 0);
	if(f->fid)
		fsclose(f->fid);
	if(f->d0)
		free(f->d0);
	f->off = 0;
	f->d0 = nil;
	f->fid = nil;
	f->d = nil;
	f->nd = 0;
	f->next = freefusefidlist;
	f->isnodeid = -1;
	freefusefidlist = f;
}

uvlong
_alloc(CFid *fid, int isnodeid)
{
	Fusefid *ff;
	
	ff = allocfusefid();
	ff->fid = fid;
	ff->isnodeid = isnodeid;
	ff->gen++;
	return ff->id+2; /* skip 0 and 1 */
}

uvlong
allocfh(CFid *fid)
{
	return _alloc(fid, 0);
}

uvlong
allocnodeid(CFid *fid)
{
	return _alloc(fid, 1);
}

Fusefid*
lookupfusefid(uvlong id, int isnodeid)
{
	Fusefid *ff;
	if(id < 2 || id >= nfusefid+2)
		return nil;
	ff = fusefid[(int)id-2];
	if(ff->isnodeid != isnodeid)
		return nil;
	return ff;
}

CFid*
_lookupcfid(uvlong id, int isnodeid)
{
	Fusefid *ff;
	
	if((ff = lookupfusefid(id, isnodeid)) == nil)
		return nil;
	return ff->fid;
}

CFid*
fh2fid(uvlong fh)
{
	return _lookupcfid(fh, 0);
}

CFid*
nodeid2fid(uvlong nodeid)
{
	if(nodeid == 1)
		return fsysroot;
	return _lookupcfid(nodeid, 1);
}

uvlong
qid2inode(Qid q)
{
	return q.path | ((uvlong)q.type<<56);
}

void
dir2attr(Dir *d, struct fuse_attr *attr)
{
	attr->ino = qid2inode(d->qid);
	attr->size = d->length;
	attr->blocks = (d->length+8191)/8192;
	attr->atime = d->atime;
	attr->mtime = d->mtime;
	attr->ctime = d->mtime;	/* not right */
	attr->atimensec = 0;
	attr->mtimensec = 0;
	attr->ctimensec = 0;
	attr->mode = d->mode&0777;
	if(d->mode&DMDIR)
		attr->mode |= S_IFDIR;
	else if(d->mode&DMSYMLINK)
		attr->mode |= S_IFLNK;
	else
		attr->mode |= S_IFREG;
	attr->nlink = 1;	/* works for directories! - see FUSE FAQ */
	attr->uid = getuid();
	attr->gid = getgid();
	attr->rdev = 0;
}

void
f2timeout(double f, __u64 *s, __u32 *ns)
{
	*s = f;
	*ns = (f - (int)f)*1e9;
}

void
dir2attrout(Dir *d, struct fuse_attr_out *out)
{
	f2timeout(attrtimeout, &out->attr_valid, &out->attr_valid_nsec);
	dir2attr(d, &out->attr);
}

/*
 * Lookup.  Walk to the name given as the argument.
 * The response is a fuse_entry_out giving full stat info.
 */
void
fuselookup(FuseMsg *m)
{
	char *name;
	Fusefid *ff;
	CFid *fid, *newfid;
	Dir *d;
	struct fuse_entry_out out;
	
	name = m->tx;
	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if(strchr(name, '/')){
		replyfuseerrno(m, ENOENT);
		return;
	}
	if((newfid = fswalk(fid, name)) == nil){
		replyfuseerrstr(m);
		return;
	}
	if((d = fsdirfstat(newfid)) == nil){
		fsclose(newfid);
		replyfuseerrstr(m);
		return;
	}
	out.nodeid = allocnodeid(newfid);
	ff = lookupfusefid(out.nodeid, 1);
	out.generation = ff->gen;
	f2timeout(attrtimeout, &out.attr_valid, &out.attr_valid_nsec);
	f2timeout(entrytimeout, &out.entry_valid, &out.entry_valid_nsec);
	dir2attr(d, &out.attr);
	free(d);
	replyfuse(m, &out, sizeof out);
}

/*
 * Forget.  Reference-counted clunk for nodeids.
 * Does not send a reply.
 * Each lookup response gives the kernel an additional reference 
 * to the returned nodeid.  Forget says "drop this many references
 * to this nodeid".  Our fuselookup, when presented with the same query,
 * does not return the same results (it allocates a new nodeid for each
 * call), but if that ever changes, fuseforget already handles the ref
 * counts properly.
 */
void
fuseforget(FuseMsg *m)
{
	struct fuse_forget_in *in;
	Fusefid *ff;

	in = m->tx;
	if((ff = lookupfusefid(m->hdr->nodeid, 1)) == nil)
		return;
	if(ff->ref > in->nlookup){
		ff->ref -= in->nlookup;
		return;
	}
	if(ff->ref < in->nlookup)
		fprint(2, "bad count in forget\n");
	ff->ref = 1;
	freefusefid(ff);
	freefusemsg(m);
}

/*
 * Getattr.
 * Replies with a fuse_attr_out structure giving the
 * attr for the requested nodeid in out.attr.
 * Out.attr_valid and out.attr_valid_nsec give 
 * the amount of time that the attributes can
 * be cached.
 *
 * Empirically, though, if I run ls -ld on the root
 * twice back to back, I still get two getattrs,
 * even with a one second attribute timeout!
 */
void
fusegetattr(FuseMsg *m)
{
	CFid *fid;
	struct fuse_attr_out out;
	Dir *d;

	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if((d = fsdirfstat(fid)) == nil){
		replyfuseerrstr(m);
		return;
	}
	memset(&out, 0, sizeof out);
	dir2attrout(d, &out);
	free(d);
	replyfuse(m, &out, sizeof out);
}

/*
 * Setattr.
 * FUSE treats the many Unix attribute setting routines
 * more or less like 9P does, with a single message.
 */
void
fusesetattr(FuseMsg *m)
{
	CFid *fid, *nfid;
	Dir d, *dd;
	struct fuse_setattr_in *in;
	struct fuse_attr_out out;

	in = m->tx;
	if(in->valid&FATTR_FH){
		if((fid = fh2fid(in->fh)) == nil){
			replyfuseerrno(m, ESTALE);
			return;
		}
	}else{
		if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
			replyfuseerrno(m, ESTALE);
			return;
		}
		/*
		 * Special case: Linux issues a size change to
		 * truncate a file before opening it OTRUNC.
		 * Synthetic file servers (e.g., plumber) honor 
		 * open(OTRUNC) but not wstat.
		 */
		if(in->valid == FATTR_SIZE && in->size == 0){
			if((nfid = fswalk(fid, nil)) == nil){
				replyfuseerrstr(m);
				return;
			}
			if(fsfopen(nfid, OWRITE|OTRUNC) < 0){
				replyfuseerrstr(m);
				fsclose(nfid);
				return;
			}
			fsclose(nfid);
			goto stat;
		}
	}

	nulldir(&d);
	if(in->valid&FATTR_SIZE)
		d.length = in->size;
	if(in->valid&FATTR_ATIME)
		d.atime = in->atime;
	if(in->valid&FATTR_MTIME)
		d.mtime = in->mtime;
	if(in->valid&FATTR_MODE)
		d.mode = in->mode;
	if((in->valid&FATTR_UID) || (in->valid&FATTR_GID)){
		/*
		 * I can't be bothered with these yet.
		 */
		replyfuseerrno(m, EPERM);
		return;
	}
	if(fsdirfwstat(fid, &d) < 0){
		replyfuseerrstr(m);
		return;
	}
stat:
	if((dd = fsdirfstat(fid)) == nil){
		replyfuseerrstr(m);
		return;
	}
	memset(&out, 0, sizeof out);
	dir2attrout(dd, &out);
	free(dd);
	replyfuse(m, &out, sizeof out);
}

CFid*
_fuseopenfid(uvlong nodeid, int isdir, int openmode, int *err)
{
	CFid *fid, *newfid;

	if((fid = nodeid2fid(nodeid)) == nil){
		*err = ESTALE;
		return nil;
	}
	if(isdir && !(fsqid(fid).type&QTDIR)){
		*err = ENOTDIR;
		return nil;
	}
	if(openmode != OREAD && fsqid(fid).type&QTDIR){
		*err = EISDIR;
		return nil;
	}

	/* Clone fid to get one we can open. */
	newfid = fswalk(fid, nil);
	if(newfid == nil){
		*err = errstr2errno();
		return nil;
	}
		
	if(fsfopen(newfid, openmode) < 0){
		*err = errstr2errno();
		fsclose(newfid);
		return nil;
	}

	return newfid;
}

/*
 * Open & Opendir.
 * Argument is a struct fuse_open_in.
 * The mode field is ignored (presumably permission bits)
 * and flags is the open mode.
 * Replies with a struct fuse_open_out.
 */
void
_fuseopen(FuseMsg *m, int isdir)
{
	struct fuse_open_in *in;
	struct fuse_open_out out;
	CFid *fid;
	int openmode, flags, err;

	in = m->tx;
	flags = in->flags;
	openmode = flags&3;
	flags &= ~3;
	flags &= ~(O_DIRECTORY|O_NONBLOCK|O_LARGEFILE|O_CLOEXEC);
#ifdef O_NOFOLLOW
	flags &= ~O_NOFOLLOW;
#endif
#ifdef O_LARGEFILE
	flags &= ~O_LARGEFILE;
#endif

	/*
	 * Discarding O_APPEND here is not completely wrong,
	 * because the host kernel will rewrite the offsets
	 * of write system calls for us.  That's the best we
	 * can do on Unix anyway.
	 */
	flags &= ~O_APPEND;
	if(flags & O_TRUNC){
		openmode |= OTRUNC;
		flags &= ~O_TRUNC;
	}
	/*
	 * Could translate but not standard 9P:
	 *	O_DIRECT -> ODIRECT
	 *	O_NONBLOCK -> ONONBLOCK
	 */
	if(flags){
		fprint(2, "unexpected open flags %#uo\n", (uint)in->flags);
		replyfuseerrno(m, EACCES);
		return;
	}
	if((fid = _fuseopenfid(m->hdr->nodeid, isdir, openmode, &err)) == nil){
		replyfuseerrno(m, err);
		return;
	}
	out.fh = allocfh(fid);
	out.open_flags = FOPEN_DIRECT_IO;	/* no page cache */	
	replyfuse(m, &out, sizeof out);
}

void
fuseopen(FuseMsg *m)
{
	_fuseopen(m, 0);
}

void
fuseopendir(FuseMsg *m)
{
	_fuseopen(m, 1);
}

/*
 * Create & Mkdir.
 */
CFid*
_fusecreate(uvlong nodeid, char *name, int perm, int ismkdir, int omode, struct fuse_entry_out *out, int *err)
{
	CFid *fid, *newfid, *newfid2;
	Dir *d;
	Fusefid *ff;

	if((fid = nodeid2fid(nodeid)) == nil){
		*err = ESTALE;
		return nil;
	}
	perm &= 0777;
	if(ismkdir)
		perm |= DMDIR;
	if(ismkdir && omode != OREAD){
		*err = EPERM;
		return nil;
	}
	if((newfid = fswalk(fid, nil)) == nil){
		*err = errstr2errno();
		return nil;
	}
	if(fsfcreate(newfid, name, omode, perm) < 0){
		*err = errstr2errno();
		fsclose(newfid);
		return nil;
	}
	if((d = fsdirfstat(newfid)) == nil){
		*err = errstr2errno();
		fsfremove(newfid);
		return nil;
	}
	/*
	 * This fid is no good, because it's open.
	 * We need an unopened fid.  Sigh.
	 */
	if((newfid2 = fswalk(fid, name)) == nil){
		*err = errstr2errno();
		free(d);
		fsfremove(newfid);
		return nil;
	}
	out->nodeid = allocnodeid(newfid2);
	ff = lookupfusefid(out->nodeid, 1);
	out->generation = ff->gen;
	f2timeout(attrtimeout, &out->attr_valid, &out->attr_valid_nsec);
	f2timeout(entrytimeout, &out->entry_valid, &out->entry_valid_nsec);
	dir2attr(d, &out->attr);
	free(d);
	return newfid;
}

void
fusemkdir(FuseMsg *m)
{
	struct fuse_mkdir_in *in;
	struct fuse_entry_out out;
	CFid *fid;
	int err;
	char *name;
	
	in = m->tx;
	name = (char*)(in+1);
	if((fid = _fusecreate(m->hdr->nodeid, name, in->mode, 1, OREAD, &out, &err)) == nil){
		replyfuseerrno(m, err);
		return;
	}
	/* Toss the open fid. */
	fsclose(fid);
	replyfuse(m, &out, sizeof out);
}

void
fusecreate(FuseMsg *m)
{
	struct fuse_open_in *in;
	struct fuse_create_out out;
	CFid *fid;
	int err, openmode, flags;
	char *name;
	
	in = m->tx;
	flags = in->flags;
	openmode = in->flags&3;
	flags &= ~3;
	flags &= ~(O_DIRECTORY|O_NONBLOCK|O_LARGEFILE|O_EXCL);
	flags &= ~O_APPEND;	/* see comment in _fuseopen */
	flags &= ~(O_CREAT|O_TRUNC);	/* huh? */
	if(flags){
		fprint(2, "bad mode %#uo\n", in->flags);
		replyfuseerrno(m, EACCES);
		return;
	}
	name = (char*)(in+1);
	if((fid = _fusecreate(m->hdr->nodeid, name, in->mode, 0, openmode, &out.e, &err)) == nil){
		replyfuseerrno(m, err);
		return;
	}
	out.o.fh = allocfh(fid);
	out.o.open_flags = FOPEN_DIRECT_IO;	/* no page cache */
	replyfuse(m, &out, sizeof out);
}

/*
 * Access.  
 * Lib9pclient implements this just as Plan 9 does,
 * by opening the file (or not) and then closing it.
 */
void
fuseaccess(FuseMsg *m)
{
	struct fuse_access_in *in;
	CFid *fid;
	int err, omode;
	static int a2o[] = {
		0,
		OEXEC,
		OWRITE,
		ORDWR,
		OREAD,
		OEXEC,
		ORDWR,
		ORDWR
	};
	
	in = m->tx;
	if(in->mask >= nelem(a2o)){
		replyfuseerrno(m, EINVAL);
		return;
	}
	omode = a2o[in->mask];
	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if(fsqid(fid).type&QTDIR)
		omode = OREAD;
	if((fid = _fuseopenfid(m->hdr->nodeid, 0, omode, &err)) == nil){
		replyfuseerrno(m, err);
		return;
	}
	fsclose(fid);
	replyfuse(m, nil, 0);
}

/*
 * Release.
 * Equivalent of clunk for file handles.
 * in->flags is the open mode used in Open or Opendir.
 */
void
fuserelease(FuseMsg *m)
{
	struct fuse_release_in *in;
	Fusefid *ff;
	
	in = m->tx;
	if((ff = lookupfusefid(in->fh, 0)) != nil)
		freefusefid(ff);
	else
		fprint(2, "fuserelease: fh not found\n");
	replyfuse(m, nil, 0);
}

void
fusereleasedir(FuseMsg *m)
{
	fuserelease(m);
}

/*
 * Read.
 * Read from file handle in->fh at offset in->offset for size in->size.
 * We truncate size to maxwrite just to keep the buffer reasonable.
 */
void
fuseread(FuseMsg *m)
{
	int n;
	uchar *buf;
	CFid *fid;
	struct fuse_read_in *in;

	in = m->tx;
	if((fid = fh2fid(in->fh)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	n = in->size;
	if(n > fusemaxwrite)
		n = fusemaxwrite;
	buf = emalloc(n);
	n = fspread(fid, buf, n, in->offset);
	if(n < 0){
		free(buf);
		replyfuseerrstr(m);
		return;
	}
	replyfuse(m, buf, n);
	free(buf);
}

/*
 * Readlink.
 */
void
fusereadlink(FuseMsg *m)
{
	Dir *d;
	CFid *fid;

	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if((d = fsdirfstat(fid)) == nil){
		replyfuseerrstr(m);
		return;
	}
	if(!(d->mode&DMSYMLINK)){
		replyfuseerrno(m, EINVAL);
		return;
	}
	replyfuse(m, d->ext, strlen(d->ext));
	free(d);
	return;
}

/* 
 * Readdir.
 * Read from file handle in->fh at offset in->offset for size in->size.
 * We truncate size to maxwrite just to keep the buffer reasonable.
 * We assume 9P directory read semantics: a read at offset 0 rewinds
 * and a read at any other offset starts where we left off.
 * If it became necessary, we could implement a crude seek
 * or cache the entire list of directory entries.
 * Directory entries read from 9P but not yet handed to FUSE
 * are stored in m->d,nd,d0.
 */
int canpack(Dir*, uvlong, uchar**, uchar*);
Dir *dotdirs(CFid*);
void
fusereaddir(FuseMsg *m)
{
	struct fuse_read_in *in;
	uchar *buf, *p, *ep;
	int n;
	Fusefid *ff;
	
	in = m->tx;
	if((ff = lookupfusefid(in->fh, 0)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}	
	if(in->offset == 0){
		fsseek(ff->fid, 0, 0);
		free(ff->d0);
		ff->d0 = ff->d = dotdirs(ff->fid);
		ff->nd = 2;
	}
	n = in->size;
	if(n > fusemaxwrite)
		n = fusemaxwrite;
	buf = emalloc(n);
	p = buf;
	ep = buf + n;
	for(;;){
		while(ff->nd > 0){
			if(!canpack(ff->d, ff->off, &p, ep))
				goto out;
			ff->off++;
			ff->d++;
			ff->nd--;
		}
		free(ff->d0);
		ff->d0 = nil;
		ff->d = nil;
		if((ff->nd = fsdirread(ff->fid, &ff->d0)) < 0){
			replyfuseerrstr(m);
			free(buf);
			return;
		}
		if(ff->nd == 0)
			break;
		ff->d = ff->d0;
	}
out:			
	replyfuse(m, buf, p - buf);
	free(buf);
}

/*
 * Fuse assumes that it can always read two directory entries.
 * If it gets just one, it will double it in the dirread results.
 * Thus if a directory contains just "a", you see "a" twice.
 * Adding . as the first directory entry works around this.
 * We could add .. too, but it isn't necessary.
 */
Dir*
dotdirs(CFid *f)
{
	Dir *d;
	CFid *f1;

	d = emalloc(2*sizeof *d);
	d[0].name = ".";
	d[0].qid = fsqid(f);
	d[1].name = "..";
	f1 = fswalk(f, "..");
	if(f1){
		d[1].qid = fsqid(f1);
		fsclose(f1);
	}
	return d;
}

int
canpack(Dir *d, uvlong off, uchar **pp, uchar *ep)
{
	uchar *p;
	struct fuse_dirent *de;
	int pad, size;
	
	p = *pp;
	size = FUSE_NAME_OFFSET + strlen(d->name);
	pad = 0;
	if(size%8)
		pad = 8 - size%8;
	if(size+pad > ep - p)
		return 0;
	de = (struct fuse_dirent*)p;
	de->ino = qid2inode(d->qid);
	de->off = off;
	de->namelen = strlen(d->name);
	memmove(de->name, d->name, de->namelen);
	if(pad > 0)
		memset(de->name+de->namelen, 0, pad);
	*pp = p+size+pad;
	return 1;
}

/*
 * Write.
 * Write from file handle in->fh at offset in->offset for size in->size.
 * Don't know what in->write_flags means.
 * 
 * Apparently implementations are allowed to buffer these writes
 * and wait until Flush is sent, but FUSE docs say flush may be
 * called zero, one, or even more times per close.  So better do the
 * actual writing here.  Also, errors that happen during Flush just
 * show up in the close() return status, which no one checks anyway.
 */
void
fusewrite(FuseMsg *m)
{
	struct fuse_write_in *in;
	struct fuse_write_out out;
	void *a;
	CFid *fid;
	int n;
	
	in = m->tx;
	a = in+1;
	if((fid = fh2fid(in->fh)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if(in->size > fusemaxwrite){
		replyfuseerrno(m, EINVAL);
		return;
	}
	n = fspwrite(fid, a, in->size, in->offset);
	if(n < 0){
		replyfuseerrstr(m);
		return;
	}
	out.size = n;
	replyfuse(m, &out, sizeof out);
}

/*
 * Flush.  Supposed to flush any buffered writes.  Don't use this.
 * 
 * Flush is a total crock.  It gets called on close() of a file descriptor
 * associated with this open file.  Some open files have multiple file
 * descriptors and thus multiple closes of those file descriptors.
 * In those cases, Flush is called multiple times.  Some open files
 * have file descriptors that are closed on process exit instead of
 * closed explicitly.  For those files, Flush is never called.
 * Even more amusing, Flush gets called before close() of read-only
 * file descriptors too!
 * 
 * This is just a bad idea.
 */
void
fuseflush(FuseMsg *m)
{
	replyfuse(m, nil, 0);
}

/*
 * Unlink & Rmdir.
 */
void
_fuseremove(FuseMsg *m, int isdir)
{
	char *name;
	CFid *fid, *newfid;
	
	name = m->tx;
	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if(strchr(name, '/')){
		replyfuseerrno(m, ENOENT);
		return;
	}
	if((newfid = fswalk(fid, name)) == nil){
		replyfuseerrstr(m);
		return;
	}
	if(isdir && !(fsqid(newfid).type&QTDIR)){
		replyfuseerrno(m, ENOTDIR);
		fsclose(newfid);
		return;
	}
	if(!isdir && (fsqid(newfid).type&QTDIR)){
		replyfuseerrno(m, EISDIR);
		fsclose(newfid);
		return;
	}
	if(fsfremove(newfid) < 0){
		replyfuseerrstr(m);
		return;
	}
	replyfuse(m, nil, 0);
}

void
fuseunlink(FuseMsg *m)
{
	_fuseremove(m, 0);
}

void
fusermdir(FuseMsg *m)
{
	_fuseremove(m, 1);
}

/*
 * Rename.
 *
 * FUSE sends the nodeid for the source and destination
 * directory and then the before and after names as strings.
 * 9P can only do the rename if the source and destination
 * are the same.  If the same nodeid is used for source and
 * destination, we're fine, but if FUSE gives us different nodeids
 * that happen to correspond to the same directory, we have
 * no way of figuring that out.  Let's hope it doesn't happen too often.
 */
void
fuserename(FuseMsg *m)
{
	struct fuse_rename_in *in;
	char *before, *after;
	CFid *fid, *newfid;
	Dir d;
	
	in = m->tx;
	if(in->newdir != m->hdr->nodeid){
		replyfuseerrno(m, EXDEV);
		return;
	}
	before = (char*)(in+1);
	after = before + strlen(before) + 1;
	if((fid = nodeid2fid(m->hdr->nodeid)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	if(strchr(before, '/') || strchr(after, '/')){
		replyfuseerrno(m, ENOENT);
		return;
	}
	if((newfid = fswalk(fid, before)) == nil){
		replyfuseerrstr(m);
		return;
	}
	nulldir(&d);
	d.name = after;
	if(fsdirfwstat(newfid, &d) < 0){
		replyfuseerrstr(m);
		fsclose(newfid);
		return;
	}
	fsclose(newfid);
	replyfuse(m, nil, 0);
}

/*
 * Fsync.  Commit file info to stable storage.
 * Not sure what in->fsync_flags are.
 */
void
fusefsync(FuseMsg *m)
{
	struct fuse_fsync_in *in;
	CFid *fid;
	Dir d;
	
	in = m->tx;
	if((fid = fh2fid(in->fh)) == nil){
		replyfuseerrno(m, ESTALE);
		return;
	}
	nulldir(&d);
	if(fsdirfwstat(fid, &d) < 0){
		replyfuseerrstr(m);
		return;
	}
	replyfuse(m, nil, 0);
}

/*
 * Fsyncdir.  Commit dir info to stable storage?
 */
void
fusefsyncdir(FuseMsg *m)
{
	fusefsync(m);
}

/*
 * Statfs.  Send back information about file system.
 * Not really worth implementing, except that if we
 * reply with ENOSYS, programs like df print messages like
 *   df: `/tmp/z': Function not implemented
 * and that gets annoying.  Returning all zeros excludes
 * us from df without appearing to cause any problems.
 */
void
fusestatfs(FuseMsg *m)
{
	struct fuse_statfs_out out;
	
	memset(&out, 0, sizeof out);
	replyfuse(m, &out, sizeof out);
}

void (*fusehandlers[100])(FuseMsg*);

struct {
	int op;
	void (*fn)(FuseMsg*);
} fuselist[] = {
	{ FUSE_LOOKUP,		fuselookup },
	{ FUSE_FORGET,		fuseforget },
	{ FUSE_GETATTR,		fusegetattr },
	{ FUSE_SETATTR,		fusesetattr },
	/*
	 * FUSE_SYMLINK, FUSE_MKNOD are unimplemented.
	 */
	{ FUSE_READLINK,	fusereadlink },
	{ FUSE_MKDIR,		fusemkdir },
	{ FUSE_UNLINK,		fuseunlink },
	{ FUSE_RMDIR,		fusermdir },
	{ FUSE_RENAME,		fuserename },
	/*
	 * FUSE_LINK is unimplemented.
	 */
	{ FUSE_OPEN,		fuseopen },
	{ FUSE_READ,		fuseread },
	{ FUSE_WRITE,		fusewrite },
	{ FUSE_STATFS,		fusestatfs },
	{ FUSE_RELEASE,		fuserelease },
	{ FUSE_FSYNC,		fusefsync },
	/*
	 * FUSE_SETXATTR, FUSE_GETXATTR, FUSE_LISTXATTR, and
	 * FUSE_REMOVEXATTR are unimplemented. 
	 * FUSE will stop sending these requests after getting
	 * an -ENOSYS reply (see dispatch below).
	 */
	{ FUSE_FLUSH,		fuseflush },
	/*
	 * FUSE_INIT is handled in initfuse and should not be seen again.
	 */
	{ FUSE_OPENDIR,		fuseopendir },
	{ FUSE_READDIR,		fusereaddir },
	{ FUSE_RELEASEDIR,	fusereleasedir },
	{ FUSE_FSYNCDIR,	fusefsyncdir },
	{ FUSE_ACCESS,		fuseaccess },
	{ FUSE_CREATE,		fusecreate },
};

void
fusethread(void *v)
{
	FuseMsg *m;

	m = v;
	if((uint)m->hdr->opcode >= nelem(fusehandlers) 
	|| !fusehandlers[m->hdr->opcode]){
		replyfuseerrno(m, ENOSYS);
		return;
	}
	fusehandlers[m->hdr->opcode](m);
}

void
fusedispatch(void *v)
{
	int i;
	FuseMsg *m;

	eofkill9pclient = 1;	/* threadexitsall on 9P eof */
	atexit(unmountatexit);

	recvp(fusechan);	/* sync */

	for(i=0; i<nelem(fuselist); i++){
		if(fuselist[i].op >= nelem(fusehandlers))
			sysfatal("make fusehandlers bigger op=%d", fuselist[i].op);
		fusehandlers[fuselist[i].op] = fuselist[i].fn;
	}

	while((m = recvp(fusechan)) != nil) {
		switch(m->hdr->opcode) {
		case FUSE_FORGET:
		 	fusehandlers[m->hdr->opcode](m);
			break;
		default: 
			threadcreate(fusethread, m, STACK);
		}
	}
}

void*
emalloc(uint n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc(%d): %r", n);
	memset(p, 0, n);
	return p;
}

void*
erealloc(void *p, uint n)
{
	p = realloc(p, n);
	if(p == nil)
		sysfatal("realloc(..., %d): %r", n);
	return p;
}

char*
estrdup(char *p)
{
	char *pp;
	pp = strdup(p);
	if(pp == nil)
		sysfatal("strdup(%.20s): %r", p);
	return pp;
}

void
watchfd(void *v)
{
	int fd = (int)(uintptr)v;

	/* wait for exception (file closed) */
	fd_set set;
	FD_ZERO(&set);
	FD_SET(fd, &set);
	if(select(fd+1, NULL, NULL, &set, NULL) >= 0)
		threadexitsall(nil);
	return;
}
