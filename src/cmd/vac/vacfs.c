#include "stdinc.h"
#include <auth.h>
#include <fcall.h>
#include "vac.h"

typedef struct Fid Fid;
typedef struct DirBuf DirBuf;

enum
{
	OPERM	= 0x3,		/* mask of all permission types in open mode */
};

enum
{
	DirBufSize = 20,
};

struct Fid
{
	short busy;
	short open;
	int fid;
	char *user;
	Qid qid;
	VacFile *file;

	DirBuf *db;

	Fid	*next;
};

struct DirBuf
{
	VacDirEnum *vde;
	VacDir buf[DirBufSize];
	int i, n;
	int eof;
};

enum
{
	Pexec =		1,
	Pwrite = 	2,
	Pread = 	4,
	Pother = 	1,
	Pgroup = 	8,
	Powner =	64,
};

Fid	*fids;
uchar	*data;
int	mfd[2];
char	*user;
uchar	mdata[8192+IOHDRSZ];
int messagesize = sizeof mdata;
Fcall	rhdr;
Fcall	thdr;
VacFS	*fs;
VtSession *session;
int	noperm;

Fid *	newfid(int);
void	error(char*);
void	io(void);
void	shutdown(void);
void	usage(void);
int	perm(Fid*, int);
int	permf(VacFile*, char*, int);
ulong	getl(void *p);
void	init(char*, char*, long, int);
DirBuf	*dirBufAlloc(VacFile*);
VacDir	*dirBufGet(DirBuf*);
int	dirBufUnget(DirBuf*);
void	dirBufFree(DirBuf*);
int	vacdirread(Fid *f, char *p, long off, long cnt);
int	vdStat(VacDir *vd, uchar *p, int np);

char	*rflush(Fid*), *rversion(Fid*),
	*rauth(Fid*), *rattach(Fid*), *rwalk(Fid*),
	*ropen(Fid*), *rcreate(Fid*),
	*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
	*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*);

char 	*(*fcalls[])(Fid*) = {
	[Tflush]	rflush,
	[Tversion]	rversion,
	[Tattach]	rattach,
	[Tauth]		rauth,
	[Twalk]		rwalk,
	[Topen]		ropen,
	[Tcreate]	rcreate,
	[Tread]		rread,
	[Twrite]	rwrite,
	[Tclunk]	rclunk,
	[Tremove]	rremove,
	[Tstat]		rstat,
	[Twstat]	rwstat,
};

char	Eperm[] =	"permission denied";
char	Enotdir[] =	"not a directory";
char	Enotexist[] =	"file does not exist";
char	Einuse[] =	"file in use";
char	Eexist[] =	"file exists";
char	Enotowner[] =	"not owner";
char	Eisopen[] = 	"file already open for I/O";
char	Excl[] = 	"exclusive use file already open";
char	Ename[] = 	"illegal name";
char	Erdonly[] = 	"read only file system";
char	Eio[] = 	"i/o error";
char	Eempty[] = 	"directory is not empty";
char	Emode[] =	"illegal mode";

int dflag;

void
notifyf(void *a, char *s)
{
	USED(a);
	if(strncmp(s, "interrupt", 9) == 0)
		noted(NCONT);
	noted(NDFLT);
}

void
main(int argc, char *argv[])
{
	char *defmnt;
	int p[2];
	char buf[12];
	int fd;
	int stdio = 0;
	char *host = nil;
	long ncache = 1000;
	int readOnly = 1;

	defmnt = "/n/vac";
	ARGBEGIN{
	case 'd':
		fmtinstall('F', fcallfmt);
		dflag = 1;
		break;
	case 'c':
		ncache = atoi(ARGF());
		break;
	case 'i':
		defmnt = 0;
		stdio = 1;
		mfd[0] = 0;
		mfd[1] = 1;
		break;
	case 'h':
		host = ARGF();
		break;
	case 's':
		defmnt = 0;
		break;
	case 'p':
		noperm = 1;
		break;
	case 'm':
		defmnt = ARGF();
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	vtAttach();

	init(argv[0], host, ncache, readOnly);

	if(pipe(p) < 0)
		sysfatal("pipe failed: %r");
	if(!stdio){
		mfd[0] = p[0];
		mfd[1] = p[0];
		if(defmnt == 0){
			fd = create("#s/vacfs", OWRITE, 0666);
			if(fd < 0)
				sysfatal("create of /srv/vacfs failed: %r");
			sprint(buf, "%d", p[1]);
			if(write(fd, buf, strlen(buf)) < 0)
				sysfatal("writing /srv/vacfs: %r");
		}
	}

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
	case -1:
		sysfatal("fork: %r");
	case 0:
		vtAttach();
		close(p[1]);
		io();
		shutdown();
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
		if(defmnt && mount(p[1], -1, defmnt, MREPL|MCREATE, "") < 0)
			sysfatal("mount failed: %r");
	}
	vtDetach();
	exits(0);
}

void
usage(void)
{
	fprint(2, "usage: %s [-sd] [-h host] [-c ncache] [-m mountpoint] vacfile\n", argv0);
	exits("usage");
}

char*
rversion(Fid *unused)
{
	Fid *f;

	USED(unused);

	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);

	if(rhdr.msize < 256)
		return "version: message size too small";
	messagesize = rhdr.msize;
	if(messagesize > sizeof mdata)
		messagesize = sizeof mdata;
	thdr.msize = messagesize;
	if(strncmp(rhdr.version, "9P2000", 6) != 0)
		return "unrecognized 9P version";
	thdr.version = "9P2000";
	return nil;
}

char*
rflush(Fid *f)
{
	USED(f);
	return 0;
}

char*
rauth(Fid *f)
{
	USED(f);
	return "vacfs: authentication not required";
}

char*
rattach(Fid *f)
{
	/* no authentication for the momment */
	VacFile *file;

	file = vfsGetRoot(fs);
	if(file == nil)
		return vtGetError();
	f->busy = 1;
	f->file = file;
	f->qid = (Qid){vfGetId(f->file), 0, QTDIR};
	thdr.qid = f->qid;
	if(rhdr.uname[0])
		f->user = vtStrDup(rhdr.uname);
	else
		f->user = "none";
	return 0;
}

VacFile*
_vfWalk(VacFile *file, char *name)
{
	VacFile *n;

	n = vfWalk(file, name);
	if(n)
		return n;
	if(strcmp(name, "SLASH") == 0)
		return vfWalk(file, "/");
	return nil;
}

char*
rwalk(Fid *f)
{
	VacFile *file, *nfile;
	Fid *nf;
	int nqid, nwname;
	Qid qid;

	if(f->busy == 0)
		return Enotexist;
	nf = nil;
	if(rhdr.fid != rhdr.newfid){
		if(f->open)
			return Eisopen;
		if(f->busy == 0)
			return Enotexist;
		nf = newfid(rhdr.newfid);
		if(nf->busy)
			return Eisopen;
		nf->busy = 1;
		nf->open = 0;
		nf->qid = f->qid;
		nf->file = vfIncRef(f->file);
		nf->user = vtStrDup(f->user);
		f = nf;
	}

	nwname = rhdr.nwname;

	/* easy case */
	if(nwname == 0) {
		thdr.nwqid = 0;
		return 0;
	}

	file = f->file;
	vfIncRef(file);
	qid = f->qid;

	for(nqid = 0; nqid < nwname; nqid++){
		if((qid.type & QTDIR) == 0){
			vtSetError(Enotdir);
			break;
		}
		if(!permf(file, f->user, Pexec)) {
			vtSetError(Eperm);
			break;
		}
		nfile = _vfWalk(file, rhdr.wname[nqid]);
		if(nfile == nil)
			break;
		vfDecRef(file);
		file = nfile;
		qid.type = QTFILE;
		if(vfIsDir(file))
			qid.type = QTDIR;
		qid.vers = vfGetMcount(file);
		qid.path = vfGetId(file);
		thdr.wqid[nqid] = qid;
	}

	thdr.nwqid = nqid;

	if(nqid == nwname){
		/* success */
		f->qid = thdr.wqid[nqid-1];
		vfDecRef(f->file);
		f->file = file;
		return 0;
	}

	vfDecRef(file);
	if(nf != nil)
		rclunk(nf);

	/* only error on the first element */
	if(nqid == 0)
		return vtGetError();

	return 0;
}

char *
ropen(Fid *f)
{
	int mode, trunc;

	if(f->open)
		return Eisopen;
	if(!f->busy)
		return Enotexist;
	mode = rhdr.mode;
	thdr.iounit = messagesize - IOHDRSZ;
	if(f->qid.type & QTDIR){
		if(mode != OREAD)
			return Eperm;
		if(!perm(f, Pread))
			return Eperm;
		thdr.qid = f->qid;
		f->db = nil;
		f->open = 1;
		return 0;
	}
	if(mode & ORCLOSE)
		return Erdonly;
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
		if(!perm(f, Pwrite))
			return Eperm;
	if(mode==OREAD || mode==ORDWR)
		if(!perm(f, Pread))
			return Eperm;
	if(mode==OEXEC)
		if(!perm(f, Pexec))
			return Eperm;
	thdr.qid = f->qid;
	thdr.iounit = messagesize - IOHDRSZ;
	f->open = 1;
	return 0;
}

char*
rcreate(Fid* fid)
{
	VacFile *vf;
	ulong mode;

	if(fid->open)
		return Eisopen;
	if(!fid->busy)
		return Enotexist;
	if(vfsIsReadOnly(fs))
		return Erdonly;
	vf = fid->file;
	if(!vfIsDir(vf))
		return Enotdir;
	if(!permf(vf, fid->user, Pwrite))
		return Eperm;

	mode = rhdr.perm & 0777;

	if(rhdr.perm & DMDIR){
		if((rhdr.mode & OTRUNC) || (rhdr.perm & DMAPPEND))
			return Emode;
		switch(rhdr.mode & OPERM){
		default:
			return Emode;
		case OEXEC:
		case OREAD:
			break;
		case OWRITE:
		case ORDWR:
			return Eperm;
		}
		mode |= ModeDir;
	}
	vf = vfCreate(vf, rhdr.name, mode, "none");
	if(vf == nil)
		return vtGetError();
	vfDecRef(fid->file);

	fid->file = vf;
	fid->qid.type = QTFILE;
	if(vfIsDir(vf))
		fid->qid.type = QTDIR;
	fid->qid.vers = vfGetMcount(vf);
	fid->qid.path = vfGetId(vf);

	thdr.qid = fid->qid;
	thdr.iounit = messagesize - IOHDRSZ;

	return 0;
}

char*
rread(Fid *f)
{
	char *buf;
	vlong off;
	int cnt;
	VacFile *vf;
	char *err;
	int n;

	if(!f->busy)
		return Enotexist;
	vf = f->file;
	thdr.count = 0;
	off = rhdr.offset;
	buf = thdr.data;
	cnt = rhdr.count;
	if(f->qid.type & QTDIR)
		n = vacdirread(f, buf, off, cnt);
	else
		n = vfRead(vf, buf, cnt, off);
	if(n < 0) {
		err = vtGetError();
		if(err == nil)
			err = "unknown error!";
		return err;
	}
	thdr.count = n;
	return 0;
}

char*
rwrite(Fid *f)
{
	char *buf;
	vlong off;
	int cnt;
	VacFile *vf;

	if(!f->busy)
		return Enotexist;
	vf = f->file;
	thdr.count = 0;
	off = rhdr.offset;
	buf = rhdr.data;
	cnt = rhdr.count;
	if(f->qid.type & QTDIR)
		return "file is a directory";
	thdr.count = vfWrite(vf, buf, cnt, off, "none");
	if(thdr.count < 0) {
fprint(2, "write failed: %s\n", vtGetError());
		return vtGetError();
	}
	return 0;
}

char *
rclunk(Fid *f)
{
	f->busy = 0;
	f->open = 0;
	vtMemFree(f->user);
	f->user = nil;
	vfDecRef(f->file);
	f->file = nil;
	dirBufFree(f->db);
	f->db = nil;
	return 0;
}

char *
rremove(Fid *f)
{
	VacFile *vf, *vfp;
	char *err = nil;

	if(!f->busy)
		return Enotexist;
	vf = f->file;
	vfp = vfGetParent(vf);

	if(!permf(vfp, f->user, Pwrite)) {
		err = Eperm;
		goto Exit;
	}

	if(!vfRemove(vf, "none")) {
print("vfRemove failed\n");
		err = vtGetError();
	}

Exit:
	vfDecRef(vfp);
	rclunk(f);
	return err;
}

char *
rstat(Fid *f)
{
	VacDir dir;
	static uchar statbuf[1024];

	if(!f->busy)
		return Enotexist;
	vfGetDir(f->file, &dir);
	thdr.stat = statbuf;
	thdr.nstat = vdStat(&dir, thdr.stat, sizeof statbuf);
	vdCleanup(&dir);
	return 0;
}

char *
rwstat(Fid *f)
{
	if(!f->busy)
		return Enotexist;
	return Erdonly;
}

int
vdStat(VacDir *vd, uchar *p, int np)
{
	Dir dir;

	memset(&dir, 0, sizeof(dir));

	/*
	 * Where do path and version come from
	 */
	dir.qid.path = vd->qid;
	dir.qid.vers = vd->mcount;
	dir.mode = vd->mode & 0777;
	if(vd->mode & ModeAppend){
		dir.qid.type |= QTAPPEND;
		dir.mode |= DMAPPEND;
	}
	if(vd->mode & ModeExclusive){
		dir.qid.type |= QTEXCL;
		dir.mode |= DMEXCL;
	}
	if(vd->mode & ModeDir){
		dir.qid.type |= QTDIR;
		dir.mode |= DMDIR;
	}

	dir.atime = vd->atime;
	dir.mtime = vd->mtime;
	dir.length = vd->size;

	dir.name = vd->elem;
	dir.uid = vd->uid;
	dir.gid = vd->gid;
	dir.muid = vd->mid;

	return convD2M(&dir, p, np);
}

DirBuf*
dirBufAlloc(VacFile *vf)
{
	DirBuf *db;

	db = vtMemAllocZ(sizeof(DirBuf));
	db->vde = vfDirEnum(vf);
	return db;
}

VacDir *
dirBufGet(DirBuf *db)
{
	VacDir *vd;
	int n;

	if(db->eof)
		return nil;

	if(db->i >= db->n) {
		n = vdeRead(db->vde, db->buf, DirBufSize);
		if(n < 0)
			return nil;
		db->i = 0;
		db->n = n;
		if(n == 0) {
			db->eof = 1;
			return nil;
		}
	}

	vd = db->buf + db->i;
	db->i++;

	return vd;
}

int
dirBufUnget(DirBuf *db)
{
	assert(db->i > 0);
	db->i--;
	return 1;
}

void
dirBufFree(DirBuf *db)
{
	int i;

	if(db == nil)
		return;

	for(i=db->i; i<db->n; i++)
		vdCleanup(db->buf + i);
	vdeFree(db->vde);
	vtMemFree(db);
}

int
vacdirread(Fid *f, char *p, long off, long cnt)
{
	int n, nb;
	VacDir *vd;

	/*
	 * special case of rewinding a directory
	 * otherwise ignore the offset
	 */
	if(off == 0 && f->db) {
		dirBufFree(f->db);
		f->db = nil;
	}

	if(f->db == nil)
		f->db = dirBufAlloc(f->file);

	for(nb = 0; nb < cnt; nb += n) {
		vd = dirBufGet(f->db);
		if(vd == nil) {
			if(!f->db->eof)
				return -1;
			break;
		}
		n = vdStat(vd, (uchar*)p, cnt-nb);
		if(n <= BIT16SZ) {
			dirBufUnget(f->db);
			break;
		}
		vdCleanup(vd);
		p += n;
	}
	return nb;
}

Fid *
newfid(int fid)
{
	Fid *f, *ff;

	ff = 0;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = vtMemAllocZ(sizeof *f);
	f->fid = fid;
	f->next = fids;
	fids = f;
	return f;
}

void
io(void)
{
	char *err;
	int n;

	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error
		 */
		n = read9pmsg(mfd[0], mdata, sizeof mdata);
		if(n == 0)
			continue;
		if(n < 0)
			break;
		if(convM2S(mdata, n, &rhdr) != n)
			sysfatal("convM2S conversion error");

		if(dflag)
			fprint(2, "vacfs:<-%F\n", &rhdr);

		thdr.data = (char*)mdata + IOHDRSZ;
		if(!fcalls[rhdr.type])
			err = "bad fcall type";
		else
			err = (*fcalls[rhdr.type])(newfid(rhdr.fid));
		if(err){
			thdr.type = Rerror;
			thdr.ename = err;
		}else{
			thdr.type = rhdr.type + 1;
			thdr.fid = rhdr.fid;
		}
		thdr.tag = rhdr.tag;
		if(dflag)
			fprint(2, "vacfs:->%F\n", &thdr);
		n = convS2M(&thdr, mdata, messagesize);
		if(write(mfd[1], mdata, n) != n)
			sysfatal("mount write: %r");
	}
}

int
permf(VacFile *vf, char *user, int p)
{
	VacDir dir;
	ulong perm;

	if(!vfGetDir(vf, &dir))
		return 0;
	perm = dir.mode & 0777;
	if(noperm)
		goto Good;
	if((p*Pother) & perm)
		goto Good;
	if(strcmp(user, dir.gid)==0 && ((p*Pgroup) & perm))
		goto Good;
	if(strcmp(user, dir.uid)==0 && ((p*Powner) & perm))
		goto Good;
	vdCleanup(&dir);
	return 0;
Good:
	vdCleanup(&dir);
	return 1;
}

int
perm(Fid *f, int p)
{
	return permf(f->file, f->user, p);
}

void
init(char *file, char *host, long ncache, int readOnly)
{
	notify(notifyf);
	user = getuser();

	fmtinstall('V', vtScoreFmt);
	fmtinstall('R', vtErrFmt);

	session = vtDial(host, 0);
	if(session == nil)
		vtFatal("could not connect to server: %s", vtGetError());

	if(!vtConnect(session, 0))
		vtFatal("vtConnect: %s", vtGetError());

	fs = vfsOpen(session, file, readOnly, ncache);
	if(fs == nil)
		vtFatal("vfsOpen: %s", vtGetError());
}

void
shutdown(void)
{
	Fid *f;

	for(f = fids; f; f = f->next) {
		if(!f->busy)
			continue;
fprint(2, "open fid: %d\n", f->fid);
		rclunk(f);
	}

	vfsClose(fs);
	vtClose(session);
}

