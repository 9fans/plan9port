#include <u.h>
#include <libc.h>
#include <authsrv.h>
#include <fcall.h>
#include "tapefs.h"

Fid	*fids;
Ram	*ram;
int	mfd[2];
char	*user;
uchar	mdata[Maxbuf+IOHDRSZ];
int	messagesize = Maxbuf+IOHDRSZ;
Fcall	rhdr;
Fcall	thdr;
ulong	path;
Idmap	*uidmap;
Idmap	*gidmap;
int	replete;
int	blocksize;		/* for 32v */
int	verbose;
int	newtap;		/* tap with time in sec */

Fid *	newfid(int);
int	ramstat(Ram*, uchar*, int);
void	io(void);
void	usage(void);
int	perm(int);

char	*rflush(Fid*), *rversion(Fid*), *rauth(Fid*),
	*rattach(Fid*), *rwalk(Fid*),
	*ropen(Fid*), *rcreate(Fid*),
	*rread(Fid*), *rwrite(Fid*), *rclunk(Fid*),
	*rremove(Fid*), *rstat(Fid*), *rwstat(Fid*);

char 	*(*fcalls[Tmax])(Fid*);
void
initfcalls(void)
{
	fcalls[Tflush]=	rflush;
	fcalls[Tversion]=	rversion;
	fcalls[Tauth]=	rauth;
	fcalls[Tattach]=	rattach;
	fcalls[Twalk]=	rwalk;
	fcalls[Topen]=	ropen;
	fcalls[Tcreate]=	rcreate;
	fcalls[Tread]=	rread;
	fcalls[Twrite]=	rwrite;
	fcalls[Tclunk]=	rclunk;
	fcalls[Tremove]=	rremove;
	fcalls[Tstat]=	rstat;
	fcalls[Twstat]=	rwstat;
}

char	Eperm[] =	"permission denied";
char	Enotdir[] =	"not a directory";
char	Enoauth[] =	"tapefs: authentication not required";
char	Enotexist[] =	"file does not exist";
char	Einuse[] =	"file in use";
char	Eexist[] =	"file exists";
char	Enotowner[] =	"not owner";
char	Eisopen[] = 	"file already open for I/O";
char	Excl[] = 	"exclusive use file already open";
char	Ename[] = 	"illegal name";

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
	Ram *r;
	char *defmnt, *defsrv;
	int p[2];
	char buf[TICKREQLEN];

	fmtinstall('F', fcallfmt);
	initfcalls();

	defmnt = nil;
	defsrv = nil;
	ARGBEGIN{
	case 'm':
		defmnt = ARGF();
		break;
	case 's':
		defsrv = ARGF();
		break;
	case 'p':			/* password file */
		uidmap = getpass(ARGF());
		break;
	case 'g':			/* group file */
		gidmap = getpass(ARGF());
		break;
	case 'v':
		verbose++;

	case 'n':
		newtap++;
		break;
	default:
		usage();
	}ARGEND

	if(argc==0)
		error("no file to mount");
	user = getuser();
	if(user == nil)
		user = "dmr";
	ram = r = (Ram *)emalloc(sizeof(Ram));
	r->busy = 1;
	r->data = 0;
	r->ndata = 0;
	r->perm = DMDIR | 0775;
	r->qid.path = 0;
	r->qid.vers = 0;
	r->qid.type = QTDIR;
	r->parent = 0;
	r->child = 0;
	r->next = 0;
	r->user = user;
	r->group = user;
	r->atime = time(0);
	r->mtime = r->atime;
	r->replete = 0;
	r->name = estrdup(".");
	populate(argv[0]);
	r->replete |= replete;
	if(pipe(p) < 0)
		error("pipe failed");
	mfd[0] = mfd[1] = p[0];
	notify(notifyf);

	switch(rfork(RFFDG|RFPROC|RFNAMEG|RFNOTEG)){
	case -1:
		error("fork");
	case 0:
		close(p[1]);
		notify(notifyf);
		io();
		break;
	default:
		close(p[0]);	/* don't deadlock if child fails */
		if(post9pservice(p[1], defsrv, defmnt) < 0){
			sprint(buf, "post9pservice: %r");
			error(buf);
		}
	}
	exits(0);
}

char*
rversion(Fid *unused)
{
	Fid *f;

	USED(unused);

	if(rhdr.msize < 256)
		return "version: message too small";
	if(rhdr.msize > messagesize)
		rhdr.msize = messagesize;
	else
		messagesize = rhdr.msize;
	thdr.msize = messagesize;
	if(strncmp(rhdr.version, "9P2000", 6) != 0)
		return "unrecognized 9P version";
	thdr.version = "9P2000";

	for(f = fids; f; f = f->next)
		if(f->busy)
			rclunk(f);
	return 0;
}

char*
rauth(Fid *unused)
{
	USED(unused);

	return Enoauth;
}

char*
rflush(Fid *f)
{
	USED(f);
	return 0;
}

char*
rattach(Fid *f)
{
	/* no authentication! */
	f->busy = 1;
	f->rclose = 0;
	f->ram = ram;
	thdr.qid = f->ram->qid;
	if(rhdr.uname[0])
		f->user = strdup(rhdr.uname);
	else
		f->user = "none";
	return 0;
}

char*
rwalk(Fid *f)
{
	Fid *nf;
	Ram *r;
	char *err;
	char *name;
	Ram *dir;
	int i;

	nf = nil;
	if(f->ram->busy == 0)
		return Enotexist;
	if(f->open)
		return Eisopen;
	if(rhdr.newfid != rhdr.fid){
		nf = newfid(rhdr.newfid);
		nf->busy = 1;
		nf->open = 0;
		nf->rclose = 0;
		nf->ram = f->ram;
		nf->user = f->user;	/* no ref count; the leakage is minor */
		f = nf;
	}

	thdr.nwqid = 0;
	err = nil;
	r = f->ram;

	if(rhdr.nwname > 0){
		for(i=0; i<rhdr.nwname; i++){
			if((r->qid.type & QTDIR) == 0){
				err = Enotdir;
				break;
			}
			if(r->busy == 0){
				err = Enotexist;
				break;
			}
			r->atime = time(0);
			name = rhdr.wname[i];
			dir = r;
			if(!perm(Pexec)){
				err = Eperm;
				break;
			}
			if(strcmp(name, "..") == 0){
				r = dir->parent;
   Accept:
				if(i == MAXWELEM){
					err = "name too long";
					break;
				}
 				thdr.wqid[thdr.nwqid++] = r->qid;
				continue;
			}
			if(!dir->replete)
				popdir(dir);
			for(r=dir->child; r; r=r->next)
				if(r->busy && strcmp(name, r->name)==0)
					goto Accept;
			break;	/* file not found */
		}

		if(i==0 && err == nil)
			err = Enotexist;
	}

	if(err!=nil || thdr.nwqid<rhdr.nwname){
		if(nf){
			nf->busy = 0;
			nf->open = 0;
			nf->ram = 0;
		}
	}else if(thdr.nwqid  == rhdr.nwname)
		f->ram = r;

	return err;

}

char *
ropen(Fid *f)
{
	Ram *r;
	int mode, trunc;

	if(f->open)
		return Eisopen;
	r = f->ram;
	if(r->busy == 0)
		return Enotexist;
	if(r->perm & DMEXCL)
		if(r->open)
			return Excl;
	mode = rhdr.mode;
	if(r->qid.type & QTDIR){
		if(mode != OREAD)
			return Eperm;
		thdr.qid = r->qid;
		return 0;
	}
	if(mode & ORCLOSE)
		return Eperm;
	trunc = mode & OTRUNC;
	mode &= OPERM;
	if(mode==OWRITE || mode==ORDWR || trunc)
		if(!perm(Pwrite))
			return Eperm;
	if(mode==OREAD || mode==ORDWR)
		if(!perm(Pread))
			return Eperm;
	if(mode==OEXEC)
		if(!perm(Pexec))
			return Eperm;
	if(trunc && (r->perm&DMAPPEND)==0){
		r->ndata = 0;
		dotrunc(r);
		r->qid.vers++;
	}
	thdr.qid = r->qid;
	thdr.iounit = messagesize-IOHDRSZ;
	f->open = 1;
	r->open++;
	return 0;
}

char *
rcreate(Fid *f)
{
	USED(f);

	return Eperm;
}

char*
rread(Fid *f)
{
	int i, len;
	Ram *r;
	char *buf;
	uvlong off, end;
	int n, cnt;

	if(f->ram->busy == 0)
		return Enotexist;
	n = 0;
	thdr.count = 0;
	off = rhdr.offset;
	end = rhdr.offset + rhdr.count;
	cnt = rhdr.count;
	if(cnt > messagesize-IOHDRSZ)
		cnt = messagesize-IOHDRSZ;
	buf = thdr.data;
	if(f->ram->qid.type & QTDIR){
		if(!f->ram->replete)
			popdir(f->ram);
		for(i=0,r=f->ram->child; r!=nil && i<end; r=r->next){
			if(!r->busy)
				continue;
			len = ramstat(r, (uchar*)buf+n, cnt-n);
			if(len <= BIT16SZ)
				break;
			if(i >= off)
				n += len;
			i += len;
		}
		thdr.count = n;
		return 0;
	}
	r = f->ram;
	if(off >= r->ndata)
		return 0;
	r->atime = time(0);
	n = cnt;
	if(off+n > r->ndata)
		n = r->ndata - off;
	thdr.data = doread(r, off, n);
	thdr.count = n;
	return 0;
}

char*
rwrite(Fid *f)
{
	Ram *r;
	ulong off;
	int cnt;

	r = f->ram;
	if(dopermw(f->ram)==0)
		return Eperm;
	if(r->busy == 0)
		return Enotexist;
	off = rhdr.offset;
	if(r->perm & DMAPPEND)
		off = r->ndata;
	cnt = rhdr.count;
	if(r->qid.type & QTDIR)
		return "file is a directory";
	if(off > 100*1024*1024)		/* sanity check */
		return "write too big";
	dowrite(r, rhdr.data, off, cnt);
	r->qid.vers++;
	r->mtime = time(0);
	thdr.count = cnt;
	return 0;
}

char *
rclunk(Fid *f)
{
	if(f->open)
		f->ram->open--;
	f->busy = 0;
	f->open = 0;
	f->ram = 0;
	return 0;
}

char *
rremove(Fid *f)
{
	USED(f);
	return Eperm;
}

char *
rstat(Fid *f)
{
	if(f->ram->busy == 0)
		return Enotexist;
	thdr.nstat = ramstat(f->ram, thdr.stat, messagesize-IOHDRSZ);
	return 0;
}

char *
rwstat(Fid *f)
{
	if(f->ram->busy == 0)
		return Enotexist;
	return Eperm;
}

int
ramstat(Ram *r, uchar *buf, int nbuf)
{
	Dir dir;

	dir.name = r->name;
	dir.qid = r->qid;
	dir.mode = r->perm;
	dir.length = r->ndata;
	dir.uid = r->user;
	dir.gid = r->group;
	dir.muid = r->user;
	dir.atime = r->atime;
	dir.mtime = r->mtime;
	return convD2M(&dir, buf, nbuf);
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
		ff->open = 0;
		ff->busy = 1;
	}
	f = emalloc(sizeof *f);
	f->ram = 0;
	f->fid = fid;
	f->busy = 1;
	f->open = 0;
	f->next = fids;
	fids = f;
	return f;
}

void
io(void)
{
	char *err;
	int n, nerr;
	char buf[ERRMAX];

	errstr(buf, sizeof buf);
	for(nerr=0, buf[0]='\0'; nerr<100; nerr++){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error
		 */
		n = read9pmsg(mfd[0], mdata, sizeof mdata);
		if(n==0)
			continue;
		if(n < 0){
			if(buf[0]=='\0')
				errstr(buf, sizeof buf);
			continue;
		}
		nerr = 0;
		buf[0] = '\0';
		if(convM2S(mdata, n, &rhdr) != n)
			error("convert error in convM2S");

		if(verbose)
			fprint(2, "tapefs: <=%F\n", &rhdr);/**/

		thdr.data = (char*)mdata + IOHDRSZ;
		thdr.stat = mdata + IOHDRSZ;
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
		n = convS2M(&thdr, mdata, messagesize);
		if(n <= 0)
			error("convert error in convS2M");
		if(verbose)
			fprint(2, "tapefs: =>%F\n", &thdr);/**/
		if(write(mfd[1], mdata, n) != n)
			error("mount write");
	}
	if(buf[0]=='\0' || strstr(buf, "hungup"))
		exits("");
	fprint(2, "%s: mount read: %s\n", argv0, buf);
	exits(buf);
}

int
perm(int p)
{
	if(p==Pwrite)
		return 0;
	return 1;
}

void
error(char *s)
{
	fprint(2, "%s: %s: ", argv0, s);
	perror("");
	exits(s);
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

void *
emalloc(ulong n)
{
	void *p;
	p = mallocz(n, 1);
	if(!p)
		error("out of memory");
	return p;
}

void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(!p)
		error("out of memory");
	return p;
}

void
usage(void)
{
	fprint(2, "usage: %s [-s] [-m mountpoint]\n", argv0);
	exits("usage");
}
