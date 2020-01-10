#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>
#include <venti.h>
#include <libsec.h>

#undef stime
#define stime configstime	/* sometimes in <time.h> */
typedef struct Entry Entry;
struct Entry
{
	Entry *parent;
	Entry *nextdir;
	Entry *nexthash;
	Entry *kids;
	int isfsys;
	Fsys *fsys;
	uchar score[VtScoreSize];	/* of fsys */
	char *name;
	uchar sha1[VtScoreSize];	/* of path to this entry */
	ulong time;
};

typedef struct Config Config;
struct Config
{
	VtCache *vcache;
	Entry *root;
	Entry *hash[1024];
	Qid qid;
};

Config *config;
static	ulong 	mtime;	/* mod time */
static	ulong 	stime;	/* sync time */
static	char*	configfile;

static int addpath(Config*, char*, uchar[VtScoreSize], ulong);
Fsys fsysconfig;

static void
freeconfig(Config *c)
{
	Entry *next, *e;
	int i;

	for(i=0; i<nelem(c->hash); i++){
		for(e=c->hash[i]; e; e=next){
			next = e->nexthash;
			free(e);
		}
	}
	free(c);
}

static int
namehash(uchar *s)
{
	return (s[0]<<2)|(s[1]>>6);
}

static Entry*
entrybyhandle(Nfs3Handle *h)
{
	int hh;
	Entry *e;

	hh = namehash(h->h);
	for(e=config->hash[hh]; e; e=e->nexthash)
		if(memcmp(e->sha1, h->h, VtScoreSize) == 0)
			return e;
	return nil;
}

static Config*
readconfigfile(char *name, VtCache *vcache)
{
	char *p, *pref, *f[10];
	int ok;
	Config *c;
	uchar score[VtScoreSize];
	int h, nf, line;
	Biobuf *b;
	Dir *dir;

	configfile = vtstrdup(name);

	if((dir = dirstat(name)) == nil)
		return nil;

	if((b = Bopen(name, OREAD)) == nil){
		free(dir);
		return nil;
	}

	line = 0;
	ok = 1;
	c = emalloc(sizeof(Config));
	c->vcache = vcache;
	c->qid = dir->qid;
	free(dir);
	c->root = emalloc(sizeof(Entry));
	c->root->name = "/";
	c->root->parent = c->root;
	sha1((uchar*)"/", 1, c->root->sha1, nil);
	h = namehash(c->root->sha1);
	c->hash[h] = c->root;

	for(; (p = Brdstr(b, '\n', 1)) != nil; free(p)){
		line++;
		if(p[0] == '#')
			continue;
		nf = tokenize(p, f, nelem(f));
		if(nf != 3){
			fprint(2, "%s:%d: syntax error\n", name, line);
			/* ok = 0; */
			continue;
		}
		if(vtparsescore(f[1], &pref, score) < 0){
			fprint(2, "%s:%d: bad score '%s'\n", name, line, f[1]);
			/* ok = 0; */
			continue;
		}
		if(f[0][0] != '/'){
			fprint(2, "%s:%d: unrooted path '%s'\n", name, line, f[0]);
			/* ok = 0; */
			continue;
		}
		if(addpath(c, f[0], score, strtoul(f[2], 0, 0)) < 0){
			fprint(2, "%s:%d: %s: %r\n", name, line, f[0]);
			/* ok = 0; */
			continue;
		}
	}
	Bterm(b);

	if(!ok){
		freeconfig(c);
		return nil;
	}

	return c;
}

static void
refreshconfig(void)
{
	ulong now;
	Config *c, *old;
	Dir *d;

	now = time(0);
	if(now - stime < 60)
		return;
	if((d = dirstat(configfile)) == nil)
		return;
	if(d->mtime == mtime){
		free(d);
		stime = now;
		return;
	}

	c = readconfigfile(configfile, config->vcache);
	if(c == nil){
		free(d);
		return;
	}

	old = config;
	config = c;
	stime = now;
	mtime = d->mtime;
	free(d);
	freeconfig(old);
}

static Entry*
entrylookup(Entry *e, char *p, int np)
{
	for(e=e->kids; e; e=e->nextdir)
		if(strlen(e->name) == np && memcmp(e->name, p, np) == 0)
			return e;
	return nil;
}

static Entry*
walkpath(Config *c, char *name)
{
	Entry *e, *ee;
	char *p, *nextp;
	int h;

	e = c->root;
	p = name;
	for(; *p; p=nextp){
		assert(*p == '/');
		p++;
		nextp = strchr(p, '/');
		if(nextp == nil)
			nextp = p+strlen(p);
		if(e->fsys){
			werrstr("%.*s is already a mount point", utfnlen(name, nextp-name), name);
			return nil;
		}
		if((ee = entrylookup(e, p, nextp-p)) == nil){
			ee = emalloc(sizeof(Entry)+(nextp-p)+1);
			ee->parent = e;
			ee->nextdir = e->kids;
			e->kids = ee;
			ee->name = (char*)&ee[1];
			memmove(ee->name, p, nextp-p);
			ee->name[nextp-p] = 0;
			sha1((uchar*)name, nextp-name, ee->sha1, nil);
			h = namehash(ee->sha1);
			ee->nexthash = c->hash[h];
			c->hash[h] = ee;
		}
		e = ee;
	}
	if(e->kids){
		werrstr("%s already has children; cannot be mount point", name);
		return nil;
	}
	return e;
}

static int
addpath(Config *c, char *name, uchar score[VtScoreSize], ulong time)
{
	Entry *e;

	e = walkpath(c, name);
	if(e == nil)
		return -1;
	e->isfsys = 1;
	e->time = time;
	memmove(e->score, score, VtScoreSize);
	return 0;
}

static void
mkhandle(Nfs3Handle *h, Entry *e)
{
	memmove(h->h, e->sha1, VtScoreSize);
	h->len = VtScoreSize;
}

Nfs3Status
handleparse(Nfs3Handle *h, Fsys **pfsys, Nfs3Handle *nh, int isgetattr)
{
	int hh;
	Entry *e;
	Disk *disk;
	Fsys *fsys;

	refreshconfig();

	if(h->len < VtScoreSize)
		return Nfs3ErrBadHandle;

	hh = namehash(h->h);
	for(e=config->hash[hh]; e; e=e->nexthash)
		if(memcmp(e->sha1, h->h, VtScoreSize) == 0)
			break;
	if(e == nil)
		return Nfs3ErrBadHandle;

	if(e->isfsys == 1 && e->fsys == nil && (h->len != VtScoreSize || !isgetattr)){
		if((disk = diskopenventi(config->vcache, e->score)) == nil){
			fprint(2, "cannot open disk %V: %r\n", e->score);
			return Nfs3ErrIo;
		}
		if((fsys = fsysopen(disk)) == nil){
			fprint(2, "cannot open fsys on %V: %r\n", e->score);
			diskclose(disk);
			return Nfs3ErrIo;
		}
		e->fsys = fsys;
	}

	if(e->fsys == nil || (isgetattr && h->len == VtScoreSize)){
		if(h->len != VtScoreSize)
			return Nfs3ErrBadHandle;
		*pfsys = &fsysconfig;
		*nh = *h;
		return Nfs3Ok;
	}
	*pfsys = e->fsys;
	if(h->len == VtScoreSize)
		return fsysroot(*pfsys, nh);
	nh->len = h->len - VtScoreSize;
	memmove(nh->h, h->h+VtScoreSize, nh->len);
	return Nfs3Ok;
}

void
handleunparse(Fsys *fsys, Nfs3Handle *h, Nfs3Handle *nh, int dotdot)
{
	Entry *e;
	int hh;

	refreshconfig();

	if(fsys == &fsysconfig)
		return;

	if(dotdot && nh->len == h->len - VtScoreSize
	&& memcmp(h->h+VtScoreSize, nh->h, nh->len) == 0){
		/* walked .. but didn't go anywhere: must be at root */
		hh = namehash(h->h);
		for(e=config->hash[hh]; e; e=e->nexthash)
			if(memcmp(e->sha1, h->h, VtScoreSize) == 0)
				break;
		if(e == nil)
			return;	/* cannot happen */

		/* walk .. */
		e = e->parent;
		nh->len = VtScoreSize;
		memmove(nh->h, e->sha1, VtScoreSize);
		return;
	}

	/* otherwise just insert the same prefix */
	memmove(nh->h+VtScoreSize, nh->h, VtScoreSize);
	nh->len += VtScoreSize;
	memmove(nh->h, h->h, VtScoreSize);
}

Nfs3Status
fsysconfigroot(Fsys *fsys, Nfs3Handle *h)
{
	USED(fsys);

	mkhandle(h, config->root);
	return Nfs3Ok;
}

Nfs3Status
fsysconfiggetattr(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	Entry *e;

	USED(fsys);
	USED(au);

	if(h->len != VtScoreSize)
		return Nfs3ErrBadHandle;

	e = entrybyhandle(h);
	if(e == nil)
		return Nfs3ErrNoEnt;

	memset(attr, 0, sizeof *attr);
	attr->type = Nfs3FileDir;
	attr->mode = 0555;
	attr->nlink = 2;
	attr->size = 1024;
	attr->fileid = *(u64int*)h->h;
	attr->atime.sec = e->time;
	attr->mtime.sec = e->time;
	attr->ctime.sec = e->time;
	return Nfs3Ok;
}

Nfs3Status
fsysconfigaccess(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	want &= Nfs3AccessRead|Nfs3AccessLookup|Nfs3AccessExecute;
	*got = want;
	return fsysconfiggetattr(fsys, au, h, attr);
}

Nfs3Status
fsysconfiglookup(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	Entry *e;

	USED(fsys);
	USED(au);

	if(h->len != VtScoreSize)
		return Nfs3ErrBadHandle;

	e = entrybyhandle(h);
	if(e == nil)
		return Nfs3ErrNoEnt;

	if(strcmp(name, "..") == 0)
		e = e->parent;
	else if(strcmp(name, ".") == 0){
		/* nothing */
	}else{
		if((e = entrylookup(e, name, strlen(name))) == nil)
			return Nfs3ErrNoEnt;
	}

	mkhandle(nh, e);
	return Nfs3Ok;
}

Nfs3Status
fsysconfigreadlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **link)
{
	USED(h);
	USED(fsys);
	USED(au);

	*link = 0;
	return Nfs3ErrNotSupp;
}

Nfs3Status
fsysconfigreadfile(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int offset, uchar **pdata, u32int *pcount, u1int *peof)
{
	USED(fsys);
	USED(h);
	USED(count);
	USED(offset);
	USED(pdata);
	USED(pcount);
	USED(peof);
	USED(au);

	return Nfs3ErrNotSupp;
}

Nfs3Status
fsysconfigreaddir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int cookie, uchar **pdata, u32int *pcount, u1int *peof)
{
	uchar *data, *p, *ep, *np;
	u64int c;
	Entry *e;
	Nfs3Entry ne;

	USED(fsys);
	USED(au);

	if(h->len != VtScoreSize)
		return Nfs3ErrBadHandle;

	e = entrybyhandle(h);
	if(e == nil)
		return Nfs3ErrNoEnt;

	e = e->kids;
	c = cookie;
	for(; c && e; c--)
		e = e->nextdir;
	if(e == nil){
		*pdata = 0;
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}

	data = emalloc(count);
	p = data;
	ep = data+count;
	while(e && p < ep){
		ne.name = e->name;
		ne.namelen = strlen(e->name);
		ne.cookie = ++cookie;
		ne.fileid = *(u64int*)e->sha1;
		if(nfs3entrypack(p, ep, &np, &ne) < 0)
			break;
		p = np;
		e = e->nextdir;
	}
	*pdata = data;
	*pcount = p - data;
	*peof = 0;
	return Nfs3Ok;
}

void
fsysconfigclose(Fsys *fsys)
{
	USED(fsys);
}

int
readconfig(char *name, VtCache *vcache, Nfs3Handle *h)
{
	Config *c;
	Dir *d;

	if((d = dirstat(name)) == nil)
		return -1;

	c = readconfigfile(name, vcache);
	if(c == nil){
		free(d);
		return -1;
	}

	config = c;
	mtime = d->mtime;
	stime = time(0);
	free(d);

	mkhandle(h, c->root);
	fsysconfig._lookup = fsysconfiglookup;
	fsysconfig._access = fsysconfigaccess;
	fsysconfig._getattr = fsysconfiggetattr;
	fsysconfig._readdir = fsysconfigreaddir;
	fsysconfig._readfile = fsysconfigreadfile;
	fsysconfig._readlink = fsysconfigreadlink;
	fsysconfig._root = fsysconfigroot;
	fsysconfig._close = fsysconfigclose;
	return 0;
}
