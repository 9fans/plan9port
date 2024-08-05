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
extern int scotabwd;
int mainstacksize = 256*1024;
VtSrv *ventisrv;

void trie_init();
int trie_insert(unsigned char *, IAddr*); 
int trie_retrieve(unsigned char *, IAddr*); 
void trie_print(int);

static void fmtindex(Config *conf, Index *ix);

void
usage(void)
{
	fprint(2, "usage: venti [-Ldrs] [-a address] [-B blockcachesize] [-c config] "
"[-C lumpcachesize] [-h httpaddress] [-I indexcachesize] [-W webroot]\n");
	threadexitsall("usage");
}

#ifdef xxx
int
threadmaybackground(void)
{
	return 1;
}
#endif
static Config config;
void
threadmain(int argc, char *argv[])
{
	char *configfile, *haddr, *vaddr, *webroot;
	u32int mem, icmem, bcmem, minbcmem;

	traceinit();
	threadsetname("main");
	vaddr = nil;
	haddr = nil;
	configfile = nil;
	webroot = nil;
	mem = 0;
	icmem = 0;
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
	case 'I':
		icmem = unittoull(EARGF(usage()));
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
	if(icmem == 0)
		icmem = config.icmem;
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
	trie_print(0);

	threadexits(nil);
}

static void
vtrerror(VtReq *r, char *error)
{
	r->rx.msgtype = VtRerror;
	r->rx.error = estrdup(error);
}

/*
 * Index, mapping scores to log positions.
 *
 * The index is made up of some number of index sections, each of
 * which is typically stored on a different disk.  The blocks in all the
 * index sections are logically numbered, with each index section
 * responsible for a range of blocks.  Blocks are typically 8kB.
 *
 * The N index blocks are treated as a giant hash table.  The top 32 bits
 * of score are used as the key for a lookup.  Each index block holds
 * one hash bucket, which is responsible for ceil(2^32 / N) of the key space.
 *
 * The index is sized so that a particular bucket is extraordinarily
 * unlikely to overflow: assuming compressed data blocks are 4kB
 * on disk, and assuming each block has a 40 byte index entry,
 * the index data will be 1% of the total data.  Since scores are essentially
 * random, all buckets should be about the same fullness.
 * A factor of 5 gives us a wide comfort boundary to account for
 * random variation.  So the index disk space should be 5% of the arena disk space.
 */
Index*
initindex(char *name, ISect **sects, int n)
{
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
	if(0) fprint(2,"amap %llx %llx ix-amap %llx\n", amap, arenas, ix->amap);
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
	ISect *is;
#if 0
	ZBlock *b;
	int ok;

	b = alloczblock(HeadSize, 0, 0);
	if(b == nil || readpart(part, PartBlank, b->data, HeadSize) < 0){
		seterr(EAdmin, "can't read index section header: %r");
		return nil;
	}
#endif
	is = MKZ(ISect);
#if 0
	if(is == nil){
		freezblock(b);
		return nil;
	}
	is->part = part;
	ok = unpackisect(is, b->data);
	freezblock(b);
	if(ok < 0){
		seterr(ECorrupt, "corrupted index section header: %r");
		freeisect(is);
		return nil;
A
	}

	if(is->version != ISectVersion1 && is->version != ISectVersion2){
		seterr(EAdmin, "unknown index section version %d", is->version);
		freeisect(is);
		return nil;
	}

#endif
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
			trie_insert(c->info.score,&ia);
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

/*
 * lookup the score in the partition
 *
 * nothing needs to be explicitly locked:
 * only static parts of ix are used, and
 * the bucket is locked by the DBlock lock.
 */
int
loadientry(Index *ix, u8int *score, int type, IEntry *ie)
{
	int h, ok;

	ok = -1;

	trace(TraceLump, "loadientry enter");

	/*
	qlock(&stats.lock);
	stats.indexreads++;
	qunlock(&stats.lock);
	*/
	h = trie_retrieve(score,&ie->ia);
	if( h != ~0) ok = 0; 
	else {
		trace(TraceLump, "loadientry notfound");
		addstat(StatBloomFalseMiss, 1);
	}
#ifdef PARANOIA
	Arena *arena;
	u64int aa;
	unsigned char buf[VtScoreSize];
	if(ok==0) {
		arena = amapitoa(mainindex, ie->ia.addr, &aa);
		if(arena!=nil) {
			readarena(arena,aa+9,buf,VtScoreSize);
			assert(memcmp(buf,score,VtScoreSize)==0);
		}
	}
#endif
	trace(TraceLump, "loadientry exit");
	return ok;
}

// hic sunt leones
int icacheprefetch = 1;

void emptyicache(void) { }
void icacheclean(IEntry *ie) { }
void initicache(u32int mem0) { }

int
lookupscore(u8int score[VtScoreSize], int type, IAddr *ia)
{
	int ret = trie_retrieve(score, ia); 
	if(ret == ~0) return -1;
	if(ia->type != type) return -1;
	return 0;
}

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

ulong
icachedirtyfrac(void)
{
        return 500000;
}

void
initicachewrite(void) {}

int icachesleeptime = 1000;

void		flushicache(void) {}

void		kickicache(void) {}

int minicachesleeptime = 0;

void		delaykickicache(void) {}

