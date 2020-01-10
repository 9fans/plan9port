#include "std.h"
#include "dat.h"

enum
{
	Qroot,
	Qfactotum,
	Qrpc,
	Qkeylist,
	Qprotolist,
	Qconfirm,
	Qlog,
	Qctl,
	Qneedkey,
	Qconv
};

static int qtop;

Qid
mkqid(int type, int path)
{
	Qid q;

	q.type = type;
	q.path = path;
	q.vers = 0;
	return q;
}

static struct
{
	char *name;
	int qidpath;
	ulong perm;
} dirtab[] = {
	/* positions of confirm and needkey known below */
	"confirm",		Qconfirm,		0600|DMEXCL,
	"needkey",	Qneedkey,	0600|DMEXCL,
	"ctl",			Qctl,			0600,
	"rpc",		Qrpc,		0666,
	"proto",		Qprotolist,	0444,
	"log",		Qlog,		0600|DMEXCL,
	"conv",		Qconv,		0400
};

static void
fillstat(Dir *dir, char *name, int type, int path, ulong perm)
{
	dir->name = estrdup(name);
	dir->uid = estrdup(owner);
	dir->gid = estrdup(owner);
	dir->mode = perm;
	dir->length = 0;
	dir->qid = mkqid(type, path);
	dir->atime = time(0);
	dir->mtime = time(0);
	dir->muid = estrdup("");
}

static int
rootdirgen(int n, Dir *dir, void *v)
{
	USED(v);

	if(n > 0)
		return -1;

	fillstat(dir, factname, QTDIR, Qfactotum, DMDIR|0555);
	return 0;
}

static int
fsdirgen(int n, Dir *dir, void *v)
{
	USED(v);

	if(n >= nelem(dirtab))
		return -1;
	fillstat(dir, dirtab[n].name, 0, dirtab[n].qidpath, dirtab[n].perm);
	return 0;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	int i;

	switch((int)fid->qid.path){
	default:
		return "fswalk1: cannot happen";
	case Qroot:
		if(strcmp(name, factname) == 0){
			*qid = mkqid(QTDIR, Qfactotum);
			fid->qid = *qid;
			return nil;
		}
		if(strcmp(name, "..") == 0){
			*qid = fid->qid;
			return nil;
		}
		return "not found";
	case Qfactotum:
		for(i=0; i<nelem(dirtab); i++)
			if(strcmp(name, dirtab[i].name) == 0){
				*qid = mkqid(0, dirtab[i].qidpath);
				fid->qid = *qid;
				return nil;
			}
		if(strcmp(name, "..") == 0){
			*qid = mkqid(QTDIR, qtop);
			fid->qid = *qid;
			return nil;
		}
		return "not found";
	}
}

static void
fsstat(Req *r)
{
	int i, path;

	path = r->fid->qid.path;
	switch(path){
	case Qroot:
		fillstat(&r->d, "/", QTDIR, Qroot, 0555|DMDIR);
		break;
	case Qfactotum:
		fillstat(&r->d, "factotum", QTDIR, Qfactotum, 0555|DMDIR);
		break;
	default:
		for(i=0; i<nelem(dirtab); i++)
			if(dirtab[i].qidpath == path){
				fillstat(&r->d, dirtab[i].name, 0, dirtab[i].qidpath, dirtab[i].perm);
				goto Break2;
			}
		respond(r, "file not found");
		break;
	}
    Break2:
	respond(r, nil);
}

static int
readlist(int off, int (*gen)(int, char*, uint), Req *r)
{
	char *a, *ea;
	int n;

	a = r->ofcall.data;
	ea = a+r->ifcall.count;
	for(;;){
		n = (*gen)(off, a, ea-a);
		if(n == 0){
			r->ofcall.count = a - (char*)r->ofcall.data;
			return off;
		}
		a += n;
		off++;
	}
	/* not reached */
}

static int
keylist(int i, char *a, uint nn)
{
	int n;
	char buf[4096];
	Key *k;

	if(i >= ring.nkey)
		return 0;

	k = ring.key[i];
	k->attr = sortattr(k->attr);
	n = snprint(buf, sizeof buf, "key %A %N\n", k->attr, k->privattr);
	if(n >= sizeof(buf)-5)
		strcpy(buf+sizeof(buf)-5, "...\n");
	n = strlen(buf);
	if(n > nn)
		return 0;
	memmove(a, buf, n);
	return n;
}

static int
protolist(int i, char *a, uint n)
{
	if(prototab[i] == nil)
		return 0;
	if(strlen(prototab[i]->name)+1 > n)
		return 0;
	n = strlen(prototab[i]->name)+1;
	memmove(a, prototab[i]->name, n-1);
	a[n-1] = '\n';
	return n;
}

/* BUG this is O(n^2) to fill in the list */
static int
convlist(int i, char *a, uint nn)
{
	Conv *c;
	char buf[512];
	int n;

	for(c=conv; c && i-- > 0; c=c->next)
		;

	if(c == nil)
		return 0;

	if(c->state)
		n = snprint(buf, sizeof buf, "conv state=%q %A\n", c->state, c->attr);
	else
		n = snprint(buf, sizeof buf, "conv state=closed err=%q\n", c->err);

	if(n >= sizeof(buf)-5)
		strcpy(buf+sizeof(buf)-5, "...\n");
	n = strlen(buf);
	if(n > nn)
		return 0;
	memmove(a, buf, n);
	return n;
}

static void
fskickreply(Conv *c)
{
	Req *r;

	if(c->hangup){
		if((r = c->req) != nil){
			c->req = nil;
			respond(r, "hangup");
		}
		return;
	}

	if(!c->req || !c->nreply)
		return;

	r = c->req;
	r->ofcall.count = c->nreply;
	r->ofcall.data = c->reply;
	if(r->ofcall.count > r->ifcall.count)
		r->ofcall.count = r->ifcall.count;
	c->req = nil;
	respond(r, nil);
	c->nreply = 0;
}

/*
 * Some of the file system work happens in the fs proc, but
 * fsopen, fsread, fswrite, fsdestroyfid, and fsflush happen in
 * the main proc so that they can access the various shared
 * data structures without worrying about locking.
 */
static int inuse[nelem(dirtab)];
int *confirminuse = &inuse[0];
int *needkeyinuse = &inuse[1];
static void
fsopen(Req *r)
{
	int i, *inusep, perm;
	static int need[4] = { 4, 2, 6, 1 };
	Conv *c;

	inusep = nil;
	perm = 5;	/* directory */
	for(i=0; i<nelem(dirtab); i++)
		if(dirtab[i].qidpath == r->fid->qid.path){
			if(dirtab[i].perm & DMEXCL)
				inusep = &inuse[i];
			if(strcmp(r->fid->uid, owner) == 0)
				perm = dirtab[i].perm>>6;
			else
				perm = dirtab[i].perm;
			break;
		}

	if((r->ifcall.mode&~(OMASK|OTRUNC))
	|| (need[r->ifcall.mode&3] & ~perm)){
		respond(r, "permission denied");
		return;
	}

	if(inusep){
		if(*inusep){
			respond(r, "file in use");
			return;
		}
		*inusep = 1;
	}

	if(r->fid->qid.path == Qrpc){
		if((c = convalloc(r->fid->uid)) == nil){
			char e[ERRMAX];

			rerrstr(e, sizeof e);
			respond(r, e);
			return;
		}
		c->kickreply = fskickreply;
		r->fid->aux = c;
	}

	respond(r, nil);
}

static void
fsread(Req *r)
{
	Conv *c;

	switch((int)r->fid->qid.path){
	default:
		respond(r, "fsread: cannot happen");
		break;
	case Qroot:
		dirread9p(r, rootdirgen, nil);
		respond(r, nil);
		break;
	case Qfactotum:
		dirread9p(r, fsdirgen, nil);
		respond(r, nil);
		break;
	case Qrpc:
		c = r->fid->aux;
		if(c->rpc.op == RpcUnknown){
			respond(r, "no rpc pending");
			break;
		}
		if(c->req){
			respond(r, "read already pending");
			break;
		}
		c->req = r;
		if(c->nreply)
			(*c->kickreply)(c);
		else
			rpcexec(c);
		break;
	case Qconfirm:
		confirmread(r);
		break;
	case Qlog:
		logread(r);
		break;
	case Qctl:
		r->fid->aux = (void*)(uintptr)readlist((uintptr)r->fid->aux, keylist, r);
		respond(r, nil);
		break;
	case Qneedkey:
		needkeyread(r);
		break;
	case Qprotolist:
		r->fid->aux = (void*)(uintptr)readlist((uintptr)r->fid->aux, protolist, r);
		respond(r, nil);
		break;
	case Qconv:
		r->fid->aux = (void*)(uintptr)readlist((uintptr)r->fid->aux, convlist, r);
		respond(r, nil);
		break;
	}
}

static void
fswrite(Req *r)
{
	int ret;
	char err[ERRMAX], *s;
	int (*strfn)(char*);
	char *name;

	switch((int)r->fid->qid.path){
	default:
		respond(r, "fswrite: cannot happen");
		break;
	case Qrpc:
		if(rpcwrite(r->fid->aux, r->ifcall.data, r->ifcall.count) < 0){
			rerrstr(err, sizeof err);
			respond(r, err);
		}else{
			r->ofcall.count = r->ifcall.count;
			respond(r, nil);
		}
		break;
	case Qneedkey:
		name = "needkey";
		strfn = needkeywrite;
		goto string;
	case Qctl:
		name = "ctl";
		strfn = ctlwrite;
		goto string;
	case Qconfirm:
		name = "confirm";
		strfn = confirmwrite;
	string:
		s = emalloc(r->ifcall.count+1);
		memmove(s, r->ifcall.data, r->ifcall.count);
		s[r->ifcall.count] = '\0';
		ret = (*strfn)(s);
		free(s);
		if(ret < 0){
			rerrstr(err, sizeof err);
			respond(r, err);
			flog("write %s: %s", name, err);
		}else{
			r->ofcall.count = r->ifcall.count;
			respond(r, nil);
		}
		break;
	}
}

static void
fsflush(Req *r)
{
	confirmflush(r->oldreq);
	logflush(r->oldreq);
	respond(r, nil);
}

static void
fsdestroyfid(Fid *fid)
{
	if(fid->qid.path == Qrpc && fid->aux){
		convhangup(fid->aux);
		convclose(fid->aux);
	}
}

static Channel *creq;
static Channel *cfid, *cfidr;

static void
fsreqthread(void *v)
{
	Req *r;

	USED(v);

	while((r = recvp(creq)) != nil){
		switch(r->ifcall.type){
		default:
			respond(r, "bug in fsreqthread");
			break;
		case Topen:
			fsopen(r);
			break;
		case Tread:
			fsread(r);
			break;
		case Twrite:
			fswrite(r);
			break;
		case Tflush:
			fsflush(r);
			break;
		}
	}
}

static void
fsclunkthread(void *v)
{
	Fid *f;

	USED(v);

	while((f = recvp(cfid)) != nil){
		fsdestroyfid(f);
		sendp(cfidr, 0);
	}
}

static void
fsproc(void *v)
{
	USED(v);

	threadcreate(fsreqthread, nil, STACK);
	threadcreate(fsclunkthread, nil, STACK);
	threadexits(nil);
}

static void
fsattach(Req *r)
{
	r->fid->qid = mkqid(QTDIR, qtop);
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static void
fssend(Req *r)
{
	sendp(creq, r);
}

static void
fssendclunk(Fid *f)
{
	sendp(cfid, f);
	recvp(cfidr);
}

void
fsstart(Srv *s)
{
	USED(s);

	if(extrafactotumdir)
		qtop = Qroot;
	else
		qtop = Qfactotum;
	creq = chancreate(sizeof(Req*), 0);
	cfid = chancreate(sizeof(Fid*), 0);
	cfidr = chancreate(sizeof(Fid*), 0);
	proccreate(fsproc, nil, STACK);
}

Srv fs;

void
fsinit0(void)
{
	fs.attach = fsattach;
	fs.walk1 = fswalk1;
	fs.open = fssend;
	fs.read = fssend;
	fs.write = fssend;
	fs.stat = fsstat;
	fs.flush = fssend;
	fs.destroyfid = fssendclunk;
	fs.start = fsstart;
}
