#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <errno.h>
#include "dat.h"
#include "fns.h"

enum
{
	Maxfdata	= 8192,
	Maxiosize	= IOHDRSZ+Maxfdata,
};

void io(int);
void rversion(void);
void	rattach(void);
void	rauth(void);
void	rclunk(void);
void	rcreate(void);
void	rflush(void);
void	ropen(void);
void	rread(void);
void	rremove(void);
void	rsession(void);
void	rstat(void);
void	rwalk(void);
void	rwrite(void);
void	rwstat(void);

static int	openflags(int);
static void	usage(void);

#define Reqsize (sizeof(Fcall)+Maxfdata)

Fcall *req;
Fcall *rep;

uchar mdata[Maxiosize];
char fdata[Maxfdata];
uchar statbuf[STATMAX];


extern Xfsub	*xsublist[];
extern int	nclust;

jmp_buf	err_lab[16];
int	nerr_lab;
char	err_msg[ERRMAX];

int	chatty;
int	nojoliet;
int	noplan9;
int norock;

void    (*fcalls[Tmax])(void);

static void
initfcalls(void)
{
	fcalls[Tversion]=	rversion;
	fcalls[Tflush]=	rflush;
	fcalls[Tauth]=	rauth;
	fcalls[Tattach]=	rattach;
	fcalls[Twalk]=		rwalk;
	fcalls[Topen]=		ropen;
	fcalls[Tcreate]=	rcreate;
	fcalls[Tread]=		rread;
	fcalls[Twrite]=	rwrite;
	fcalls[Tclunk]=	rclunk;
	fcalls[Tremove]=	rremove;
	fcalls[Tstat]=		rstat;
	fcalls[Twstat]=	rwstat;
}

void
main(int argc, char **argv)
{
	int srvfd, pipefd[2], stdio;
	Xfsub **xs;
	char *mtpt;

	initfcalls();
	stdio = 0;
	mtpt = nil;
	ARGBEGIN {
	case '9':
		noplan9 = 1;
		break;
	case 'c':
		nclust = atoi(EARGF(usage()));
		if (nclust <= 0)
			sysfatal("nclust %d non-positive", nclust);
		break;
	case 'f':
		deffile = EARGF(usage());
		break;
	case 'r':
		norock = 1;
		break;
	case 's':
		stdio = 1;
		break;
	case 'v':
		chatty = 1;
		break;
	case 'J':
		nojoliet = 1;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	switch(argc) {
	case 0:
		break;
	case 1:
		srvname = argv[0];
		break;
	default:
		usage();
	}

	iobuf_init();
	for(xs=xsublist; *xs; xs++)
		(*(*xs)->reset)();

	if(stdio) {
		pipefd[0] = 0;
		pipefd[1] = 1;
	} else {
		close(0);
		close(1);
		open("/dev/null", OREAD);
		open("/dev/null", OWRITE);
		if(pipe(pipefd) < 0)
			panic(1, "pipe");

		if(post9pservice(pipefd[0], srvname, mtpt) < 0)
			sysfatal("post9pservice: %r");
		close(pipefd[0]);
	}
	srvfd = pipefd[1];

	switch(rfork(RFNOWAIT|RFNOTEG|RFFDG|RFPROC)){
	case -1:
		panic(1, "fork");
	default:
		_exits(0);
	case 0:
		break;
	}

	io(srvfd);
	exits(0);
}

void
io(int srvfd)
{
	int n, pid;
	Fcall xreq, xrep;

	req = &xreq;
	rep = &xrep;
	pid = getpid();
	fmtinstall('F', fcallfmt);

	for(;;){
		/*
		 * reading from a pipe or a network device
		 * will give an error after a few eof reads.
		 * however, we cannot tell the difference
		 * between a zero-length read and an interrupt
		 * on the processes writing to us,
		 * so we wait for the error.
		 */
		n = read9pmsg(srvfd, mdata, sizeof mdata);
		if(n < 0)
			break;
		if(n == 0)
			continue;
		if(convM2S(mdata, n, req) == 0)
			continue;

		if(chatty)
			fprint(2, "9660srv %d:<-%F\n", pid, req);

		errno = 0;
		if(!waserror()){
			err_msg[0] = 0;
			if(req->type >= nelem(fcalls) || !fcalls[req->type])
				error("bad fcall type");
			(*fcalls[req->type])();
			poperror();
		}

		if(err_msg[0]){
			rep->type = Rerror;
			rep->ename = err_msg;
		}else{
			rep->type = req->type + 1;
			rep->fid = req->fid;
		}
		rep->tag = req->tag;

		if(chatty)
			fprint(2, "9660srv %d:->%F\n", pid, rep);
		n = convS2M(rep, mdata, sizeof mdata);
		if(n == 0)
			panic(1, "convS2M error on write");
		if(write(srvfd, mdata, n) != n)
			panic(1, "mount write");
		if(nerr_lab != 0)
			panic(0, "err stack %d");
	}
	chat("server shut down");
}

static void
usage(void)
{
	fprint(2, "usage: %s [-v] [-9Jr] [-s] [-f devicefile] [srvname]\n", argv0);
	exits("usage");
}

void
error(char *p)
{
	strecpy(err_msg, err_msg+sizeof err_msg, p);
	nexterror();
}

void
nexterror(void)
{
	longjmp(err_lab[--nerr_lab], 1);
}

void*
ealloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == 0)
		error("no memory");
	return p;
}

void
setnames(Dir *d, char *n)
{
	d->name = n;
	d->uid = n+Maxname;
	d->gid = n+Maxname*2;
	d->muid = n+Maxname*3;

	d->name[0] = '\0';
	d->uid[0] = '\0';
	d->gid[0] = '\0';
	d->muid[0] = '\0';
}

void
rversion(void)
{
	if(req->msize > Maxiosize)
		rep->msize = Maxiosize;
	else
		rep->msize = req->msize;
	rep->version = "9P2000";
}

void
rauth(void)
{
	error("9660srv: authentication not required");
}

void
rflush(void)
{
}

void
rattach(void)
{
	Xfs *xf;
	Xfile *root;
	Xfsub **xs;

	chat("attach(fid=%d,uname=\"%s\",aname=\"%s\")...",
		req->fid, req->uname, req->aname);

	if(waserror()){
		xfile(req->fid, Clunk);
		nexterror();
	}
	root = xfile(req->fid, Clean);
	root->qid = (Qid){0, 0, QTDIR};
	root->xf = xf = ealloc(sizeof(Xfs));
	memset(xf, 0, sizeof(Xfs));
	xf->ref = 1;
	xf->d = getxdata(req->aname);

	for(xs=xsublist; *xs; xs++)
		if((*(*xs)->attach)(root) >= 0){
			poperror();
			xf->s = *xs;
			xf->rootqid = root->qid;
			rep->qid = root->qid;
			return;
		}
	error("unknown format");
}

Xfile*
doclone(Xfile *of, int newfid)
{
	Xfile *nf, *next;

	nf = xfile(newfid, Clean);
	if(waserror()){
		xfile(newfid, Clunk);
		nexterror();
	}
	next = nf->next;
	*nf = *of;
	nf->next = next;
	nf->fid = newfid;
	refxfs(nf->xf, 1);
	if(nf->len){
		nf->ptr = ealloc(nf->len);
		memmove(nf->ptr, of->ptr, nf->len);
	}else
		nf->ptr = of->ptr;
	(*of->xf->s->clone)(of, nf);
	poperror();
	return nf;
}

void
rwalk(void)
{
	Xfile *f, *nf;
	Isofile *oldptr;
	int oldlen;
	Qid oldqid;

	rep->nwqid = 0;
	nf = nil;
	f = xfile(req->fid, Asis);
	if(req->fid != req->newfid)
		f = nf = doclone(f, req->newfid);

	/* save old state in case of error */
	oldqid = f->qid;
	oldlen = f->len;
	oldptr = f->ptr;
	if(oldlen){
		oldptr = ealloc(oldlen);
		memmove(oldptr, f->ptr, oldlen);
	}

	if(waserror()){
		if(nf != nil)
			xfile(req->newfid, Clunk);
		if(rep->nwqid == req->nwname){
			if(oldlen)
				free(oldptr);
		}else{
			/* restore previous state */
			f->qid = oldqid;
			if(f->len)
				free(f->ptr);
			f->ptr = oldptr;
			f->len = oldlen;
		}
		if(rep->nwqid==req->nwname || rep->nwqid > 0){
			err_msg[0] = '\0';
			return;
		}
		nexterror();
	}

	for(rep->nwqid=0; rep->nwqid < req->nwname && rep->nwqid < MAXWELEM; rep->nwqid++){
		chat("\twalking %s\n", req->wname[rep->nwqid]);
		if(!(f->qid.type & QTDIR)){
			chat("\tnot dir: type=%#x\n", f->qid.type);
			error("walk in non-directory");
		}

		if(strcmp(req->wname[rep->nwqid], "..")==0){
			if(f->qid.path != f->xf->rootqid.path)
				(*f->xf->s->walkup)(f);
		}else
			(*f->xf->s->walk)(f, req->wname[rep->nwqid]);
		rep->wqid[rep->nwqid] = f->qid;
	}
	poperror();
	if(oldlen)
		free(oldptr);
}

void
ropen(void)
{
	Xfile *f;

	f = xfile(req->fid, Asis);
	if(f->flags&Omodes)
		error("open on open file");
	if(req->mode&ORCLOSE)
		error("no removes");
	(*f->xf->s->open)(f, req->mode);
	f->flags = openflags(req->mode);
	rep->qid = f->qid;
	rep->iounit = 0;
}

void
rcreate(void)
{
	error("no creates");
/*
	Xfile *f;

	if(strcmp(req->name, ".") == 0 || strcmp(req->name, "..") == 0)
		error("create . or ..");
	f = xfile(req->fid, Asis);
	if(f->flags&Omodes)
		error("create on open file");
	if(!(f->qid.path&CHDIR))
		error("create in non-directory");
	(*f->xf->s->create)(f, req->name, req->perm, req->mode);
	chat("f->qid=0x%8.8lux...", f->qid.path);
	f->flags = openflags(req->mode);
	rep->qid = f->qid;
*/
}

void
rread(void)
{
	Xfile *f;

	f=xfile(req->fid, Asis);
	if (!(f->flags&Oread))
		error("file not opened for reading");
	if(f->qid.type & QTDIR)
		rep->count = (*f->xf->s->readdir)(f, (uchar*)fdata, req->offset, req->count);
	else
		rep->count = (*f->xf->s->read)(f, fdata, req->offset, req->count);
	rep->data = fdata;
}

void
rwrite(void)
{
	Xfile *f;

	f=xfile(req->fid, Asis);
	if(!(f->flags&Owrite))
		error("file not opened for writing");
	rep->count = (*f->xf->s->write)(f, req->data, req->offset, req->count);
}

void
rclunk(void)
{
	Xfile *f;

	if(!waserror()){
		f = xfile(req->fid, Asis);
		(*f->xf->s->clunk)(f);
		poperror();
	}
	xfile(req->fid, Clunk);
}

void
rremove(void)
{
	error("no removes");
}

void
rstat(void)
{
	Xfile *f;
	Dir dir;

	chat("stat(fid=%d)...", req->fid);
	f=xfile(req->fid, Asis);
	setnames(&dir, fdata);
	(*f->xf->s->stat)(f, &dir);
	if(chatty)
		showdir(2, &dir);
	rep->nstat = convD2M(&dir, statbuf, sizeof statbuf);
	rep->stat = statbuf;
}

void
rwstat(void)
{
	error("no wstat");
}

static int
openflags(int mode)
{
	int flags = 0;

	switch(mode & ~(OTRUNC|OCEXEC|ORCLOSE)){
	case OREAD:
	case OEXEC:
		flags = Oread; break;
	case OWRITE:
		flags = Owrite; break;
	case ORDWR:
		flags = Oread|Owrite; break;
	}
	if(mode & ORCLOSE)
		flags |= Orclose;
	return flags;
}

void
showdir(int fd, Dir *s)
{
	char a_time[32], m_time[32];
	char *p;

	strcpy(a_time, ctime(s->atime));
	if(p=strchr(a_time, '\n'))	/* assign = */
		*p = 0;
	strcpy(m_time, ctime(s->mtime));
	if(p=strchr(m_time, '\n'))	/* assign = */
		*p = 0;
	fprint(fd, "name=\"%s\" qid=(0x%llux,%lud) type=%d dev=%d \
mode=0x%8.8lux=0%luo atime=%s mtime=%s length=%lld uid=\"%s\" gid=\"%s\"...",
		s->name, s->qid.path, s->qid.vers, s->type, s->dev,
		s->mode, s->mode,
		a_time, m_time, s->length, s->uid, s->gid);
}

#define	SIZE	1024

void
chat(char *fmt, ...)
{
	va_list arg;

	if(chatty){
		va_start(arg, fmt);
		vfprint(2, fmt, arg);
		va_end(arg);
	}
}

void
panic(int rflag, char *fmt, ...)
{
	va_list arg;
	char buf[SIZE]; int n;

	n = sprint(buf, "%s %d: ", argv0, getpid());
	va_start(arg, fmt);
	vseprint(buf+n, buf+SIZE, fmt, arg);
	va_end(arg);
	fprint(2, (rflag ? "%s: %r\n" : "%s\n"), buf);
	if(chatty){
		fprint(2, "abort\n");
		abort();
	}
	exits("panic");
}
