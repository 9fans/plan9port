#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>

static void
usage(void)
{
	fprint(2, "mntgen [-s srvname] [mtpt]\n");
	threadexits("usage");
}

ulong time0;

typedef struct Tab Tab;
struct Tab
{
	char *name;
	vlong qid;
	ulong time;
	int ref;
};

Tab *tab;
int ntab;
int mtab;

static Tab*
findtab(vlong path)
{
	int i;

	for(i=0; i<ntab; i++)
		if(tab[i].qid == path)
			return &tab[i];
	return nil;
}

static vlong
hash(char *name)
{
	vlong digest[MD5dlen / sizeof(vlong) + 1];
	md5((uchar *)name, strlen(name), (uchar *)digest, nil);
	return digest[0] & ((1ULL<<48)-1);
}

static void
fsopen(Req *r)
{
	if(r->ifcall.mode != OREAD)
		respond(r, "permission denied");
	else
		respond(r, nil);
}

static int
dirgen(int i, Dir *d, void* v)
{
	USED(v);

	if(i >= ntab)
		return -1;
	memset(d, 0, sizeof *d);
	d->qid.type = QTDIR;
	d->uid = estrdup9p("sys");
	d->gid = estrdup9p("sys");
	d->mode = DMDIR|0555;
	d->length = 0;
	if(i == -1){
		d->name = estrdup9p("/");
		d->atime = d->mtime = time0;
	}else{
		d->qid.path = tab[i].qid;
		d->name = estrdup9p(tab[i].name);
		d->atime = d->mtime = tab[i].time;
	}
	return 0;
}

static void
fsread(Req *r)
{
	if(r->fid->qid.path == 0)
		dirread9p(r, dirgen, nil);
	else
		r->ofcall.count = 0;
	respond(r, nil);
}

static void
fsstat(Req *r)
{
	Tab *t;
	vlong qid;

	qid = r->fid->qid.path;
	if(qid == 0)
		dirgen(-1, &r->d, nil);
	else{
		if((t = findtab(qid)) == nil){
			respond(r, "path not found ( ??? )");
			return;
		}
		dirgen(t-tab, &r->d, nil);
	}
	respond(r, nil);
}

static char*
fswalk1(Fid *fid, char *name, void* v)
{
	int i;
	Tab *t;
	vlong h;

	USED(v);

	if(fid->qid.path != 0){
		/* nothing in child directory */
		if(strcmp(name, "..") == 0){
			if((t = findtab(fid->qid.path)) != nil)
				t->ref--;
			fid->qid.path = 0;
			return nil;
		}
		return "path not found";
	}
	/* root */
	if(strcmp(name, "..") == 0)
		return nil;
	for(i=0; i<ntab; i++)
		if(strcmp(tab[i].name, name) == 0){
			tab[i].ref++;
			fid->qid.path = tab[i].qid;
			return nil;
		}
	h = hash(name);
	if(findtab(h) != nil)
		return "hash collision";

	/* create it */
	if(ntab == mtab){
		if(mtab == 0)
			mtab = 16;
		else
			mtab *= 2;
		tab = erealloc9p(tab, sizeof(tab[0])*mtab);
	}
	tab[ntab].qid = h;
	fid->qid.path = tab[ntab].qid;
	tab[ntab].name = estrdup9p(name);
	tab[ntab].time = time(0);
	tab[ntab].ref = 1;
	ntab++;

	return nil;
}

static char*
fsclone(Fid *fid, Fid* v, void* w)
{
	USED(v);
	USED(w);

	Tab *t;

	if((t = findtab(fid->qid.path)) != nil)
		t->ref++;
	return nil;
}

static void
fswalk(Req *r)
{
	walkandclone(r, fswalk1, fsclone, nil);
}

static void
fsclunk(Fid *fid)
{
	Tab *t;
	vlong qid;

	qid = fid->qid.path;
	if(qid == 0)
		return;
	if((t = findtab(qid)) == nil){
		fprint(2, "warning: cannot find %llux\n", qid);
		return;
	}
	if(--t->ref == 0){
		free(t->name);
		tab[t-tab] = tab[--ntab];
	}else if(t->ref < 0)
		fprint(2, "warning: negative ref count for %s\n", t->name);
}

static void
fsattach(Req *r)
{
	char *spec;

	spec = r->ifcall.aname;
	if(spec && spec[0]){
		respond(r, "invalid attach specifier");
		return;
	}
	
	r->ofcall.qid = (Qid){0, 0, QTDIR};
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

Srv fs=
{
.attach=	fsattach,
.open=	fsopen,
.read=	fsread,
.stat=	fsstat,
.walk=	fswalk,
.destroyfid=	fsclunk
};

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	char *service;

	time0 = time(0);
	service = nil;
	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 's':
		service = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();
	threadpostmountsrv(&fs, service, argc ? argv[0] : "/n", MAFTER);
	threadexits(nil);
}

