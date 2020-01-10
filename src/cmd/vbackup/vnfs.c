/*
 * TO DO:
 *	- gc of file systems (not going to do just yet?)
 *	- statistics file
 *	- configure on amsterdam
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ip.h>
#include <thread.h>
#include <libsec.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>
#include <venti.h>
#include "nfs3srv.h"

#define trace if(!tracecalls){}else print

typedef struct Ipokay Ipokay;
typedef struct Config Config;
typedef struct Ctree Ctree;
typedef struct Cnode Cnode;

struct Ipokay
{
	int okay;
	uchar ip[IPaddrlen];
	uchar mask[IPaddrlen];
};

struct Config
{
	Ipokay *ok;
	uint nok;
	ulong mtime;
	Ctree *ctree;
};

char 		*addr;
int			blocksize;
int			cachesize;
Config		config;
char			*configfile;
int			encryptedhandles = 1;
Channel		*nfschan;
Channel		*mountchan;
Channel		*timerchan;
Nfs3Handle	root;
SunSrv		*srv;
int			tracecalls;
VtCache		*vcache;
VtConn		*z;

void			cryptinit(void);
void			timerthread(void*);
void			timerproc(void*);

extern	void			handleunparse(Fsys*, Nfs3Handle*, Nfs3Handle*, int);
extern	Nfs3Status	handleparse(Nfs3Handle*, Fsys**, Nfs3Handle*, int);

Nfs3Status	logread(Cnode*, u32int, u64int, uchar**, u32int*, u1int*);
Nfs3Status	refreshdiskread(Cnode*, u32int, u64int, uchar**, u32int*, u1int*);
Nfs3Status	refreshconfigread(Cnode*, u32int, u64int, uchar**, u32int*, u1int*);

int readconfigfile(Config *cp);
void setrootfid(void);
int ipokay(uchar *ip, ushort port);

u64int	unittoull(char*);

void
usage(void)
{
	fprint(2, "usage: vnfs [-LLRVir] [-a addr] [-b blocksize] [-c cachesize] configfile\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	fmtinstall('B', sunrpcfmt);
	fmtinstall('C', suncallfmt);
	fmtinstall('F', vtfcallfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('I', eipfmt);
	fmtinstall('V', vtscorefmt);
	sunfmtinstall(&nfs3prog);
	sunfmtinstall(&nfsmount3prog);

	addr = "udp!*!2049";
	blocksize = 8192;
	cachesize = 400;
	srv = sunsrv();
	srv->ipokay = ipokay;
	cryptinit();

	ARGBEGIN{
	default:
		usage();
	case 'E':
		encryptedhandles = 0;
		break;
	case 'L':
		if(srv->localonly == 0)
			srv->localonly = 1;
		else
			srv->localparanoia = 1;
		break;
	case 'R':
		srv->chatty++;
		break;
	case 'T':
		tracecalls = 1;
		break;
	case 'V':
		if(chattyventi++)
			vttracelevel++;
		break;
	case 'a':
		addr = EARGF(usage());
		break;
	case 'b':
		blocksize = unittoull(EARGF(usage()));
		break;
	case 'c':
		cachesize = unittoull(EARGF(usage()));
		break;
	case 'i':
		insecure = 1;
		break;
	case 'r':
		srv->alwaysreject++;
		break;
	}ARGEND

	if(argc != 1)
		usage();

	if((z = vtdial(nil)) == nil)
		sysfatal("vtdial: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");
	if((vcache = vtcachealloc(z, blocksize*cachesize)) == nil)
		sysfatal("vtcache: %r");

	configfile = argv[0];
	if(readconfigfile(&config) < 0)
		sysfatal("readConfig: %r");
	setrootfid();

	nfschan = chancreate(sizeof(SunMsg*), 0);
	mountchan = chancreate(sizeof(SunMsg*), 0);
	timerchan = chancreate(sizeof(void*), 0);

	if(sunsrvudp(srv, addr) < 0)
		sysfatal("starting server: %r");

	sunsrvthreadcreate(srv, nfs3proc, nfschan);
	sunsrvthreadcreate(srv, mount3proc, mountchan);
	sunsrvthreadcreate(srv, timerthread, nil);
	proccreate(timerproc, nil, 32768);

	sunsrvprog(srv, &nfs3prog, nfschan);
	sunsrvprog(srv, &nfsmount3prog, mountchan);

	threadexits(nil);
}

#define TWID64	((u64int)~(u64int)0)

u64int
unittoull(char *s)
{
	char *es;
	u64int n;

	if(s == nil)
		return TWID64;
	n = strtoul(s, &es, 0);
	if(*es == 'k' || *es == 'K'){
		n *= 1024;
		es++;
	}else if(*es == 'm' || *es == 'M'){
		n *= 1024*1024;
		es++;
	}else if(*es == 'g' || *es == 'G'){
		n *= 1024*1024*1024;
		es++;
	}else if(*es == 't' || *es == 'T'){
		n *= 1024*1024;
		n *= 1024*1024;
	}
	if(*es != '\0')
		return TWID64;
	return n;
}

/*
 * Handles.
 *
 * We store all the state about which file a client is accessing in
 * the handle, so that we don't have to maintain any per-client state
 * ourselves.  In order to avoid leaking handles or letting clients
 * create arbitrary handles, we sign and encrypt each handle with
 * AES using a key selected randomly when the server starts.
 * Thus, handles cannot be used across sessions.
 *
 * The decrypted handles begin with the following header:
 *
 *	sessid[8]		random session id chosen at start time
 *	len[4]		length of handle that follows
 *
 * If we're pressed for space in the rest of the handle, we could
 * probably reduce the amount of sessid bytes.  Note that the sessid
 * bytes must be consistent during a run of vnfs, or else some
 * clients (e.g., Linux 2.4) eventually notice that successive TLookups
 * return different handles, and they return "Stale NFS file handle"
 * errors to system calls in response (even though we never sent
 * that error!).
 *
 * Security woes aside, the fact that we have to shove everything
 * into the handles is quite annoying.  We have to encode, in 40 bytes:
 *
 *	- position in the synthesized config tree
 *	- enough of the path to do glob matching
 *	- position in an archived file system image
 *
 * and the handles need to be stable across changes in the config file
 * (though not across server restarts since encryption screws
 * that up nicely).
 *
 * We encode each of the first two as a 10-byte hash that is
 * the first half of a SHA1 hash.
 */

enum
{
	SessidSize = 8,
	HeaderSize = SessidSize+4,
	MaxHandleSize = Nfs3MaxHandleSize - HeaderSize
};

AESstate		aesstate;
uchar		sessid[SessidSize];

static void
hencrypt(Nfs3Handle *h)
{
	uchar *p;
	AESstate aes;

	/*
	 * root handle has special encryption - a single 0 byte - so that it
	 * never goes stale.
	 */
	if(h->len == root.len && memcmp(h->h, root.h, root.len) == 0){
		h->h[0] = 0;
		h->len = 1;
		return;
	}

	if(!encryptedhandles)
		return;

	if(h->len > MaxHandleSize){
		/* oops */
		fprint(2, "handle too long: %.*lH\n", h->len, h->h);
		memset(h->h, 'X', Nfs3MaxHandleSize);
		h->len = Nfs3MaxHandleSize;
		return;
	}

	p = h->h;
	memmove(p+HeaderSize, p, h->len);
	memmove(p, sessid, SessidSize);
	*(u32int*)(p+SessidSize) = h->len;
	h->len += HeaderSize;

	if(encryptedhandles){
		while(h->len < MaxHandleSize)
			h->h[h->len++] = 0;
		aes = aesstate;
		aesCBCencrypt(h->h, MaxHandleSize, &aes);
	}
}

static Nfs3Status
hdecrypt(Nfs3Handle *h)
{
	AESstate aes;

	if(h->len == 1 && h->h[0] == 0){	/* single 0 byte is root */
		*h = root;
		return Nfs3Ok;
	}

	if(!encryptedhandles)
		return Nfs3Ok;

	if(h->len <= HeaderSize)
		return Nfs3ErrBadHandle;
	if(encryptedhandles){
		if(h->len != MaxHandleSize)
			return Nfs3ErrBadHandle;
		aes = aesstate;
		aesCBCdecrypt(h->h, h->len, &aes);
	}
	if(memcmp(h->h, sessid, SessidSize) != 0)
		return Nfs3ErrStale;	/* give benefit of doubt */
	h->len = *(u32int*)(h->h+SessidSize);
	if(h->len >= MaxHandleSize-HeaderSize)
		return Nfs3ErrBadHandle;
	memmove(h->h, h->h+HeaderSize, h->len);
	return Nfs3Ok;
}

void
cryptinit(void)
{
	uchar key[32], ivec[AESbsize];
	int i;
	u32int u32;

	u32 = truerand();
	memmove(sessid, &u32, 4);
	for(i=0; i<nelem(key); i+=4) {
		u32 = truerand();
		memmove(key+i, &u32, 4);
	}
	for(i=0; i<nelem(ivec); i++)
		ivec[i] = fastrand();
	setupAESstate(&aesstate, key, sizeof key, ivec);
}

/*
 * Config file.
 *
 * The main purpose of the configuration file is to define a tree
 * in which the archived file system images are mounted.
 * The tree is stored as Entry structures, defined below.
 *
 * The configuration file also allows one to define shell-like
 * glob expressions matching paths that are not to be displayed.
 * The matched files or directories are shown in directory listings
 * (could suppress these if we cared) but they cannot be opened,
 * read, or written, and getattr returns zeroed data.
 */
enum
{
	/* sizes used in handles; see nfs server below */
	CnodeHandleSize = 8,
	FsysHandleOffset = CnodeHandleSize
};

/*
 * Config file tree.
 */
struct Ctree
{
	Cnode *root;
	Cnode *hash[1024];
};

struct Cnode
{
	char *name;	/* path element */
	Cnode *parent;	/* in tree */
	Cnode *nextsib;	/* in tree */
	Cnode *kidlist;	/* in tree */
	Cnode *nexthash;	/* in hash list */

	Nfs3Status (*read)(Cnode*, u32int, u64int, uchar**, u32int*, u1int*);	/* synthesized read fn */

	uchar handle[VtScoreSize];	/* sha1(path to here) */
	ulong mtime;	/* mtime for this directory entry */

	/* fsys overlay on this node */
	Fsys *fsys;	/* cache of memory structure */
	Nfs3Handle fsyshandle;
	int isblackhole;	/* walking down keeps you here */

	/*
	 * mount point info.
	 * if a mount point is inside another file system,
	 * the fsys and fsyshandle above have the old fs info,
	 * the mfsys and mfsyshandle below have the new one.
	 * getattrs must use the old info for consistency.
	 */
	int ismtpt;	/* whether there is an fsys mounted here */
	uchar fsysscore[VtScoreSize];	/* score of fsys image on venti */
	char *fsysimage;	/* raw disk image */
	Fsys *mfsys;	/* mounted file system (nil until walked) */
	Nfs3Handle mfsyshandle;	/* handle to root of mounted fsys */

	int mark;	/* gc */
};

static uint
dumbhash(uchar *s)
{
	return (s[0]<<2)|(s[1]>>6);	/* first 10 bits */
}

static Cnode*
mkcnode(Ctree *t, Cnode *parent, char *elem, uint elen, char *path, uint plen)
{
	uint h;
	Cnode *n;

	n = emalloc(sizeof *n + elen+1);
	n->name = (char*)(n+1);
	memmove(n->name, elem, elen);
	n->name[elen] = 0;
	n->parent = parent;
	if(parent){
		n->nextsib = parent->kidlist;
		parent->kidlist = n;
	}
	n->kidlist = nil;
	sha1((uchar*)path, plen, n->handle, nil);
	h = dumbhash(n->handle);
	n->nexthash = t->hash[h];
	t->hash[h] = n;

	return n;
}

void
markctree(Ctree *t)
{
	int i;
	Cnode *n;

	for(i=0; i<nelem(t->hash); i++)
		for(n=t->hash[i]; n; n=n->nexthash)
			if(n->name[0] != '+')
				n->mark = 1;
}

int
refreshdisk(void)
{
	int i;
	Cnode *n;
	Ctree *t;

	t = config.ctree;
	for(i=0; i<nelem(t->hash); i++)
		for(n=t->hash[i]; n; n=n->nexthash){
			if(n->mfsys)
				disksync(n->mfsys->disk);
			if(n->fsys)
				disksync(n->fsys->disk);
		}
	return 0;
}

void
sweepctree(Ctree *t)
{
	int i;
	Cnode *n;

	/* just zero all the garbage and leave it linked into the tree */
	for(i=0; i<nelem(t->hash); i++){
		for(n=t->hash[i]; n; n=n->nexthash){
			if(!n->mark)
				continue;
			n->fsys = nil;
			free(n->fsysimage);
			n->fsysimage = nil;
			memset(n->fsysscore, 0, sizeof n->fsysscore);
			n->mfsys = nil;
			n->ismtpt = 0;
			memset(&n->fsyshandle, 0, sizeof n->fsyshandle);
			memset(&n->mfsyshandle, 0, sizeof n->mfsyshandle);
		}
	}
}

static Cnode*
cnodewalk(Cnode *n, char *name, uint len, int markokay)
{
	Cnode *nn;

	for(nn=n->kidlist; nn; nn=nn->nextsib)
		if(strncmp(nn->name, name, len) == 0 && nn->name[len] == 0)
		if(!nn->mark || markokay)
			return nn;
	return nil;
}

Cnode*
ctreewalkpath(Ctree *t, char *name, ulong createmtime)
{
	Cnode *n, *nn;
	char *p, *nextp;

	n = t->root;
	p = name;
	for(; *p; p=nextp){
		n->mark = 0;
		assert(*p == '/');
		p++;
		nextp = strchr(p, '/');
		if(nextp == nil)
			nextp = p+strlen(p);
		if((nn = cnodewalk(n, p, nextp-p, 1)) == nil){
			if(createmtime == 0)
				return nil;
			nn = mkcnode(t, n, p, nextp-p, name, nextp-name);
			nn->mtime = createmtime;
		}
		if(nn->mark)
			nn->mark = 0;
		n = nn;
	}
	n->mark = 0;
	return n;
}

Ctree*
mkctree(void)
{
	Ctree *t;

	t = emalloc(sizeof *t);
	t->root = mkcnode(t, nil, "", 0, "", 0);

	ctreewalkpath(t, "/+log", time(0))->read = logread;
	ctreewalkpath(t, "/+refreshdisk", time(0))->read = refreshdiskread;
	ctreewalkpath(t, "/+refreshconfig", time(0))->read = refreshconfigread;

	return t;
}

Cnode*
ctreemountfsys(Ctree *t, char *path, ulong time, uchar *score, char *file)
{
	Cnode *n;

	if(time == 0)
		time = 1;
	n = ctreewalkpath(t, path, time);
	if(score){
		if(n->ismtpt && (n->fsysimage || memcmp(n->fsysscore, score, VtScoreSize) != 0)){
			free(n->fsysimage);
			n->fsysimage = nil;
			n->fsys = nil;	/* leak (might be other refs) */
		}
		memmove(n->fsysscore, score, VtScoreSize);
	}else{
		if(n->ismtpt && (n->fsysimage==nil || strcmp(n->fsysimage, file) != 0)){
			free(n->fsysimage);
			n->fsysimage = nil;
			n->fsys = nil;	/* leak (might be other refs) */
		}
		n->fsysimage = emalloc(strlen(file)+1);
		strcpy(n->fsysimage, file);
	}
	n->ismtpt = 1;
	return n;
}

Cnode*
cnodebyhandle(Ctree *t, uchar *p)
{
	int h;
	Cnode *n;

	h = dumbhash(p);
	for(n=t->hash[h]; n; n=n->nexthash)
		if(memcmp(n->handle, p, CnodeHandleSize) == 0)
			return n;
	return nil;
}

static int
parseipandmask(char *s, uchar *ip, uchar *mask)
{
	char *p, *q;

	p = strchr(s, '/');
	if(p)
		*p++ = 0;
	if(parseip(ip, s) == ~0UL)
		return -1;
	if(p == nil)
		memset(mask, 0xFF, IPaddrlen);
	else{
		if(isdigit((uchar)*p) && strtol(p, &q, 10)>=0 && *q==0)
			*--p = '/';
		if(parseipmask(mask, p) == ~0UL)
			return -1;
		if(*p != '/')
			*--p = '/';
	}
/*fprint(2, "parseipandmask %s => %I %I\n", s, ip, mask); */
	return 0;
}

static int
parsetime(char *s, ulong *time)
{
	ulong x;
	char *p;
	int i;
	Tm tm;

	/* decimal integer is seconds since 1970 */
	x = strtoul(s, &p, 10);
	if(x > 0 && *p == 0){
		*time = x;
		return 0;
	}

	/* otherwise expect yyyy/mmdd/hhmm */
	if(strlen(s) != 14 || s[4] != '/' || s[9] != '/')
		return -1;
	for(i=0; i<4; i++)
		if(!isdigit((uchar)s[i]) || !isdigit((uchar)s[i+5]) || !isdigit((uchar)s[i+10]))
			return -1;
	memset(&tm, 0, sizeof tm);
	tm.year = atoi(s)-1900;
	if(tm.year < 0 || tm.year > 200)
		return -1;
	tm.mon = (s[5]-'0')*10+s[6]-'0' - 1;
	if(tm.mon < 0 || tm.mon > 11)
		return -1;
	tm.mday = (s[7]-'0')*10+s[8]-'0';
	if(tm.mday < 0 || tm.mday > 31)
		return -1;
	tm.hour = (s[10]-'0')*10+s[11]-'0';
	if(tm.hour < 0 || tm.hour > 23)
		return -1;
	tm.min = (s[12]-'0')*10+s[13]-'0';
	if(tm.min < 0 || tm.min > 59)
		return -1;
	strcpy(tm.zone, "XXX");	/* anything but GMT */
if(0){
print("tm2sec %d/%d/%d/%d/%d\n",
	tm.year, tm.mon, tm.mday, tm.hour, tm.min);
}
	*time = tm2sec(&tm);
if(0) print("time %lud\n", *time);
	return 0;
}


int
readconfigfile(Config *cp)
{
	char *f[10], *image, *p, *pref, *q, *name;
	int nf, line;
	uchar scorebuf[VtScoreSize], *score;
	ulong time;
	Biobuf *b;
	Config c;
	Dir *dir;

	name = configfile;
	c = *cp;
	if((dir = dirstat(name)) == nil)
		return -1;
	if(c.mtime == dir->mtime){
		free(dir);
		return 0;
	}
	c.mtime = dir->mtime;
	free(dir);
	if((b = Bopen(name, OREAD)) == nil)
		return -1;

	/*
	 * Reuse old tree, garbage collecting entries that
	 * are not mentioned in the new config file.
	 */
	if(c.ctree == nil)
		c.ctree = mkctree();

	markctree(c.ctree);
	c.ok = nil;
	c.nok = 0;

	line = 0;
	for(; (p=Brdstr(b, '\n', 1)) != nil; free(p)){
		line++;
		if((q = strchr(p, '#')) != nil)
			*q = 0;
		nf = tokenize(p, f, nelem(f));
		if(nf == 0)
			continue;
		if(strcmp(f[0], "mount") == 0){
			if(nf != 4){
				werrstr("syntax error: mount /path /dev|score mtime");
				goto badline;
			}
			if(f[1][0] != '/'){
				werrstr("unrooted path %s", f[1]);
				goto badline;
			}
			score = nil;
			image = nil;
			if(f[2][0] == '/'){
				if(access(f[2], AEXIST) < 0){
					werrstr("image %s does not exist", f[2]);
					goto badline;
				}
				image = f[2];
			}else{
				if(vtparsescore(f[2], &pref, scorebuf) < 0){
					werrstr("bad score %s", f[2]);
					goto badline;
				}
				score = scorebuf;
			}
			if(parsetime(f[3], &time) < 0){
				fprint(2, "%s:%d: bad time %s\n", name, line, f[3]);
				time = 1;
			}
			ctreemountfsys(c.ctree, f[1], time, score, image);
			continue;
		}
		if(strcmp(f[0], "allow") == 0 || strcmp(f[0], "deny") == 0){
			if(nf != 2){
				werrstr("syntax error: allow|deny ip[/mask]");
				goto badline;
			}
			c.ok = erealloc(c.ok, (c.nok+1)*sizeof(c.ok[0]));
			if(parseipandmask(f[1], c.ok[c.nok].ip, c.ok[c.nok].mask) < 0){
				werrstr("bad ip[/mask]: %s", f[1]);
				goto badline;
			}
			c.ok[c.nok].okay = (strcmp(f[0], "allow") == 0);
			c.nok++;
			continue;
		}
		werrstr("unknown verb '%s'", f[0]);
	badline:
		fprint(2, "%s:%d: %r\n", name, line);
	}
	Bterm(b);

	sweepctree(c.ctree);
	free(cp->ok);
	*cp = c;
	return 0;
}

int
ipokay(uchar *ip, ushort port)
{
	int i;
	uchar ipx[IPaddrlen];
	Ipokay *ok;

	for(i=0; i<config.nok; i++){
		ok = &config.ok[i];
		maskip(ip, ok->mask, ipx);
if(0) fprint(2, "%I & %I = %I (== %I?)\n",
	ip, ok->mask, ipx, ok->ip);
		if(memcmp(ipx, ok->ip, IPaddrlen) == 0)
			return ok->okay;
	}
	if(config.nok == 0)	/* all is permitted */
		return 1;
	/* otherwise default is none allowed */
	return 0;
}

Nfs3Status
cnodelookup(Ctree *t, Cnode **np, char *name)
{
	Cnode *n, *nn;

	n = *np;
	if(n->isblackhole)
		return Nfs3Ok;
	if((nn = cnodewalk(n, name, strlen(name), 0)) == nil){
		if(n->ismtpt || n->fsys){
			if((nn = cnodewalk(n, "", 0, 1)) == nil){
				nn = mkcnode(t, n, "", 0, (char*)n->handle, SHA1dlen);
				nn->isblackhole = 1;
			}
			nn->mark = 0;
		}
	}
	if(nn == nil)
		return Nfs3ErrNoEnt;
	*np = nn;
	return Nfs3Ok;
}

Nfs3Status
cnodegetattr(Cnode *n, Nfs3Attr *attr)
{
	uint64 u64;

	memset(attr, 0, sizeof *attr);
	if(n->read){
		attr->type = Nfs3FileReg;
		attr->mode = 0444;
		attr->size = 512;
		attr->nlink = 1;
	}else{
		attr->type = Nfs3FileDir;
		attr->mode = 0555;
		attr->size = 1024;
		attr->nlink = 10;
	}
	memmove(&u64, n->handle, 8);
	attr->fileid = u64;
	attr->atime.sec = n->mtime;
	attr->mtime.sec = n->mtime;
	attr->ctime.sec = n->mtime;
	return Nfs3Ok;
}

Nfs3Status
cnodereaddir(Cnode *n, u32int count, u64int cookie, uchar **pdata, u32int *pcount, u1int *peof)
{
	uchar *data, *p, *ep, *np;
	u64int c;
	u64int u64;
	Nfs3Entry ne;

	n = n->kidlist;
	c = cookie;
	for(; c && n; c--)
		n = n->nextsib;
	if(n == nil){
		*pdata = 0;
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}

	data = emalloc(count);
	p = data;
	ep = data+count;
	while(n && p < ep){
		if(n->mark || n->name[0] == '+'){
			n = n->nextsib;
			++cookie;
			continue;
		}
		ne.name = n->name;
		ne.namelen = strlen(n->name);
		ne.cookie = ++cookie;
		memmove(&u64, n->handle, 8);
		ne.fileid = u64;
		if(nfs3entrypack(p, ep, &np, &ne) < 0)
			break;
		p = np;
		n = n->nextsib;
	}
	*pdata = data;
	*pcount = p - data;
	*peof = n==nil;
	return Nfs3Ok;
}

void
timerproc(void *v)
{
	for(;;){
		sleep(60*1000);
		sendp(timerchan, 0);
	}
}

void
timerthread(void *v)
{
	for(;;){
		recvp(timerchan);
	/*	refreshconfig(); */
	}
}

/*
 * Actually serve the NFS requests.  Called from nfs3srv.c.
 * Each request runs in its own thread (coroutine).
 *
 * Decrypted handles have the form:
 *
 *	config[20] - SHA1 hash identifying a config tree node
 *	glob[10] - SHA1 hash prefix identifying a glob state
 *	fsyshandle[<=10] - disk file system handle (usually 4 bytes)
 */

/*
 * A fid represents a point in the file tree.
 * There are three components, all derived from the handle:
 *
 *	- config tree position (also used to find fsys)
 *	- glob state for exclusions
 *	- file system position
 */
enum
{
	HAccess,
	HAttr,
	HWalk,
	HDotdot,
	HRead
};
typedef struct Fid Fid;
struct Fid
{
	Cnode *cnode;
	Fsys *fsys;
	Nfs3Handle fsyshandle;
};

int
handlecmp(Nfs3Handle *h, Nfs3Handle *h1)
{
	if(h->len != h1->len)
		return h->len - h1->len;
	return memcmp(h->h, h1->h, h->len);
}

Nfs3Status
handletofid(Nfs3Handle *eh, Fid *fid, int mode)
{
	int domount;
	Cnode *n;
	Disk *disk, *cdisk;
	Fsys *fsys;
	Nfs3Status ok;
	Nfs3Handle h2, *h, *fh;

	memset(fid, 0, sizeof *fid);

	domount = 1;
	if(mode == HDotdot)
		domount = 0;
	/*
	 * Not necessary, but speeds up ls -l /dump/2005
	 * HAttr and HAccess must be handled the same way
	 * because both can be used to fetch attributes.
	 * Acting differently yields inconsistencies at mount points,
	 * and causes FreeBSD ls -l to fail.
	 */
	if(mode == HAttr || mode == HAccess)
		domount = 0;

	/*
	 * Decrypt handle.
	 */
	h2 = *eh;
	h = &h2;
	if((ok = hdecrypt(h)) != Nfs3Ok)
		return ok;
	trace("handletofid: decrypted %.*lH\n", h->len, h->h);
	if(h->len < FsysHandleOffset)
		return Nfs3ErrBadHandle;

	/*
	 * Find place in config tree.
	 */
	if((n = cnodebyhandle(config.ctree, h->h)) == nil)
		return Nfs3ErrStale;
	fid->cnode = n;

	if(n->ismtpt && domount){
		/*
		 * Open fsys for mount point if needed.
		 */
		if(n->mfsys == nil){
			trace("handletofid: mounting %V/%s\n", n->fsysscore, n->fsysimage);
			if(n->fsysimage){
				if(strcmp(n->fsysimage, "/dev/null") == 0)
					return Nfs3ErrAcces;
				if((disk = diskopenfile(n->fsysimage)) == nil){
					fprint(2, "cannot open disk %s: %r\n", n->fsysimage);
					return Nfs3ErrIo;
				}
				if((cdisk = diskcache(disk, blocksize, 64)) == nil){
					fprint(2, "cannot cache disk %s: %r\n", n->fsysimage);
					diskclose(disk);
				}
				disk = cdisk;
			}else{
				if((disk = diskopenventi(vcache, n->fsysscore)) == nil){
					fprint(2, "cannot open venti disk %V: %r\n", n->fsysscore);
					return Nfs3ErrIo;
				}
			}
			if((fsys = fsysopen(disk)) == nil){
				fprint(2, "cannot open fsys on %V: %r\n", n->fsysscore);
				diskclose(disk);
				return Nfs3ErrIo;
			}
			n->mfsys = fsys;
			fsysroot(fsys, &n->mfsyshandle);
		}

		/*
		 * Use inner handle.
		 */
		fid->fsys = n->mfsys;
		fid->fsyshandle = n->mfsyshandle;
	}else{
		/*
		 * Use fsys handle from tree or from handle.
		 * This assumes that fsyshandle was set by fidtohandle
		 * earlier, so it's not okay to reuse handles (except the root)
		 * across sessions.  The encryption above makes and
		 * enforces the same restriction, so this is okay.
		 */
		fid->fsys = n->fsys;
		fh = &fid->fsyshandle;
		if(n->isblackhole){
			fh->len = h->len-FsysHandleOffset;
			memmove(fh->h, h->h+FsysHandleOffset, fh->len);
		}else
			*fh = n->fsyshandle;
		trace("handletofid: fsyshandle %.*lH\n", fh->len, fh->h);
	}

	/*
	 * TO DO (maybe): some sort of path restriction here.
	 */
	trace("handletofid: cnode %s fsys %p fsyshandle %.*lH\n",
		n->name, fid->fsys, fid->fsyshandle.len, fid->fsyshandle.h);
	return Nfs3Ok;
}

void
_fidtohandle(Fid *fid, Nfs3Handle *h)
{
	Cnode *n;

	n = fid->cnode;
	/*
	 * Record fsys handle in n, don't bother sending it to client
	 * for black holes.
	 */
	n->fsys = fid->fsys;
	if(!n->isblackhole){
		n->fsyshandle = fid->fsyshandle;
		fid->fsyshandle.len = 0;
	}
	memmove(h->h, n->handle, CnodeHandleSize);
	memmove(h->h+FsysHandleOffset, fid->fsyshandle.h, fid->fsyshandle.len);
	h->len = FsysHandleOffset+fid->fsyshandle.len;
}

void
fidtohandle(Fid *fid, Nfs3Handle *h)
{
	_fidtohandle(fid, h);
	hencrypt(h);
}

void
setrootfid(void)
{
	Fid fid;

	memset(&fid, 0, sizeof fid);
	fid.cnode = config.ctree->root;
	_fidtohandle(&fid, &root);
}

void
fsgetroot(Nfs3Handle *h)
{
	*h = root;
	hencrypt(h);
}

Nfs3Status
fsgetattr(SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	Fid fid;
	Nfs3Status ok;

	trace("getattr %.*lH\n", h->len, h->h);
	if((ok = handletofid(h, &fid, HAttr)) != Nfs3Ok)
		return ok;
	if(fid.fsys)
		return fsysgetattr(fid.fsys, au, &fid.fsyshandle, attr);
	else
		return cnodegetattr(fid.cnode, attr);
}

/*
 * Lookup is always the hard part.
 */
Nfs3Status
fslookup(SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	Fid fid;
	Cnode *n;
	Nfs3Status ok;
	Nfs3Handle xh;
	int mode;

	trace("lookup %.*lH %s\n", h->len, h->h, name);

	mode = HWalk;
	if(strcmp(name, "..") == 0 || strcmp(name, ".") == 0)
		mode = HDotdot;
	if((ok = handletofid(h, &fid, mode)) != Nfs3Ok){
		nfs3errstr(ok);
		trace("lookup: handletofid %r\n");
		return ok;
	}

	if(strcmp(name, ".") == 0){
		fidtohandle(&fid, nh);
		return Nfs3Ok;
	}

	/*
	 * Walk down file system and cnode simultaneously.
	 * If dotdot and file system doesn't move, need to walk
	 * up cnode.  Save the corresponding fsys handles in
	 * the cnode as we walk down so that we'll have them
	 * for dotdotting back up.
	 */
	n = fid.cnode;
	if(mode == HWalk){
		/*
		 * Walk down config tree and file system simultaneously.
		 */
		if((ok = cnodelookup(config.ctree, &n, name)) != Nfs3Ok){
			nfs3errstr(ok);
			trace("lookup: cnodelookup: %r\n");
			return ok;
		}
		fid.cnode = n;
		if(fid.fsys){
			if((ok = fsyslookup(fid.fsys, au, &fid.fsyshandle, name, &xh)) != Nfs3Ok){
				nfs3errstr(ok);
				trace("lookup: fsyslookup: %r\n");
				return ok;
			}
			fid.fsyshandle = xh;
		}
	}else{
		/*
		 * Walking dotdot.  Ick.
		 */
		trace("lookup dotdot fsys=%p\n", fid.fsys);
		if(fid.fsys){
			/*
			 * Walk up file system, then try up config tree.
			 */
			if((ok = fsyslookup(fid.fsys, au, &fid.fsyshandle, "..", &xh)) != Nfs3Ok){
				nfs3errstr(ok);
				trace("lookup fsyslookup: %r\n");
				return ok;
			}
			fid.fsyshandle = xh;

			/*
			 * Usually just go to n->parent.
			 *
			 * If we're in a subtree of the mounted file system that
			 * isn't represented explicitly by the config tree (instead
			 * the black hole node represents the entire file tree),
			 * then we only go to n->parent when we've dotdotted back
			 * to the right handle.
			 */
			if(n->parent == nil)
				trace("lookup dotdot: no parent\n");
			else{
				trace("lookup dotdot: parent %.*lH, have %.*lH\n",
					n->parent->fsyshandle.len, n->parent->fsyshandle.h,
					xh.len, xh.h);
			}

			if(n->isblackhole){
				if(handlecmp(&n->parent->mfsyshandle, &xh) == 0)
					n = n->parent;
			}else{
				if(n->parent)
					n = n->parent;
			}
		}else{
			/*
			 * No file system, just walk up.
			 */
			if(n->parent)
				n = n->parent;
		}
		fid.fsys = n->fsys;
		if(!n->isblackhole)
			fid.fsyshandle = n->fsyshandle;
		fid.cnode = n;
	}
	fidtohandle(&fid, nh);
	return Nfs3Ok;
}

Nfs3Status
fsaccess(SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	Fid fid;
	Nfs3Status ok;

	trace("access %.*lH 0x%ux\n", h->len, h->h, want);
	if((ok = handletofid(h, &fid, HAccess)) != Nfs3Ok)
		return ok;
	if(fid.fsys)
		return fsysaccess(fid.fsys, au, &fid.fsyshandle, want, got, attr);
	*got = want & (Nfs3AccessRead|Nfs3AccessLookup|Nfs3AccessExecute);
	return cnodegetattr(fid.cnode, attr);
}

Nfs3Status
fsreadlink(SunAuthUnix *au, Nfs3Handle *h, char **link)
{
	Fid fid;
	Nfs3Status ok;

	trace("readlink %.*lH\n", h->len, h->h);
	if((ok = handletofid(h, &fid, HRead)) != Nfs3Ok)
		return ok;
	if(fid.fsys)
		return fsysreadlink(fid.fsys, au, &fid.fsyshandle, link);
	*link = 0;
	return Nfs3ErrNotSupp;
}

Nfs3Status
fsreadfile(SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int offset, uchar **data, u32int *pcount, u1int *peof)
{
	Fid fid;
	Nfs3Status ok;

	trace("readfile %.*lH\n", h->len, h->h);
	if((ok = handletofid(h, &fid, HRead)) != Nfs3Ok)
		return ok;
	if(fid.cnode->read)
		return fid.cnode->read(fid.cnode, count, offset, data, pcount, peof);
	if(fid.fsys)
		return fsysreadfile(fid.fsys, au, &fid.fsyshandle, count, offset, data, pcount, peof);
	return Nfs3ErrNotSupp;
}

Nfs3Status
fsreaddir(SunAuthUnix *au, Nfs3Handle *h, u32int len, u64int cookie, uchar **pdata, u32int *pcount, u1int *peof)
{
	Fid fid;
	Nfs3Status ok;

	trace("readdir %.*lH\n", h->len, h->h);
	if((ok = handletofid(h, &fid, HRead)) != Nfs3Ok)
		return ok;
	if(fid.fsys)
		return fsysreaddir(fid.fsys, au, &fid.fsyshandle, len, cookie, pdata, pcount, peof);
	return cnodereaddir(fid.cnode, len, cookie, pdata, pcount, peof);
}

Nfs3Status
logread(Cnode *n, u32int count, u64int offset, uchar **data, u32int *pcount, u1int *peof)
{
	*pcount = 0;
	*peof = 1;
	return Nfs3Ok;
}

Nfs3Status
refreshdiskread(Cnode *n, u32int count, u64int offset, uchar **data, u32int *pcount, u1int *peof)
{
	char buf[128];

	if(offset != 0){
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}
	if(refreshdisk() < 0)
		snprint(buf, sizeof buf, "refreshdisk: %r\n");
	else
		strcpy(buf, "ok\n");
	*data = emalloc(strlen(buf));
	strcpy((char*)*data, buf);
	*pcount = strlen(buf);
	*peof = 1;
	return Nfs3Ok;
}

Nfs3Status
refreshconfigread(Cnode *n, u32int count, u64int offset, uchar **data, u32int *pcount, u1int *peof)
{
	char buf[128];

	if(offset != 0){
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}
	if(readconfigfile(&config) < 0)
		snprint(buf, sizeof buf, "readconfig: %r\n");
	else
		strcpy(buf, "ok\n");
	*data = emalloc(strlen(buf));
	strcpy((char*)*data, buf);
	*pcount = strlen(buf);
	*peof = 1;
	return Nfs3Ok;
}
