#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "xml.h"

typedef struct HttpObj	HttpObj;
extern QLock memdrawlock;

enum
{
	ObjNameSize	= 64,
	MaxObjs		= 64
};

struct HttpObj
{
	char	name[ObjNameSize];
	int	(*f)(HConnect*);
};

static HttpObj	objs[MaxObjs];

static char *webroot;

static	void		listenproc(void*);
static	int		estats(HConnect *c);
static	int		dindex(HConnect *c);
static	int		xindex(HConnect *c);
static	int		xlog(HConnect *c);
static	int		sindex(HConnect *c);
static	int		hempty(HConnect *c);
static	int		hlcacheempty(HConnect *c);
static	int		hdcacheempty(HConnect *c);
static	int		hicacheempty(HConnect *c);
static	int		hicachekick(HConnect *c);
static	int		hdcachekick(HConnect *c);
static	int		hicacheflush(HConnect *c);
static	int		hdcacheflush(HConnect *c);
static	int		notfound(HConnect *c);
static	int		httpdobj(char *name, int (*f)(HConnect*));
static	int		xgraph(HConnect *c);
static	int		xset(HConnect *c);
static	int		fromwebdir(HConnect *c);

int
httpdinit(char *address, char *dir)
{
	fmtinstall('D', hdatefmt);
/*	fmtinstall('H', httpfmt); */
	fmtinstall('U', hurlfmt);

	if(address == nil)
		address = "tcp!*!http";
	webroot = dir;
	
	httpdobj("/stats", estats);
	httpdobj("/index", dindex);
	httpdobj("/storage", sindex);
	httpdobj("/xindex", xindex);
	httpdobj("/flushicache", hicacheflush);
	httpdobj("/flushdcache", hdcacheflush);
	httpdobj("/kickicache", hicachekick);
	httpdobj("/kickdcache", hdcachekick);
	httpdobj("/graph/", xgraph);
	httpdobj("/set", xset);
	httpdobj("/set/", xset);
	httpdobj("/log", xlog);
	httpdobj("/log/", xlog);
	httpdobj("/empty", hempty);
	httpdobj("/emptyicache", hicacheempty);
	httpdobj("/emptylumpcache", hlcacheempty);
	httpdobj("/emptydcache", hdcacheempty);

	if(vtproc(listenproc, address) < 0)
		return -1;
	return 0;
}

static int
httpdobj(char *name, int (*f)(HConnect*))
{
	int i;

	if(name == nil || strlen(name) >= ObjNameSize)
		return -1;
	for(i = 0; i < MaxObjs; i++){
		if(objs[i].name[0] == '\0'){
			strcpy(objs[i].name, name);
			objs[i].f = f;
			return 0;
		}
		if(strcmp(objs[i].name, name) == 0)
			return -1;
	}
	return -1;
}

static HConnect*
mkconnect(void)
{
	HConnect *c;

	c = mallocz(sizeof(HConnect), 1);
	if(c == nil)
		sysfatal("out of memory");
	c->replog = nil;
	c->hpos = c->header;
	c->hstop = c->header;
	return c;
}

void httpproc(void*);

static void
listenproc(void *vaddress)
{
	HConnect *c;
	char *address, ndir[NETPATHLEN], dir[NETPATHLEN];
	int ctl, nctl, data;

	address = vaddress;
	ctl = announce(address, dir);
	if(ctl < 0){
		fprint(2, "venti: httpd can't announce on %s: %r\n", address);
		return;
	}

	if(0) print("announce ctl %d dir %s\n", ctl, dir);
	for(;;){
		/*
		 *  wait for a call (or an error)
		 */
		nctl = listen(dir, ndir);
		if(0) print("httpd listen %d %s...\n", nctl, ndir);
		if(nctl < 0){
			fprint(2, "venti: httpd can't listen on %s: %r\n", address);
			return;
		}

		data = accept(ctl, ndir);
		if(0) print("httpd accept %d...\n", data);
		if(data < 0){
			fprint(2, "venti: httpd accept: %r\n");
			close(nctl);
			continue;
		}
		if(0) print("httpd close nctl %d\n", nctl);
		close(nctl);
		c = mkconnect();
		hinit(&c->hin, data, Hread);
		hinit(&c->hout, data, Hwrite);
		vtproc(httpproc, c);
	}
}

void
httpproc(void *v)
{
	HConnect *c;
	int ok, i, n;

	c = v;

	for(;;){
		/*
		 * No timeout because the signal appears to hit every
		 * proc, not just us.
		 */
		if(hparsereq(c, 0) < 0)
			break;

		for(i = 0; i < MaxObjs && objs[i].name[0]; i++){
			n = strlen(objs[i].name);
			if((objs[i].name[n-1] == '/' && strncmp(c->req.uri, objs[i].name, n) == 0)
			|| (objs[i].name[n-1] != '/' && strcmp(c->req.uri, objs[i].name) == 0)){
				ok = (*objs[i].f)(c);
				goto found;
			}
		}
		ok = fromwebdir(c);
	found:
		if(c->head.closeit)
			ok = -1;
		hreqcleanup(c);

		if(ok < 0)
			break;
	}
	hreqcleanup(c);
	close(c->hin.fd);
	free(c);
}

static int
percent(ulong v, ulong total)
{
	if(total == 0)
		total = 1;
	if(v < 1000*1000)
		return (v * 100) / total;
	total /= 100;
	if(total == 0)
		total = 1;
	return v / total;
}

static int
preq(HConnect *c)
{
	if(hparseheaders(c, 0) < 0)
		return -1;
	if(strcmp(c->req.meth, "GET") != 0
	&& strcmp(c->req.meth, "HEAD") != 0)
		return hunallowed(c, "GET, HEAD");
	if(c->head.expectother || c->head.expectcont)
		return hfail(c, HExpectFail, nil);
	return 0;
}

static int
preqtype(HConnect *c, char *type)
{
	Hio *hout;
	int r;

	r = preq(c);
	if(r < 0)
		return r;

	hout = &c->hout;
	if(c->req.vermaj){
		hokheaders(c);
		hprint(hout, "Content-type: %s\r\n", type);
		if(http11(c))
			hprint(hout, "Transfer-Encoding: chunked\r\n");
		hprint(hout, "\r\n");
	}

	if(http11(c))
		hxferenc(hout, 1);
	else
		c->head.closeit = 1;
	return 0;
}

static int
preqtext(HConnect *c)
{
	return preqtype(c, "text/plain");
}

static int
herror(HConnect *c)
{
	int n;
	Hio *hout;
	
	hout = &c->hout;
	n = snprint(c->xferbuf, HBufSize, "<html><head><title>Error</title></head>\n<body><h1>Error</h1>\n<pre>%r</pre>\n</body></html>");
	hprint(hout, "%s %s\r\n", hversion, "400 Bad Request");
	hprint(hout, "Date: %D\r\n", time(nil));
	hprint(hout, "Server: Venti\r\n");
	hprint(hout, "Content-Type: text/html\r\n");
	hprint(hout, "Content-Length: %d\r\n", n);
	if(c->head.closeit)
		hprint(hout, "Connection: close\r\n");
	else if(!http11(c))
		hprint(hout, "Connection: Keep-Alive\r\n");
	hprint(hout, "\r\n");

	if(c->req.meth == nil || strcmp(c->req.meth, "HEAD") != 0)
		hwrite(hout, c->xferbuf, n);

	return hflush(hout);
}
	
static int
notfound(HConnect *c)
{
	int r;

	r = preq(c);
	if(r < 0)
		return r;
	return hfail(c, HNotFound, c->req.uri);
}

struct {
	char *ext;
	char *type;
} exttab[] = {
	".html",	"text/html",
	".txt",	"text/plain",
	".xml",	"text/xml",
	".png",	"image/png",
	".gif",	"image/gif",
	0
};

static int
fromwebdir(HConnect *c)
{
	char buf[4096], *p, *ext, *type;
	int i, fd, n, defaulted;
	Dir *d;
	
	if(webroot == nil || strstr(c->req.uri, ".."))
		return notfound(c);
	snprint(buf, sizeof buf-20, "%s/%s", webroot, c->req.uri+1);
	defaulted = 0;
reopen:
	if((fd = open(buf, OREAD)) < 0)
		return notfound(c);
	d = dirfstat(fd);
	if(d == nil){
		close(fd);
		return notfound(c);
	}
	if(d->mode&DMDIR){
		if(!defaulted){
			defaulted = 1;
			strcat(buf, "/index.html");
			free(d);
			close(fd);
			goto reopen;
		}
		free(d);
		return notfound(c);
	}
	free(d);
	p = buf+strlen(buf);
	type = "application/octet-stream";
	for(i=0; exttab[i].ext; i++){
		ext = exttab[i].ext;
		if(p-strlen(ext) >= buf && strcmp(p-strlen(ext), ext) == 0){
			type = exttab[i].type;
			break;
		}
	}
	if(preqtype(c, type) < 0){
		close(fd);
		return 0;
	}
	while((n = read(fd, buf, sizeof buf)) > 0)
		if(hwrite(&c->hout, buf, n) < 0)
			break;
	close(fd);
	hflush(&c->hout);
	return 0;
}

static struct
{
	char *name;
	int *p;
} namedints[] =
{
	"compress",	&compressblocks,
	"devnull",	&writestodevnull,
	"logging",	&ventilogging,
	"stats",	&collectstats,
	"icachesleeptime",	&icachesleeptime,
	"minicachesleeptime",	&minicachesleeptime,
	"arenasumsleeptime",	&arenasumsleeptime,
	"l0quantum",	&l0quantum,
	"l1quantum",	&l1quantum,
	"manualscheduling",	&manualscheduling,
	"ignorebloom",	&ignorebloom,
	"syncwrites",	&syncwrites,
	"icacheprefetch",	&icacheprefetch,
	0
};

static int
xsetlist(HConnect *c)
{
	int i;
	
	if(preqtype(c, "text/plain") < 0)
		return -1;
	for(i=0; namedints[i].name; i++)
		print("%s = %d\n", namedints[i].name, *namedints[i].p);
	hflush(&c->hout);
	return 0;
}



static int
xset(HConnect *c)
{
	int i, nf, r;
	char *f[10], *s;

	if(strcmp(c->req.uri, "/set") == 0 || strcmp(c->req.uri, "/set/") == 0)
		return xsetlist(c);

	s = estrdup(c->req.uri);
	nf = getfields(s+strlen("/set/"), f, nelem(f), 1, "/");

	if(nf < 1){
		r = preqtext(c);
		if(r < 0)
			return r;
		for(i=0; namedints[i].name; i++)
			hprint(&c->hout, "%s = %d\n", namedints[i].name, *namedints[i].p);
		hflush(&c->hout);
		return 0;
	}
	for(i=0; namedints[i].name; i++){
		if(strcmp(f[0], namedints[i].name) == 0){
			if(nf >= 2)
				*namedints[i].p = atoi(f[1]);
			r = preqtext(c);
			if(r < 0)
				return r;
			hprint(&c->hout, "%s = %d\n", f[0], *namedints[i].p);
			hflush(&c->hout);
			return 0;
		}
	}
	return notfound(c);
}

static int
estats(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;


	hout = &c->hout;
/*
	hprint(hout, "lump writes=%,ld\n", stats.lumpwrites);
	hprint(hout, "lump reads=%,ld\n", stats.lumpreads);
	hprint(hout, "lump cache read hits=%,ld\n", stats.lumphit);
	hprint(hout, "lump cache read misses=%,ld\n", stats.lumpmiss);

	hprint(hout, "clump disk writes=%,ld\n", stats.clumpwrites);
	hprint(hout, "clump disk bytes written=%,lld\n", stats.clumpbwrites);
	hprint(hout, "clump disk bytes compressed=%,lld\n", stats.clumpbcomp);
	hprint(hout, "clump disk reads=%,ld\n", stats.clumpreads);
	hprint(hout, "clump disk bytes read=%,lld\n", stats.clumpbreads);
	hprint(hout, "clump disk bytes uncompressed=%,lld\n", stats.clumpbuncomp);

	hprint(hout, "clump directory disk writes=%,ld\n", stats.ciwrites);
	hprint(hout, "clump directory disk reads=%,ld\n", stats.cireads);

	hprint(hout, "index disk writes=%,ld\n", stats.indexwrites);
	hprint(hout, "index disk reads=%,ld\n", stats.indexreads);
	hprint(hout, "index disk bloom filter hits=%,ld %d%% falsemisses=%,ld %d%%\n",
		stats.indexbloomhits,
		percent(stats.indexbloomhits, stats.indexreads),
		stats.indexbloomfalsemisses,
		percent(stats.indexbloomfalsemisses, stats.indexreads));
	hprint(hout, "bloom filter bits=%,ld of %,ld %d%%\n",
		stats.bloomones, stats.bloombits, percent(stats.bloomones, stats.bloombits));
	hprint(hout, "index disk reads for modify=%,ld\n", stats.indexwreads);
	hprint(hout, "index disk reads for allocation=%,ld\n", stats.indexareads);
	hprint(hout, "index block splits=%,ld\n", stats.indexsplits);

	hprint(hout, "index cache lookups=%,ld\n", stats.iclookups);
	hprint(hout, "index cache hits=%,ld %d%%\n", stats.ichits,
		percent(stats.ichits, stats.iclookups));
	hprint(hout, "index cache fills=%,ld %d%%\n", stats.icfills,
		percent(stats.icfills, stats.iclookups));
	hprint(hout, "index cache inserts=%,ld\n", stats.icinserts);

	hprint(hout, "disk cache hits=%,ld\n", stats.pchit);
	hprint(hout, "disk cache misses=%,ld\n", stats.pcmiss);
	hprint(hout, "disk cache reads=%,ld\n", stats.pcreads);
	hprint(hout, "disk cache bytes read=%,lld\n", stats.pcbreads);

	hprint(hout, "disk cache writes=%,ld\n", stats.dirtydblocks);
	hprint(hout, "disk cache writes absorbed=%,ld %d%%\n", stats.absorbedwrites,
		percent(stats.absorbedwrites, stats.dirtydblocks));

	hprint(hout, "disk cache flushes=%,ld\n", stats.dcacheflushes);
	hprint(hout, "disk cache flush writes=%,ld (%,ld per flush)\n", 
		stats.dcacheflushwrites,
		stats.dcacheflushwrites/(stats.dcacheflushes ? stats.dcacheflushes : 1));

	hprint(hout, "disk writes=%,ld\n", stats.diskwrites);
	hprint(hout, "disk bytes written=%,lld\n", stats.diskbwrites);
	hprint(hout, "disk reads=%,ld\n", stats.diskreads);
	hprint(hout, "disk bytes read=%,lld\n", stats.diskbreads);
*/

	hflush(hout);
	return 0;
}

static int
sindex(HConnect *c)
{
	Hio *hout;
	Index *ix;
	Arena *arena;
	vlong clumps, cclumps, uncsize, used, size;
	int i, r, active;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	ix = mainindex;

	hprint(hout, "index=%s\n", ix->name);

	active = 0;
	clumps = 0;
	cclumps = 0;
	uncsize = 0;
	used = 0;
	size = 0;
	for(i = 0; i < ix->narenas; i++){
		arena = ix->arenas[i];
		if(arena != nil && arena->memstats.clumps != 0){
			active++;
			clumps += arena->memstats.clumps;
			cclumps += arena->memstats.cclumps;
			uncsize += arena->memstats.uncsize;
			used += arena->memstats.used;
		}
		size += arena->size;
	}
	hprint(hout, "total arenas=%,d active=%,d\n", ix->narenas, active);
	hprint(hout, "total space=%,lld used=%,lld\n", size, used + clumps * ClumpInfoSize);
	hprint(hout, "clumps=%,lld compressed clumps=%,lld data=%,lld compressed data=%,lld\n",
		clumps, cclumps, uncsize, used - clumps * ClumpSize);
	hflush(hout);
	return 0;
}

static void
darena(Hio *hout, Arena *arena)
{
	hprint(hout, "arena='%s' on %s at [%lld,%lld)\n\tversion=%d created=%d modified=%d",
		arena->name, arena->part->name, arena->base, arena->base + arena->size + 2 * arena->blocksize,
		arena->version, arena->ctime, arena->wtime);
	if(arena->memstats.sealed)
		hprint(hout, " mem=sealed");
	if(arena->diskstats.sealed)
		hprint(hout, " disk=sealed");
	hprint(hout, "\n");
	if(scorecmp(zeroscore, arena->score) != 0)
		hprint(hout, "\tscore=%V\n", arena->score);

	hprint(hout, "\tmem: clumps=%d compressed clumps=%d data=%,lld compressed data=%,lld storage=%,lld\n",
		arena->memstats.clumps, arena->memstats.cclumps, arena->memstats.uncsize,
		arena->memstats.used - arena->memstats.clumps * ClumpSize,
		arena->memstats.used + arena->memstats.clumps * ClumpInfoSize);
	hprint(hout, "\tdisk: clumps=%d compressed clumps=%d data=%,lld compressed data=%,lld storage=%,lld\n",
		arena->diskstats.clumps, arena->diskstats.cclumps, arena->diskstats.uncsize,
		arena->diskstats.used - arena->diskstats.clumps * ClumpSize,
		arena->diskstats.used + arena->diskstats.clumps * ClumpInfoSize);
}

static int
hempty(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	emptylumpcache();
	emptydcache();
	emptyicache();
	hprint(hout, "emptied all caches\n");
	hflush(hout);
	return 0;
}

static int
hlcacheempty(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	emptylumpcache();
	hprint(hout, "emptied lumpcache\n");
	hflush(hout);
	return 0;
}

static int
hicacheempty(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	emptyicache();
	hprint(hout, "emptied icache\n");
	hflush(hout);
	return 0;
}

static int
hdcacheempty(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	emptydcache();
	hprint(hout, "emptied dcache\n");
	hflush(hout);
	return 0;
}
static int
hicachekick(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	kickicache();
	hprint(hout, "kicked icache\n");
	hflush(hout);
	return 0;
}

static int
hdcachekick(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	kickdcache();
	hprint(hout, "kicked dcache\n");
	hflush(hout);
	return 0;
}
static int
hicacheflush(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	flushicache();
	hprint(hout, "flushed icache\n");
	hflush(hout);
	return 0;
}

static int
hdcacheflush(HConnect *c)
{
	Hio *hout;
	int r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;

	flushdcache();
	hprint(hout, "flushed dcache\n");
	hflush(hout);
	return 0;
}

static int
dindex(HConnect *c)
{
	Hio *hout;
	Index *ix;
	int i, r;

	r = preqtext(c);
	if(r < 0)
		return r;
	hout = &c->hout;


	ix = mainindex;
	hprint(hout, "index=%s version=%d blocksize=%d tabsize=%d\n",
		ix->name, ix->version, ix->blocksize, ix->tabsize);
	hprint(hout, "\tbuckets=%d div=%d\n", ix->buckets, ix->div);
	for(i = 0; i < ix->nsects; i++)
		hprint(hout, "\tsect=%s for buckets [%lld,%lld) buckmax=%d\n", ix->smap[i].name, ix->smap[i].start, ix->smap[i].stop, ix->sects[i]->buckmax);
	for(i = 0; i < ix->narenas; i++){
		if(ix->arenas[i] != nil && ix->arenas[i]->memstats.clumps != 0){
			hprint(hout, "arena=%s at index [%lld,%lld)\n\t", ix->amap[i].name, ix->amap[i].start, ix->amap[i].stop);
			darena(hout, ix->arenas[i]);
		}
	}
	hflush(hout);
	return 0;
}

typedef struct Arg Arg;
struct Arg
{
	int index;
	int index2;
};

static long
rawgraph(Stats *s, Stats *t, void *va)
{
	Arg *a;

	USED(s);
	a = va;
	return t->n[a->index];
}

static long
diffgraph(Stats *s, Stats *t, void *va)
{
	Arg *a;

	a = va;
	return t->n[a->index] - s->n[a->index];
}

static long
pctgraph(Stats *s, Stats *t, void *va)
{
	Arg *a;

	USED(s);
	a = va;
	return percent(t->n[a->index], t->n[a->index2]);
}

static long
pctdiffgraph(Stats *s, Stats *t, void *va)
{
	Arg *a;

	a = va;
	return percent(t->n[a->index]-s->n[a->index], t->n[a->index2]-s->n[a->index2]);
}

static long
netbw(Stats *s)
{
	ulong *n;

	n = s->n;
	return n[StatRpcReadBytes]+n[StatRpcWriteBytes];	/* not exactly right */
}

static long
diskbw(Stats *s)
{
	ulong *n;

	n = s->n;
	return n[StatApartReadBytes]+n[StatApartWriteBytes]	
		+ n[StatIsectReadBytes]+n[StatIsectWriteBytes]
		+ n[StatSumReadBytes];
}

static long
iobw(Stats *s)
{
	return netbw(s)+diskbw(s);
}

static long
diskgraph(Stats *s, Stats *t, void *va)
{
	USED(va);
	return diskbw(t)-diskbw(s);
}

static long
netgraph(Stats *s, Stats *t, void *va)
{
	USED(va);
	return netbw(t)-netbw(s);
}

static long
iograph(Stats *s, Stats *t, void *va)
{
	USED(va);
	return iobw(t)-iobw(s);
}


static char* graphname[] =
{
	"rpctotal",
	"rpcread",
	"rpcreadok",
	"rpcreadfail",
	"rpcreadbyte",
	"rpcreadtime",
	"rpcreadcached",
	"rpcreadcachedtime",
	"rpcreaduncached",
	"rpcreaduncachedtime",
	"rpcwrite",
	"rpcwritenew",
	"rpcwriteold",
	"rpcwritefail",
	"rpcwritebyte",
	"rpcwritetime",
	"rpcwritenewtime",
	"rpcwriteoldtime",

	"lcachehit",
	"lcachemiss",
	"lcachelookup",
	"lcachewrite",
	"lcachesize",
	"lcachestall",
	"lcachelookuptime",
	
	"dcachehit",
	"dcachemiss",
	"dcachelookup",
	"dcacheread",
	"dcachewrite",
	"dcachedirty",
	"dcachesize",
	"dcacheflush",
	"dcachestall",
	"dcachelookuptime",

	"dblockstall",
	"lumpstall",

	"icachehit",
	"icachemiss",
	"icachelookup",
	"icachewrite",
	"icachefill",
	"icacheprefetch",
	"icachedirty",
	"icachesize",
	"icacheflush",
	"icachestall",
	"icachelookuptime",

	"bloomhit",
	"bloommiss",
	"bloomfalsemiss",
	"bloomlookup",
	"bloomones",
	"bloombits",
	"bloomlookuptime",

	"apartread",
	"apartreadbyte",
	"apartwrite",
	"apartwritebyte",

	"isectread",
	"isectreadbyte",
	"isectwrite",
	"isectwritebyte",

	"sumread",
	"sumreadbyte",
};

static int
findname(char *s)
{
	int i;

	for(i=0; i<nelem(graphname); i++)
		if(strcmp(graphname[i], s) == 0)
			return i;
	return -1;
}

static void
dotextbin(Hio *io, Graph *g)
{
	int i, nbin;
	Statbin *b, bin[2000];	/* 32 kB, but whack is worse */

	needstack(8192);	/* double check that bin didn't kill us */
	nbin = 100;
	binstats(g->fn, g->arg, g->t0, g->t1, bin, nbin);

	hprint(io, "stats\n\n");
	for(i=0; i<nbin; i++){
		b = &bin[i];
		hprint(io, "%d: nsamp=%d min=%d max=%d avg=%d\n",
			i, b->nsamp, b->min, b->max, b->avg);
	}
}

static int
xgraph(HConnect *c)
{
	char *f[20], *s;
	Hio *hout;
	Memimage *m;
	int i, nf, dotext;
	Graph g;
	Arg arg;

	s = estrdup(c->req.uri);
if(0) fprint(2, "graph %s\n" ,s);
	memset(&g, 0, sizeof g);
	nf = getfields(s+strlen("/graph/"), f, nelem(f), 1, "/");
	if(nf < 1){
		werrstr("bad syntax -- not enough fields");
		goto error;
	}
	if((arg.index = findname(f[0])) == -1 && strcmp(f[0], "*") != 0){
		werrstr("unknown name %s", f[0]);
		goto error;
	}
	g.arg = &arg;
	g.t0 = -120;
	g.t1 = 0;
	g.min = -1;
	g.max = -1;
	g.fn = rawgraph;
	g.wid = -1;
	g.ht = -1;
	dotext = 0;
	g.fill = -1;
	for(i=1; i<nf; i++){
		if(strncmp(f[i], "t0=", 3) == 0)
			g.t0 = atoi(f[i]+3);
		else if(strncmp(f[i], "t1=", 3) == 0)
			g.t1 = atoi(f[i]+3);
		else if(strncmp(f[i], "min=", 4) == 0)
			g.min = atoi(f[i]+4);
		else if(strncmp(f[i], "max=", 4) == 0)
			g.max = atoi(f[i]+4);
		else if(strncmp(f[i], "pct=", 4) == 0){
			if((arg.index2 = findname(f[i]+4)) == -1){
				werrstr("unknown name %s", f[i]+4);
				goto error;
			}
			g.fn = pctgraph;
			g.min = 0;
			g.max = 100;
		}else if(strncmp(f[i], "pctdiff=", 8) == 0){
			if((arg.index2 = findname(f[i]+8)) == -1){
				werrstr("unknown name %s", f[i]+8);
				goto error;
			}
			g.fn = pctdiffgraph;
			g.min = 0;
			g.max = 100;
		}else if(strcmp(f[i], "diff") == 0)
			g.fn = diffgraph;
		else if(strcmp(f[i], "text") == 0)
			dotext = 1;
		else if(strncmp(f[i], "wid=", 4) == 0)
			g.wid = atoi(f[i]+4);
		else if(strncmp(f[i], "ht=", 3) == 0)
			g.ht = atoi(f[i]+3);
		else if(strncmp(f[i], "fill=", 5) == 0)
			g.fill = atoi(f[i]+5);
		else if(strcmp(f[i], "diskbw") == 0)
			g.fn = diskgraph;
		else if(strcmp(f[i], "iobw") == 0)
			g.fn = iograph;
		else if(strcmp(f[i], "netbw") == 0)
			g.fn = netgraph;
	}
	if(dotext){
		preqtype(c, "text/plain");
		dotextbin(&c->hout, &g);
		hflush(&c->hout);
		return 0;
	}

	m = statgraph(&g);
	if(m == nil)
		goto error;

	if(preqtype(c, "image/png") < 0)
		return -1;
	hout = &c->hout;
	writepng(hout, m);
	qlock(&memdrawlock);
	freememimage(m);
	qunlock(&memdrawlock);
	hflush(hout);
	free(s);
	return 0;

error:
	free(s);
	return herror(c);
}

static int
xloglist(HConnect *c)
{
	if(preqtype(c, "text/html") < 0)
		return -1;
	vtloghlist(&c->hout);
	hflush(&c->hout);
	return 0;
}

static int
xlog(HConnect *c)
{
	char *name;
	VtLog *l;

	if(strcmp(c->req.uri, "/log") == 0 || strcmp(c->req.uri, "/log/") == 0)
		return xloglist(c);
	if(strncmp(c->req.uri, "/log/", 5) != 0)
		return notfound(c);
	name = c->req.uri + strlen("/log/");
	l = vtlogopen(name, 0);
	if(l == nil)
		return notfound(c);
	if(preqtype(c, "text/html") < 0){
		vtlogclose(l);
		return -1;
	}
	vtloghdump(&c->hout, l);
	vtlogclose(l);
	hflush(&c->hout);
	return 0;
}

static int
xindex(HConnect *c)
{
	if(preqtype(c, "text/xml") < 0)
		return -1;
	xmlindex(&c->hout, mainindex, "index", 0);
	hflush(&c->hout);
	return 0;
}

void
xmlindent(Hio *hout, int indent)
{
	int i;

	for(i = 0; i < indent; i++)
		hputc(hout, '\t');
}

void
xmlaname(Hio *hout, char *v, char *tag)
{
	hprint(hout, " %s=\"%s\"", tag, v);
}

void
xmlscore(Hio *hout, u8int *v, char *tag)
{
	if(scorecmp(zeroscore, v) == 0)
		return;
	hprint(hout, " %s=\"%V\"", tag, v);
}

void
xmlsealed(Hio *hout, int v, char *tag)
{
	if(!v)
		return;
	hprint(hout, " %s=\"yes\"", tag);
}

void
xmlu32int(Hio *hout, u32int v, char *tag)
{
	hprint(hout, " %s=\"%ud\"", tag, v);
}

void
xmlu64int(Hio *hout, u64int v, char *tag)
{
	hprint(hout, " %s=\"%llud\"", tag, v);
}

void
vtloghdump(Hio *h, VtLog *l)
{
	int i;
	VtLogChunk *c;
	char *name;
	
	name = l ? l->name : "&lt;nil&gt;";

	hprint(h, "<html><head>\n");
	hprint(h, "<title>Venti Server Log: %s</title>\n", name);
	hprint(h, "</head><body>\n");
	hprint(h, "<b>Venti Server Log: %s</b>\n<p>\n", name);
	
	if(l){
		c = l->w;
		for(i=0; i<l->nchunk; i++){
			if(++c == l->chunk+l->nchunk)
				c = l->chunk;
			hwrite(h, c->p, c->wp-c->p);
		}
	}
	hprint(h, "</body></html>\n");
}

static int
strpcmp(const void *va, const void *vb)
{
	return strcmp(*(char**)va, *(char**)vb);
}

void
vtloghlist(Hio *h)
{
	char **p;
	int i, n;
	
	hprint(h, "<html><head>\n");
	hprint(h, "<title>Venti Server Logs</title>\n");
	hprint(h, "</head><body>\n");
	hprint(h, "<b>Venti Server Logs</b>\n<p>\n");
	
	p = vtlognames(&n);
	qsort(p, n, sizeof(p[0]), strpcmp);
	for(i=0; i<n; i++)
		hprint(h, "<a href=\"/log/%s\">%s</a><br>\n", p[i], p[i]);
	vtfree(p);
	hprint(h, "</body></html>\n");
}
