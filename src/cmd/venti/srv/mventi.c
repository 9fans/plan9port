#ifdef PLAN9PORT
#include <u.h>
#include <signal.h>
#endif
#include "stdinc.h"
#include "dat.h"
#include "fns.h"

//#define XXX

int debug;
int nofork;
int mainstacksize = 256*1024;
VtSrv *ventisrv;

void trie_init(void);
u64int totalclumps;
unsigned int trie_insert(unsigned char *, uvlong*); 
unsigned int trie_retrieve(unsigned char *, uvlong*); 

static void ventiserver(void*);
static void fmtindex(Config *conf, Index *ix);

void
usage(void)
{
	fprint(2, "usage: mventi [-Ldrs] [-a address] [-B blockcachesize] [-c config] "
"[-C lumpcachesize] [-h httpaddress] [-I initialclumps] [-W webroot]\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

static Config config;
void
threadmain(int argc, char *argv[])
{
	char *configfile, *haddr, *vaddr, *webroot;
	u32int mem, bcmem, minbcmem;

	traceinit();
	threadsetname("main");
	vaddr = nil;
	haddr = nil;
	configfile = nil;
	webroot = nil;
	mem = 0;
	bcmem = 0;
	ARGBEGIN{
	case 'a':
		vaddr = EARGF(usage());
		break;
	case 'B':
		bcmem = unittoull(EARGF(usage()));
		break;
	case 'c':
		configfile = EARGF(usage());
		break;
	case 'C':
		mem = unittoull(EARGF(usage()));
		break;
	case 'I':
		totalclumps = unittoull(EARGF(usage()));
		break;
	case 'D':
		settrace(EARGF(usage()));
		break;
	case 'd':
		debug = 1;
		nofork = 1;
		break;
	case 'h':
		haddr = EARGF(usage());
		break;
	case 'L':
		ventilogging = 1;
		break;
	case 'r':
		readonly = 1;
		break;
	case 's':
		nofork = 1;
		break;
	case 'w':			/* compatibility with old venti */
		queuewrites = 1;
		break;
	case 'W':
		webroot = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc)
		usage();

	if(!nofork)
		rfork(RFNOTEG);

#ifdef PLAN9PORT
	{
		/* sigh - needed to avoid signals when writing to hungup networks */
		struct sigaction sa;
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = SIG_IGN;
		sigaction(SIGPIPE, &sa, nil);
	}
#endif

	ventifmtinstall();
	trace(TraceQuiet, "venti started");
	fprint(2, "%T venti: ");

	if(configfile == nil)
		configfile = "venti.conf";

	fprint(2, "conf...");
	if(initventi(configfile, &config) < 0)
		sysfatal("can't init server: %r");

	if(mem == 0)
		mem = config.mem;
	if(bcmem == 0)
		bcmem = config.bcmem;
	if(haddr == nil)
		haddr = config.haddr;
	if(vaddr == nil)
		vaddr = config.vaddr;
	if(vaddr == nil)
		vaddr = "tcp!*!venti";
	if(webroot == nil)
		webroot = config.webroot;
	if(queuewrites == 0)
		queuewrites = config.queuewrites;
	fprint(2, "confdone...");

	if(haddr){
		fprint(2, "httpd %s...", haddr);
		if(httpdinit(haddr, webroot) < 0)
			fprint(2, "warning: can't start http server: %r");
	}
	fprint(2, "init...");

	if(mem == 0xffffffffUL)
		mem = 1 * 1024 * 1024;

	/*
	 * lump cache
	 */
	if(0) fprint(2, "initialize %d bytes of lump cache for %d lumps\n",
		mem, mem / (8 * 1024));
	initlumpcache(mem, mem / (8 * 1024));

	/*
	 * block cache: need a block for every arena and every process
	 */
	minbcmem = maxblocksize *
		(mainindex->narenas + mainindex->nsects*4 + 16);
	if(bcmem < minbcmem)
		bcmem = minbcmem;
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	trie_init();

	if(initarenasum() < 0)
		fprint(2, "warning: can't initialize arena summing process: %r");

	fprint(2, "announce %s...", vaddr);
	ventisrv = vtlisten(vaddr);
	if(ventisrv == nil)
		sysfatal("can't announce %s: %r", vaddr);

	fprint(2, "serving.\n");
	if(nofork)
		ventiserver(nil);
	else
		vtproc(ventiserver, nil);

	threadexits(nil);
}

static void
vtrerror(VtReq *r, char *error)
{
	r->rx.msgtype = VtRerror;
	r->rx.error = estrdup(error);
}

static void
ventiserver(void *v)
{
	Packet *p;
	VtReq *r;
	char err[ERRMAX];
	uint ms;
	int cached, ok;

	USED(v);
	threadsetname("ventiserver");
	trace(TraceWork, "start");
	while((r = vtgetreq(ventisrv)) != nil){
		trace(TraceWork, "finish");
		trace(TraceWork, "start request %F", &r->tx);
		trace(TraceRpc, "<- %F", &r->tx);
		r->rx.msgtype = r->tx.msgtype+1;
		addstat(StatRpcTotal, 1);
		if(0) print("req (arenas[0]=%p sects[0]=%p) %F\n",
			mainindex->arenas[0], mainindex->sects[0], &r->tx);
		switch(r->tx.msgtype){
		default:
			vtrerror(r, "unknown request");
			break;
		case VtTread:
			ms = msec();
			r->rx.data = readlump(r->tx.score, r->tx.blocktype, r->tx.count, &cached);
			ms = msec() - ms;
			addstat2(StatRpcRead, 1, StatRpcReadTime, ms);
			if(r->rx.data == nil){
				addstat(StatRpcReadFail, 1);
				rerrstr(err, sizeof err);
				vtrerror(r, err);
			}else{
				addstat(StatRpcReadBytes, packetsize(r->rx.data));
				addstat(StatRpcReadOk, 1);
				if(cached)
					addstat2(StatRpcReadCached, 1, StatRpcReadCachedTime, ms);
				else
					addstat2(StatRpcReadUncached, 1, StatRpcReadUncachedTime, ms);
			}
			break;
		case VtTwrite:
			if(readonly){
				vtrerror(r, "read only");
				break;
			}
			p = r->tx.data;
			r->tx.data = nil;
			addstat(StatRpcWriteBytes, packetsize(p));
			ms = msec();
			ok = writelump(p, r->rx.score, r->tx.blocktype, 0, ms);
			ms = msec() - ms;
			addstat2(StatRpcWrite, 1, StatRpcWriteTime, ms);

			if(ok < 0){
				addstat(StatRpcWriteFail, 1);
				rerrstr(err, sizeof err);
				vtrerror(r, err);
			}
			break;
		case VtTsync:
//			flushqueue();
			flushdcache();
			break;
		}
		trace(TraceRpc, "-> %F", &r->rx);
		vtrespond(r);
		trace(TraceWork, "start");
	}
	flushdcache();
	threadexitsall(0);
}

/*
 * Index, mapping scores to log positions.
 */
Index*
initindex(char *name, ISect **sects, int n)
{
	USED(n);
	USED(sects);
	USED(name);
	Index *ix;
	ix = MKZ(Index);

	fmtindex(&config, ix);
	return ix;
}

static void
fmtindex(Config *conf, Index *ix)
{
	u32int narenas;
	AMap *amap;
	u64int addr;
	ArenaPart *ap;
	Arena **arenas;
	namecp(ix->name, "main");

        narenas = 0;
        for(int i = 0; i < conf->naparts; i++){
                ap = conf->aparts[i];
                narenas += ap->narenas;
        }

	amap = MKNZ(AMap, narenas);
	arenas = MKNZ(Arena*, narenas);
	ix->amap = amap;
	ix->arenas = arenas;

	addr = IndexBase;
	int n = 0;
//	if(0) fprint(2,"amap %llx %llx ix-amap %llx\n", amap, arenas, ix->amap);
	for(int i = 0; i < conf->naparts; i++){
		ap = conf->aparts[i];
		for(int j = 0; j < ap->narenas; j++){
			if(n >= narenas)
				sysfatal("too few slots in index's arena set");

			arenas[n] = ap->arenas[j];
			if(n < ix->narenas){
				if(arenas[n] != ix->arenas[n])
					sysfatal("mismatched arenas %s and %s at slot %d",
						arenas[n]->name, ix->arenas[n]->name, n);
				amap[n] = ix->amap[n];
				if(amap[n].start != addr)
					sysfatal("mis-located arena %s in index %s", arenas[n]->name, ix->name);
				addr = amap[n].stop;
			}else{
				amap[n].start = addr;
				addr += ap->arenas[j]->size;
				amap[n].stop = addr;
				namecp(amap[n].name, ap->arenas[j]->name);
				if(0) fprint(2, "add arena %s at [%lld,%lld)\n",
					amap[n].name, amap[n].start, amap[n].stop);
			}
#ifdef XXX
	for(u64int j=0; j<amn.n;j++) {
		amn.map[j].start = j<<48;
		amn.map[j].stop = (j+1)<<48;
	}	
	amn.map[0].start = 1048576;
#endif
			n++;
		}
	}
	ix->narenas = narenas;
}

ISect*
initisect(Part *part)
{
	USED(part);
	ISect *is;
	is = MKZ(ISect);
	return is;
}

void
freeisect(ISect *is)
{
	if(is == nil)
		return;
	free(is);
}

void
freeindex(Index *ix)
{
	int i;

	if(ix == nil)
		return;
	free(ix->amap);
	free(ix->arenas);
	if(ix->sects)
		for(i = 0; i < ix->nsects; i++)
			freeisect(ix->sects[i]);
	free(ix->sects);
	free(ix->smap);
	free(ix);
}

/*
 * write a clump to an available arena in the index
 * and return the address of the clump within the index.
ZZZ question: should this distinguish between an arena
filling up and real errors writing the clump?
 */
u64int
writeiclump(Index *ix, Clump *c, u8int *clbuf)
{
	u64int a;
	int i;
	IAddr ia;
	AState as;

	ia.addr = 0;
// this should not happen, but it does ZZZ
	unsigned int h= trie_retrieve(c->info.score,&ia.addr);
if(h!=~0) fprint(2,"h: %ux, %llux %V\n", h, ia.addr, c->info.score);
        if(h!=~0) return ia.addr;
	trace(TraceLump, "writeiclump enter");
	qlock(&ix->writing);
	for(i = ix->mapalloc; i < ix->narenas; i++){
		a = writeaclump(ix->arenas[i], c, clbuf);
		if(a != TWID64){
			ix->mapalloc = i;
			ia.addr = ix->amap[i].start + a;
			ia.type = c->info.type;
			ia.size = c->info.uncsize;
			ia.blocks = (c->info.size + ClumpSize + (1<<ABlockLog) - 1) >> ABlockLog;
			as.arena = ix->arenas[i];
			as.aa = ia.addr;
			as.stats = as.arena->memstats;
			trie_insert(c->info.score,&ia.addr);
			qunlock(&ix->writing);
			trace(TraceLump, "writeiclump exit");
			return ia.addr;
		}
	}
	qunlock(&ix->writing);

	seterr(EAdmin, "no space left in arenas");
	trace(TraceLump, "writeiclump failed");
	return TWID64;
}

/*
 * convert an arena index to an relative arena address
 */
Arena*
amapitoa(Index *ix, u64int a, u64int *aa)
{
#ifdef XXX
	*aa = a& ~(0xFFFFULL<<48);
	return ix->arenas[(int)(a>>48)];
#else
	int i, r, l, m;

	l = 1;
	r = ix->narenas - 1;
	while(l <= r){
		m = (r + l) / 2;
		if(ix->amap[m].start <= a)
			l = m + 1;
		else
			r = m - 1;
	}
	l--;

	if(a > ix->amap[l].stop){
for(i=0; i<ix->narenas; i++)
	print("arena %d: %llux - %llux\n", i, ix->amap[i].start, ix->amap[i].stop);
print("want arena %d for %llux\n", l, a);
		seterr(ECrash, "unmapped address passed to amapitoa");
		return nil;
	}

	if(ix->arenas[l] == nil){
		seterr(ECrash, "unmapped arena selected in amapitoa");
		return nil;
	}
	*aa = a - ix->amap[l].start;
	return ix->arenas[l];
#endif
}

int
loadclumpinfo(uvlong addr, ClumpInfo *ci)
{
	Arena *arena;
	u64int aa;
	unsigned char buf[ClumpInfoSize];
	arena = amapitoa(mainindex, addr, &aa);
	if(arena!=nil) {
		readarena(arena,aa+4,buf,ClumpInfoSize);
		unpackclumpinfo(ci, buf);
		return 0;
	}
	return -1;
}

/*
 * lookup the score in the partition
 */
int
loadientry(Index *ix, u8int *score, int type, IEntry *ie)
{
	unsigned int h;
	int ok;
	ClumpInfo ci;

	ok = -1;

	trace(TraceLump, "loadientry enter");

	h = trie_retrieve(score,&ie->ia.addr);
	if( h != ~0) ok = 0; 
	else {
		trace(TraceLump, "loadientry notfound");
		addstat(StatBloomFalseMiss, 1);
	}
	if(ok==0) {
		if( loadclumpinfo(ie->ia.addr, &ci) == 0) {
			memcpy(ie->score,score,VtScoreSize);
			ie->ia.type = ci.type;
			ie->ia.size = ci.uncsize;
			ie->ia.blocks = (ci.size + ClumpSize + (1<<ABlockLog)-1) >> ABlockLog;
		} else ok = -1;
	}
	trace(TraceLump, "loadientry exit");
	return ok;
}

int
lookupscore(u8int score[VtScoreSize], int type, IAddr *ia)
{
// cannot use trie_retrieve, because we need ia.type
	IEntry ie;
	int ret = loadientry(mainindex, score, type, &ie);
	*ia = ie.ia;
	if(ret == -1 || ie.ia.type != type) return -1;
	return 0;
}

// hic sunt leones
// these are defined here to avoid pulling bitrot from libvs.a
int icacheprefetch = 1;

void emptyicache(void) { }
void icacheclean(IEntry *x) { USED(x); }
void initicache(u32int x) { USED(x); }

/* cannot be removed, bec. hdisk.c */
int
icachelookup(u8int score[VtScoreSize], int type, IAddr *ia)
{
	return lookupscore(score,type,ia);
}

u32int
hashbits(u8int *sc, int bits)
{
        u32int v;
                
        v = (sc[0] << 24) | (sc[1] << 16) | (sc[2] << 8) | sc[3];
        if(bits < 32)
                 v >>= (32 - bits);
        return v;
}

ulong icachedirtyfrac(void) { return 500000; }

void initicachewrite(void) {}

int icachesleeptime = 1000;

void		flushicache(void) {}

void		kickicache(void) {}

int minicachesleeptime = 0;

void		delaykickicache(void) {}

// from lumpqueue.c
int
queuewrite(Lump *u, Packet *p, int creator, uint ms) {return 0;}
