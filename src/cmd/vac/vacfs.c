#include "stdinc.h"
#include <auth.h>
#include <fcall.h>
#include <thread.h>
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
VacFs	*fs;
VtConn  *conn;
// VtSession *session;
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
void 	srv(void* a);


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
threadmain(int argc, char *argv[])
{
	char *defsrv;
	int p[2];
	int stdio = 0;
	char *host = nil;
	long ncache = 1000;
	int readOnly = 1;

	defsrv = "vacfs";
	ARGBEGIN{
	case 'd':
		fmtinstall('F', fcallfmt);
		dflag = 1;
		break;
	case 'c':
		ncache = atoi(ARGF());
		break;
	case 'i':
		stdio = 1;
		mfd[0] = 0;
		mfd[1] = 1;
		break;
	case 'h':
		host = ARGF();
		break;
	case 's':
		defsrv = ARGF();
		break;
	case 'p':
		noperm = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	init(argv[0], host, ncache, readOnly);

	if(pipe(p) < 0)
		sysfatal("pipe failed: %r");

	mfd[0] = p[0];
	mfd[1] = p[0];
	proccreate(srv, 0, 32 * 1024);

	if (post9pservice(p[1], defsrv) != 0) 
		sysfatal("post9pservice");


	threadexits(0);
}

void srv(void* a) {
	io();
	shutdown();
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
		return vtstrdup("version: message size too small");
	messagesize = rhdr.msize;
	if(messagesize > sizeof mdata)
		messagesize = sizeof mdata;
	thdr.msize = messagesize;
	if(strncmp(rhdr.version, "9P2000", 6) != 0)
		return vtstrdup("unrecognized 9P version");
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
	return vtstrdup("vacfs: authentication not required");
}

char*
rattach(Fid *f)
{
	/* no authentication for the momment */
	VacFile *file;
	char err[80];

	file = vacfsgetroot(fs);
	if(file == nil) {
		rerrstr(err, sizeof err);
		return vtstrdup(err);
	}

	f->busy = 1;
	f->file = file;
	f->qid = (Qid){vacfilegetid(f->file), 0, QTDIR};
	thdr.qid = f->qid;
	if(rhdr.uname[0])
		f->user = vtstrdup(rhdr.uname);
	else
		f->user = "none";
	return 0;
}

VacFile*
_vfWalk(VacFile *file, char *name)
{
	VacFile *n;

	n = vacfilewalk(file, name);
	if(n)
		return n;
	if(strcmp(name, "SLASH") == 0)
		return vacfilewalk(file, "/");
	return nil;
}

char*
rwalk(Fid *f)
{
	VacFile *file, *nfile;
	Fid *nf;
	int nqid, nwname;
	Qid qid;
	char *err = nil;

	if(f->busy == 0)
		return Enotexist;
	nf = nil;
	if(rhdr.fid != rhdr.newfid){
		if(f->open)
			return vtstrdup(Eisopen);
		if(f->busy == 0)
			return vtstrdup(Enotexist);
		nf = newfid(rhdr.newfid);
		if(nf->busy)
			return vtstrdup(Eisopen);
		nf->busy = 1;
		nf->open = 0;
		nf->qid = f->qid;
		nf->file = vacfileincref(f->file);
		nf->user = vtstrdup(f->user);
		f = nf;
	}

	nwname = rhdr.nwname;

	/* easy case */
	if(nwname == 0) {
		thdr.nwqid = 0;
		return 0;
	}

	file = f->file;
	vacfileincref(file);
	qid = f->qid;

	for(nqid = 0; nqid < nwname; nqid++){
		if((qid.type & QTDIR) == 0){
			err = Enotdir;
			break;
		}
		if(!permf(file, f->user, Pexec)) {
			err = Eperm;
			break;
		}
		nfile = _vfWalk(file, rhdr.wname[nqid]);
		if(nfile == nil)
			break;
		vacfiledecref(file);
		file = nfile;
		qid.type = QTFILE;
		if(vacfileisdir(file))
			qid.type = QTDIR;
		qid.vers = vacfilegetmcount(file);
		qid.path = vacfilegetid(file);
		thdr.wqid[nqid] = qid;
	}

	thdr.nwqid = nqid;

	if(nqid == nwname){
		/* success */
		f->qid = thdr.wqid[nqid-1];
		vacfiledecref(f->file);
		f->file = file;
		return 0;
	}

	vacfiledecref(file);
	if(nf != nil)
		rclunk(nf);

	/* only error on the first element */
	if(nqid == 0)
		return vtstrdup(err);

	return 0;
}

char *
ropen(Fid *f)
{
	int mode, trunc;

	if(f->open)
		return vtstrdup(Eisopen);
	if(!f->busy)
		return vtstrdup(Enotexist);

	mode = rhdr.mode;
	thdr.iounit = messagesize - IOHDRSZ;
	if(f->qid.type & QTDIR){
		if(mode != OREAD)
			return vtstrdup(Eperm);
		if(!perm(f, Pread))
			return vtstrdup(Eperm);
		thdr.qid = f->qid;
		f->db = nil;
		f->open = 1;
		return 0;
	}
	if(mode & ORCLOSE)
		return vtstrdup(Erdonly);
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
		if(!perm(f, Pwrite))
			return vtstrdup(Eperm);
	if(mode==OREAD || mode==ORDWR)
		if(!perm(f, Pread))
			return vtstrdup(Eperm);
	if(mode==OEXEC)
		if(!perm(f, Pexec))
			return vtstrdup(Eperm);
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
		return vtstrdup(Eisopen);
	if(!fid->busy)
		return vtstrdup(Enotexist);
	if(fs->mode & ModeSnapshot)
		return vtstrdup(Erdonly);
	vf = fid->file;
	if(!vacfileisdir(vf))
		return vtstrdup(Enotdir);
	if(!permf(vf, fid->user, Pwrite))
		return vtstrdup(Eperm);

	mode = rhdr.perm & 0777;

	if(rhdr.perm & DMDIR){
		if((rhdr.mode & OTRUNC) || (rhdr.perm & DMAPPEND))
			return vtstrdup(Emode);
		switch(rhdr.mode & OPERM){
		default:
			return vtstrdup(Emode);
		case OEXEC:
		case OREAD:
			break;
		case OWRITE:
		case ORDWR:
			return vtstrdup(Eperm);
		}
		mode |= ModeDir;
	}
	vf = vacfilecreate(vf, rhdr.name, mode, "none");
	if(vf == nil) {
		char err[80];
		rerrstr(err, sizeof err);

		return vtstrdup(err);
	}

	vacfiledecref(fid->file);

	fid->file = vf;
	fid->qid.type = QTFILE;
	if(vacfileisdir(vf))
		fid->qid.type = QTDIR;
	fid->qid.vers = vacfilegetmcount(vf);
	fid->qid.path = vacfilegetid(vf);

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
	char err[80];
	int n;

	if(!f->busy)
		return vtstrdup(Enotexist);
	vf = f->file;
	thdr.count = 0;
	off = rhdr.offset;
	buf = thdr.data;
	cnt = rhdr.count;
	if(f->qid.type & QTDIR)
		n = vacdirread(f, buf, off, cnt);
	else
		n = vacfileread(vf, buf, cnt, off);
	if(n < 0) {
		rerrstr(err, sizeof err);
		return vtstrdup(err);
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
		return vtstrdup(Enotexist);
	vf = f->file;
	thdr.count = 0;
	off = rhdr.offset;
	buf = rhdr.data;
	cnt = rhdr.count;
	if(f->qid.type & QTDIR)
		return "file is a directory";
	thdr.count = vacfilewrite(vf, buf, cnt, off, "none");
	if(thdr.count < 0) {
		char err[80];

		rerrstr(err, sizeof err);
fprint(2, "write failed: %s\n", err);
		return vtstrdup(err);
	}
	return 0;
}

char *
rclunk(Fid *f)
{
	f->busy = 0;
	f->open = 0;
	vtfree(f->user);
	f->user = nil;
	vacfiledecref(f->file);
	f->file = nil;
	dirBufFree(f->db);
	f->db = nil;
	return 0;
}

char *
rremove(Fid *f)
{
	VacFile *vf, *vfp;
	char errbuf[80];
	char *err = nil;

	if(!f->busy)
		return vtstrdup(Enotexist);
	vf = f->file;
	vfp = vacfilegetparent(vf);

	if(!permf(vfp, f->user, Pwrite)) {
		err = Eperm;
		goto Exit;
	}

	if(!vacfileremove(vf, "none")) {
		rerrstr(errbuf, sizeof errbuf);
print("vfRemove failed: %s\n", errbuf);

		err = errbuf;
	}

Exit:
	vacfiledecref(vfp);
	rclunk(f);
	return vtstrdup(err);
}

char *
rstat(Fid *f)
{
	VacDir dir;
	static uchar statbuf[1024];

	if(!f->busy)
		return vtstrdup(Enotexist);
	vacfilegetdir(f->file, &dir);
	thdr.stat = statbuf;
	thdr.nstat = vdStat(&dir, thdr.stat, sizeof statbuf);
	vdcleanup(&dir);
	return 0;
}

char *
rwstat(Fid *f)
{
	if(!f->busy)
		return vtstrdup(Enotexist);
	return vtstrdup(Erdonly);
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

	db = vtmallocz(sizeof(DirBuf));
	db->vde = vdeopen(vf);
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
		n = vderead(db->vde, db->buf);
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
		vdcleanup(db->buf + i);
	vdeclose(db->vde);
	vtfree(db);
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
		vdcleanup(vd);
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
	f = vtmallocz(sizeof *f);
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
		if (err)
			vtfree(err);

		if(write(mfd[1], mdata, n) != n)
			sysfatal("mount write: %r");
	}
}

int
permf(VacFile *vf, char *user, int p)
{
	VacDir dir;
	ulong perm;

	if(vacfilegetdir(vf, &dir))
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
	vdcleanup(&dir);
	return 0;
Good:
	vdcleanup(&dir);
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

	fmtinstall('V', vtscorefmt);
//	fmtinstall('R', vtErrFmt);

	conn = vtdial(host);
	if(conn == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(conn) < 0)
		sysfatal("vtconnect: %r");

	fs = vacfsopen(conn, file, /*readOnly ? ModeSnapshot :*/ VtOREAD, ncache);
	if(fs == nil)
		sysfatal("vfsOpen: %r");
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

	vacfsclose(fs);
	vthangup(conn);
}

