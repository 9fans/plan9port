#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

static	int	sfd;

enum
{
	Nhash	= 16,
	DEBUG	= 0
};

static	Fid	*fids[Nhash];

Fid	*newfid(int);

static	Xfid*	fsysflush(Xfid*, Fid*);
static	Xfid*	fsysauth(Xfid*, Fid*);
static	Xfid*	fsysversion(Xfid*, Fid*);
static	Xfid*	fsysattach(Xfid*, Fid*);
static	Xfid*	fsyswalk(Xfid*, Fid*);
static	Xfid*	fsysopen(Xfid*, Fid*);
static	Xfid*	fsyscreate(Xfid*, Fid*);
static	Xfid*	fsysread(Xfid*, Fid*);
static	Xfid*	fsyswrite(Xfid*, Fid*);
static	Xfid*	fsysclunk(Xfid*, Fid*);
static	Xfid*	fsysremove(Xfid*, Fid*);
static	Xfid*	fsysstat(Xfid*, Fid*);
static	Xfid*	fsyswstat(Xfid*, Fid*);

Xfid* 	(*fcall[Tmax])(Xfid*, Fid*);

static void
initfcall(void)
{
	fcall[Tflush]	= fsysflush;
	fcall[Tversion]	= fsysversion;
	fcall[Tauth]	= fsysauth;
	fcall[Tattach]	= fsysattach;
	fcall[Twalk]	= fsyswalk;
	fcall[Topen]	= fsysopen;
	fcall[Tcreate]	= fsyscreate;
	fcall[Tread]	= fsysread;
	fcall[Twrite]	= fsyswrite;
	fcall[Tclunk]	= fsysclunk;
	fcall[Tremove]= fsysremove;
	fcall[Tstat]	= fsysstat;
	fcall[Twstat]	= fsyswstat;
}

char Eperm[] = "permission denied";
char Eexist[] = "file does not exist";
char Enotdir[] = "not a directory";

Dirtab dirtab[]=
{
	{ ".",			QTDIR,	Qdir,		0500|DMDIR },
	{ "acme",		QTDIR,	Qacme,	0500|DMDIR },
	{ "cons",		QTFILE,	Qcons,	0600 },
	{ "consctl",	QTFILE,	Qconsctl,	0000 },
	{ "draw",		QTDIR,	Qdraw,	0000|DMDIR },	/* to suppress graphics progs started in acme */
	{ "editout",	QTFILE,	Qeditout,	0200 },
	{ "index",		QTFILE,	Qindex,	0400 },
	{ "label",		QTFILE,	Qlabel,	0600 },
	{ "log",		QTFILE,	Qlog,	0400 },
	{ "new",		QTDIR,	Qnew,	0500|DMDIR },
	{ nil, }
};

Dirtab dirtabw[]=
{
	{ ".",			QTDIR,		Qdir,			0500|DMDIR },
	{ "addr",		QTFILE,		QWaddr,		0600 },
	{ "body",		QTAPPEND,	QWbody,		0600|DMAPPEND },
	{ "ctl",		QTFILE,		QWctl,		0600 },
	{ "data",		QTFILE,		QWdata,		0600 },
	{ "editout",	QTFILE,		QWeditout,	0200 },
	{ "errors",		QTFILE,		QWerrors,		0200 },
	{ "event",		QTFILE,		QWevent,		0600 },
	{ "rdsel",		QTFILE,		QWrdsel,		0400 },
	{ "wrsel",		QTFILE,		QWwrsel,		0200 },
	{ "tag",		QTAPPEND,	QWtag,		0600|DMAPPEND },
	{ "xdata",		QTFILE,		QWxdata,		0600 },
	{ nil, }
};

typedef struct Mnt Mnt;
struct Mnt
{
	QLock	lk;
	int		id;
	Mntdir	*md;
};

Mnt	mnt;

Xfid*	respond(Xfid*, Fcall*, char*);
int		dostat(int, Dirtab*, uchar*, int, uint);
uint	getclock(void);

char	*user = "Wile E. Coyote";
static int closing = 0;
int	messagesize = Maxblock+IOHDRSZ;	/* good start */

void	fsysproc(void *);

void
fsysinit(void)
{
	int p[2];
	char *u;

	initfcall();
	if(pipe(p) < 0)
		error("can't create pipe");
	if(post9pservice(p[0], "acme", mtpt) < 0)
		error("can't post service");
	sfd = p[1];
	fmtinstall('F', fcallfmt);
	if((u = getuser()) != nil)
		user = estrdup(u);
	proccreate(fsysproc, nil, STACK);
}

void
fsysproc(void *v)
{
	int n;
	Xfid *x;
	Fid *f;
	Fcall t;
	uchar *buf;

	threadsetname("fsysproc");

	USED(v);
	x = nil;
	for(;;){
		buf = emalloc(messagesize+UTFmax);	/* overflow for appending partial rune in xfidwrite */
		n = read9pmsg(sfd, buf, messagesize);
		if(n <= 0){
			if(closing)
				break;
			error("i/o error on server channel");
		}
		if(x == nil){
			sendp(cxfidalloc, nil);
			x = recvp(cxfidalloc);
		}
		x->buf = buf;
		if(convM2S(buf, n, &x->fcall) != n)
			error("convert error in convM2S");
		if(DEBUG)
			fprint(2, "%F\n", &x->fcall);
		if(fcall[x->fcall.type] == nil)
			x = respond(x, &t, "bad fcall type");
		else{
			switch(x->fcall.type){
			case Tversion:
			case Tauth:
			case Tflush:
				f = nil;
				break;
			case Tattach:
				f = newfid(x->fcall.fid);
				break;
			default:
				f = newfid(x->fcall.fid);
				if(!f->busy){
					x->f = f;
					x = respond(x, &t, "fid not in use");
					continue;
				}
				break;
			}
			x->f = f;
			x  = (*fcall[x->fcall.type])(x, f);
		}
	}
}

Mntdir*
fsysaddid(Rune *dir, int ndir, Rune **incl, int nincl)
{
	Mntdir *m;
	int id;

	qlock(&mnt.lk);
	id = ++mnt.id;
	m = emalloc(sizeof *m);
	m->id = id;
	m->dir =  dir;
	m->ref = 1;	/* one for Command, one will be incremented in attach */
	m->ndir = ndir;
	m->next = mnt.md;
	m->incl = incl;
	m->nincl = nincl;
	mnt.md = m;
	qunlock(&mnt.lk);
	return m;
}

void
fsysincid(Mntdir *m)
{
	qlock(&mnt.lk);
	m->ref++;
	qunlock(&mnt.lk);
}

void
fsysdelid(Mntdir *idm)
{
	Mntdir *m, *prev;
	int i;
	char buf[64];

	if(idm == nil)
		return;
	qlock(&mnt.lk);
	if(--idm->ref > 0){
		qunlock(&mnt.lk);
		return;
	}
	prev = nil;
	for(m=mnt.md; m; m=m->next){
		if(m == idm){
			if(prev)
				prev->next = m->next;
			else
				mnt.md = m->next;
			for(i=0; i<m->nincl; i++)
				free(m->incl[i]);
			free(m->incl);
			free(m->dir);
			free(m);
			qunlock(&mnt.lk);
			return;
		}
		prev = m;
	}
	qunlock(&mnt.lk);
	sprint(buf, "fsysdelid: can't find id %d\n", idm->id);
	sendp(cerr, estrdup(buf));
}

/*
 * Called only in exec.c:/^run(), from a different FD group
 */
Mntdir*
fsysmount(Rune *dir, int ndir, Rune **incl, int nincl)
{
	return fsysaddid(dir, ndir, incl, nincl);
}

void
fsysclose(void)
{
	closing = 1;
	/*
	 * apparently this is not kosher on openbsd.
	 * perhaps because fsysproc is reading from sfd right now,
	 * the close hangs indefinitely.
	close(sfd);
	 */
}

Xfid*
respond(Xfid *x, Fcall *t, char *err)
{
	int n;

	if(err){
		t->type = Rerror;
		t->ename = err;
	}else
		t->type = x->fcall.type+1;
	t->fid = x->fcall.fid;
	t->tag = x->fcall.tag;
	if(x->buf == nil)
		x->buf = emalloc(messagesize);
	n = convS2M(t, x->buf, messagesize);
	if(n <= 0)
		error("convert error in convS2M");
	if(write(sfd, x->buf, n) != n)
		error("write error in respond");
	free(x->buf);
	x->buf = nil;
	if(DEBUG)
		fprint(2, "r: %F\n", t);
	return x;
}

static
Xfid*
fsysversion(Xfid *x, Fid *f)
{
	Fcall t;

	USED(f);
	if(x->fcall.msize < 256)
		return respond(x, &t, "version: message size too small");
	messagesize = x->fcall.msize;
	t.msize = messagesize;
	if(strncmp(x->fcall.version, "9P2000", 6) != 0)
		return respond(x, &t, "unrecognized 9P version");
	t.version = "9P2000";
	return respond(x, &t, nil);
}

static
Xfid*
fsysauth(Xfid *x, Fid *f)
{
	Fcall t;

	USED(f);
	return respond(x, &t, "acme: authentication not required");
}

static
Xfid*
fsysflush(Xfid *x, Fid *f)
{
	USED(f);
	sendp(x->c, (void*)xfidflush);
	return nil;
}

static
Xfid*
fsysattach(Xfid *x, Fid *f)
{
	Fcall t;
	int id;
	Mntdir *m;
	char buf[128];

	if(strcmp(x->fcall.uname, user) != 0)
		return respond(x, &t, Eperm);
	f->busy = TRUE;
	f->open = FALSE;
	f->qid.path = Qdir;
	f->qid.type = QTDIR;
	f->qid.vers = 0;
	f->dir = dirtab;
	f->nrpart = 0;
	f->w = nil;
	t.qid = f->qid;
	f->mntdir = nil;
	id = atoi(x->fcall.aname);
	qlock(&mnt.lk);
	for(m=mnt.md; m; m=m->next)
		if(m->id == id){
			f->mntdir = m;
			m->ref++;
			break;
		}
	if(m == nil && x->fcall.aname[0]){
		snprint(buf, sizeof buf, "unknown id '%s' in attach", x->fcall.aname);
		sendp(cerr, estrdup(buf));
	}
	qunlock(&mnt.lk);
	return respond(x, &t, nil);
}

static
Xfid*
fsyswalk(Xfid *x, Fid *f)
{
	Fcall t;
	int c, i, j, id;
	Qid q;
	uchar type;
	ulong path;
	Fid *nf;
	Dirtab *d, *dir;
	Window *w;
	char *err;

	nf = nil;
	w = nil;
	if(f->open)
		return respond(x, &t, "walk of open file");
	if(x->fcall.fid != x->fcall.newfid){
		nf = newfid(x->fcall.newfid);
		if(nf->busy)
			return respond(x, &t, "newfid already in use");
		nf->busy = TRUE;
		nf->open = FALSE;
		nf->mntdir = f->mntdir;
		if(f->mntdir)
			f->mntdir->ref++;
		nf->dir = f->dir;
		nf->qid = f->qid;
		nf->w = f->w;
		nf->nrpart = 0;	/* not open, so must be zero */
		if(nf->w)
			incref(&nf->w->ref);
		f = nf;	/* walk f */
	}

	t.nwqid = 0;
	err = nil;
	dir = nil;
	id = WIN(f->qid);
	q = f->qid;

	if(x->fcall.nwname > 0){
		for(i=0; i<x->fcall.nwname; i++){
			if((q.type & QTDIR) == 0){
				err = Enotdir;
				break;
			}

			if(strcmp(x->fcall.wname[i], "..") == 0){
				type = QTDIR;
				path = Qdir;
				id = 0;
				if(w){
					winclose(w);
					w = nil;
				}
    Accept:
				if(i == MAXWELEM){
					err = "name too long";
					break;
				}
				q.type = type;
				q.vers = 0;
				q.path = QID(id, path);
				t.wqid[t.nwqid++] = q;
				continue;
			}

			/* is it a numeric name? */
			for(j=0; (c=x->fcall.wname[i][j]); j++)
				if(c<'0' || '9'<c)
					goto Regular;
			/* yes: it's a directory */
			if(w)	/* name has form 27/23; get out before losing w */
				break;
			id = atoi(x->fcall.wname[i]);
			qlock(&row.lk);
			w = lookid(id, FALSE);
			if(w == nil){
				qunlock(&row.lk);
				break;
			}
			incref(&w->ref);	/* we'll drop reference at end if there's an error */
			path = Qdir;
			type = QTDIR;
			qunlock(&row.lk);
			dir = dirtabw;
			goto Accept;

    Regular:
			if(strcmp(x->fcall.wname[i], "new") == 0){
				if(w)
					error("w set in walk to new");
				sendp(cnewwindow, nil);	/* signal newwindowthread */
				w = recvp(cnewwindow);	/* receive new window */
				incref(&w->ref);
				type = QTDIR;
				path = QID(w->id, Qdir);
				id = w->id;
				dir = dirtabw;
				goto Accept;
			}

			if(id == 0)
				d = dirtab;
			else
				d = dirtabw;
			d++;	/* skip '.' */
			for(; d->name; d++)
				if(strcmp(x->fcall.wname[i], d->name) == 0){
					path = d->qid;
					type = d->type;
					dir = d;
					goto Accept;
				}

			break;	/* file not found */
		}

		if(i==0 && err == nil)
			err = Eexist;
	}

	if(err!=nil || t.nwqid<x->fcall.nwname){
		if(nf){
			nf->busy = FALSE;
			fsysdelid(nf->mntdir);
		}
	}else if(t.nwqid  == x->fcall.nwname){
		if(w){
			f->w = w;
			w = nil;	/* don't drop the reference */
		}
		if(dir)
			f->dir = dir;
		f->qid = q;
	}

	if(w != nil)
		winclose(w);

	return respond(x, &t, err);
}

static
Xfid*
fsysopen(Xfid *x, Fid *f)
{
	Fcall t;
	int m;

	/* can't truncate anything, so just disregard */
	x->fcall.mode &= ~(OTRUNC|OCEXEC);
	/* can't execute or remove anything */
	if(x->fcall.mode==OEXEC || (x->fcall.mode&ORCLOSE))
		goto Deny;
	switch(x->fcall.mode){
	default:
		goto Deny;
	case OREAD:
		m = 0400;
		break;
	case OWRITE:
		m = 0200;
		break;
	case ORDWR:
		m = 0600;
		break;
	}
	if(((f->dir->perm&~(DMDIR|DMAPPEND))&m) != m)
		goto Deny;

	sendp(x->c, (void*)xfidopen);
	return nil;

    Deny:
	return respond(x, &t, Eperm);
}

static
Xfid*
fsyscreate(Xfid *x, Fid *f)
{
	Fcall t;

	USED(f);
	return respond(x, &t, Eperm);
}

static
int
idcmp(const void *a, const void *b)
{
	return *(int*)a - *(int*)b;
}

static
Xfid*
fsysread(Xfid *x, Fid *f)
{
	Fcall t;
	uchar *b;
	int i, id, n, o, e, j, k, *ids, nids;
	Dirtab *d, dt;
	Column *c;
	uint clock, len;
	char buf[16];

	if(f->qid.type & QTDIR){
		if(FILE(f->qid) == Qacme){	/* empty dir */
			t.data = nil;
			t.count = 0;
			respond(x, &t, nil);
			return x;
		}
		o = x->fcall.offset;
		e = x->fcall.offset+x->fcall.count;
		clock = getclock();
		b = emalloc(messagesize);
		id = WIN(f->qid);
		n = 0;
		if(id > 0)
			d = dirtabw;
		else
			d = dirtab;
		d++;	/* first entry is '.' */
		for(i=0; d->name!=nil && i<e; i+=len){
			len = dostat(WIN(x->f->qid), d, b+n, x->fcall.count-n, clock);
			if(len <= BIT16SZ)
				break;
			if(i >= o)
				n += len;
			d++;
		}
		if(id == 0){
			qlock(&row.lk);
			nids = 0;
			ids = nil;
			for(j=0; j<row.ncol; j++){
				c = row.col[j];
				for(k=0; k<c->nw; k++){
					ids = realloc(ids, (nids+1)*sizeof(int));
					ids[nids++] = c->w[k]->id;
				}
			}
			qunlock(&row.lk);
			qsort(ids, nids, sizeof ids[0], idcmp);
			j = 0;
			dt.name = buf;
			for(; j<nids && i<e; i+=len){
				k = ids[j];
				sprint(dt.name, "%d", k);
				dt.qid = QID(k, Qdir);
				dt.type = QTDIR;
				dt.perm = DMDIR|0700;
				len = dostat(k, &dt, b+n, x->fcall.count-n, clock);
				if(len == 0)
					break;
				if(i >= o)
					n += len;
				j++;
			}
			free(ids);
		}
		t.data = (char*)b;
		t.count = n;
		respond(x, &t, nil);
		free(b);
		return x;
	}
	sendp(x->c, (void*)xfidread);
	return nil;
}

static
Xfid*
fsyswrite(Xfid *x, Fid *f)
{
	USED(f);
	sendp(x->c, (void*)xfidwrite);
	return nil;
}

static
Xfid*
fsysclunk(Xfid *x, Fid *f)
{
	fsysdelid(f->mntdir);
	sendp(x->c, (void*)xfidclose);
	return nil;
}

static
Xfid*
fsysremove(Xfid *x, Fid *f)
{
	Fcall t;

	USED(f);
	return respond(x, &t, Eperm);
}

static
Xfid*
fsysstat(Xfid *x, Fid *f)
{
	Fcall t;

	t.stat = emalloc(messagesize-IOHDRSZ);
	t.nstat = dostat(WIN(x->f->qid), f->dir, t.stat, messagesize-IOHDRSZ, getclock());
	x = respond(x, &t, nil);
	free(t.stat);
	return x;
}

static
Xfid*
fsyswstat(Xfid *x, Fid *f)
{
	Fcall t;

	USED(f);
	return respond(x, &t, Eperm);
}

Fid*
newfid(int fid)
{
	Fid *f, *ff, **fh;

	ff = nil;
	fh = &fids[fid&(Nhash-1)];
	for(f=*fh; f; f=f->next)
		if(f->fid == fid)
			return f;
		else if(ff==nil && f->busy==FALSE)
			ff = f;
	if(ff){
		ff->fid = fid;
		return ff;
	}
	f = emalloc(sizeof *f);
	f->fid = fid;
	f->next = *fh;
	*fh = f;
	return f;
}

uint
getclock(void)
{
	return time(0);
}

int
dostat(int id, Dirtab *dir, uchar *buf, int nbuf, uint clock)
{
	Dir d;

	d.qid.path = QID(id, dir->qid);
	d.qid.vers = 0;
	d.qid.type = dir->type;
	d.mode = dir->perm;
	d.length = 0;	/* would be nice to do better */
	d.name = dir->name;
	d.uid = user;
	d.gid = user;
	d.muid = user;
	d.atime = clock;
	d.mtime = clock;
	return convD2M(&d, buf, nbuf);
}
