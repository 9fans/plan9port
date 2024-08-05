#include "stdinc.h"
#include "dat.h"
#include "fns.h"
int loadclumpinfo(uvlong addr, ClumpInfo *ci);

/* 
   Some variables are static. To be threadsafe, accesses are to be qlocked.
*/
static QLock	trielock;
enum {
#define TRIPLET /* this changes the trie from 16-way to 8-way*/
#ifdef TRIPLET
	Dexes=8,
#else
	Dexes=16,
#endif
	Trivials=4, /* changing this constant is not trivial */
	ntries=1<<(Trivials*4),
};
struct trieleaf {
	uvlong addr;
} *leaves;
struct trienode{
	unsigned short int dex[Dexes];
};
static struct trie {
	struct trieleaf *leaves;
	struct trienode *trie;
	int ts, tmax, ls, lmax;
} *act;
uvlong actb;
unsigned int maxdepth=0;
static struct trie triedata[ntries];
static unsigned char *scotable=0;
/* scotabwd changeable for the case when a full score in scotab is requested */
static int scotabwd = 8;
static unsigned char oldscore[VtScoreSize];
static unsigned char newscore[VtScoreSize];

static void getscore(struct trieleaf *ia) {
        Arena *arena;
        u64int aa;
	arena = amapitoa(mainindex, ia->addr, &aa);
	if(arena!=nil)
		readarena(arena,aa+9,oldscore,VtScoreSize);
	if( actb == oldscore[0]*256+oldscore[1] );
	else fprint(2, "oldscore %V actb %2llux\n", oldscore, actb);
//	assert( actb == oldscore[0]*256+oldscore[1] );
}

#ifdef TRIPLET
inline static int nibble( unsigned char *c, int d) {
#if 0	/* former code, not really wrong, but changes sort order of scores */
	int d3 = 3*d;
	int r3 = d3>>3;
	int z = c[r3]+c[r3+1]*256;
	return 0x7&(z>>(d3&0x7));
#else
/* we have to start with the highest bits */
        d -= Trivials;
        int d3 = 3*d+2;
        int r3 = d3>>3;
        r3 += Trivials/2;
        int z;
        if((d&0x7) < 2) z = c[r3];
        else z = 256*c[r3-1]+c[r3];
        return 0x7&(z>>((15-d3)&0x7));
#endif
}
#else
inline static int nibble( unsigned char *c, int d) {
	if( d&1 ) return c[d/2]&0xf;
	return c[d/2]>>4;
}
#endif
inline static void realloc_leaves(struct trie *act) {
	if(act->leaves==(void*)0) {
		act->lmax = 127;
		act->leaves = malloc(act->lmax*sizeof(struct trieleaf));
		return;
	}
	assert(act->ls>=act->lmax);
	int dsiz = act->lmax/3;
	act->leaves = realloc(act->leaves, (act->lmax+dsiz)*sizeof(struct trieleaf));
	act->lmax += dsiz; }
		
inline static void realloc_nodes(struct trie *act) {
	if(act->trie==(void*)0) {
		act->tmax = 72;
		act->trie = malloc(act->tmax*sizeof(struct trienode));
		memset(act->trie,0,sizeof(struct trienode));
		return;
	}
	assert(act->ts>=act->tmax);
	int dsiz = act->tmax/3;
	act->trie = realloc( act->trie, (act->tmax+dsiz)*sizeof(struct trienode));
	act->tmax += dsiz; }

inline static unsigned int makeleaf(void) {
	/* leaf zero not allowed */
	act->ls++;
	if( act->ls >= act->lmax) realloc_leaves(act);
	assert(act->leaves != 0);
	assert(act->ls < act->lmax);
	if(scotable) memcpy(scotable+scotabwd*(actb*256+act->ls),newscore+2,scotabwd);
	act->leaves[act->ls].addr=~0ULL;
	return act->ls;
}

inline static unsigned int makenode(void) {
	act->ts++;
	if( act->ts >= act->tmax) realloc_nodes(act);
	assert(act->trie != 0);
	assert(act->ts < act->tmax);
	memset(act->trie+act->ts,0,sizeof(struct trienode));
	return act->ts;
}

inline int setdex(struct trienode *w, int d, int x) {
	return w->dex[d] = x;
} 

/* iterative version */
static unsigned int trie_lookup(int noinsert) {
   unsigned int oldp=0;
   unsigned char oldd=0;
   unsigned short int tr= ~0;
   unsigned int tscore = 0;
// we skip the trivial first trie levels encoded in act
   for(int depth=Trivials;depth<2*VtScoreSize;++depth) {
	if(depth>maxdepth) maxdepth=depth;
	assert(tr!=0);
	int td = nibble(newscore,depth);
	if(tr <= act->ls) { //  leaf node
//  convert to nonleaf
		/* for now, accessing score is not easily avoided */
		if(noinsert && tscore!=tr) {
			getscore(act->leaves+tr);
			tscore = tr;
		}
		if( depth>=19 || scotable==0 ||
				memcmp(scotable+scotabwd*(actb*256+tr),newscore+2,scotabwd) == 0 ) {
			if(tscore!=tr) {
				getscore(act->leaves+tr);
assert(scotable==0 || memcmp(scotable+scotabwd*(actb*256+tr),oldscore+2,scotabwd)==0);
				tscore = tr;
			}
		} else if(scotable && tscore!=tr)
			memcpy(oldscore+2,scotable+scotabwd*(actb*256+tr),scotabwd);
		if(noinsert) {
			assert( tr==tscore );
			if(memcmp(oldscore,newscore,VtScoreSize)==0) return tr;
			return ~0;
		}
		int t2 = nibble(oldscore,depth);
		unsigned int newp = makenode();
		assert(newp < act->tmax);
		setdex(act->trie+oldp,oldd,~0-act->ts);
		setdex(act->trie+newp,t2,tr);
		if( t2==td ) {  // need to go deeper
			oldp = newp;
			oldd = t2;
// tr does not to be changed here, we process the same leaf at more depth
			continue;
		}
		return setdex(act->trie+newp,td,makeleaf());
	} else {
		tr = ~0-tr;
		if( act->trie == (void*)0) realloc_nodes(act);
		assert(tr <= act->ts);
		assert(tr < act->tmax);
		if( act->trie[tr].dex[td] ) {
			oldp = tr;
			oldd = td;
			tr= act->trie[tr].dex[td];
			continue;
		}
		if( noinsert ) return ~0;
		if( depth < 4) { // at these levels, leaves likely be moved
			unsigned int newp = makenode();
			assert(newp < act->tmax);
			setdex(act->trie+tr,td,~0-newp);
			tr= ~0-newp;
			continue;
		}
		return setdex(act->trie+tr,td,makeleaf());
	}
   }
   return tr;
}

/* trie_print prints (part of the) input file in lexical order. */
static void trie_print0(unsigned short int t) {
	IEntry ie;
	Arena *arena;
        ClumpInfo ci;
        u64int aa=0;
	uchar buf[ClumpInfoSize];
	unsigned short int tr= t;

	assert(tr!=0);
	if(tr<=act->ls) { // leaf
                arena = amapitoa(mainindex, act->leaves[tr].addr, &aa);
                if(arena!=nil) {
                        readarena(arena,aa+4,buf,ClumpInfoSize);
                        unpackclumpinfo(&ci, buf);
                        ie.ia.type = ci.type;
                        ie.ia.size = ci.uncsize;
			print("%V %20ulld%3x%6ud\n", ci.score, aa, ci.type, ci.size);
                } else fprint(2, "illegal addr: %ullx\n", act->leaves[tr].addr );
	} else {
		tr = ~0-tr;
		assert(tr <= act->ts);
		for( int j=0; j < Dexes; j++ )
			if(act->trie[tr].dex[j]!=0) trie_print0(act->trie[tr].dex[j]);
	}
}

void trie_print(unsigned int tr) {
	USED(tr);
	for(int i=0; i< ntries; i++) {
		act = triedata+i;
		trie_print0(~0);
	}
}

static unsigned int trie_insert0(unsigned char* score, uvlong *addr, int noinsert) {
	unsigned int tf;
	qlock( &trielock );
	if( triedata[0].trie==(void*)0) {
		scotable= malloc(256*scotabwd*(uvlong)ntries);
		for( int j = 0; j<ntries; j++) {
			triedata[j].tmax = 0;
			triedata[j].lmax = 0;
			triedata[j].trie = 0;
			triedata[j].leaves = 0;
			triedata[j].ts = 0;
			triedata[j].ls = 0;
		}
		realloc_nodes(triedata);
		assert(triedata[0].tmax>0);
	}
	memcpy(newscore,score,VtScoreSize);
	if(Trivials == 4) actb = (score[0]*256+score[1]);
	else if(Trivials == 5)
		actb = 16*(score[0]*256+score[1])+nibble(score,4);
	act = triedata + actb;	
	tf =trie_lookup(noinsert);
	if(!noinsert) {
		if(act->leaves[tf].addr!=~0ULL){
#ifdef xxx

			ClumpInfo ci;
			loadclumpinfo( act->leaves[tf].addr,&ci);
			fprint(2, "duplicate entry, old=%I\n", &ci );
			loadclumpinfo( *addr,&ci);
			fprint(2, "                 new=%I\n", &ci );
#else
			fprint(2, "duplicate entry, %V\n", newscore);
#endif
		}
		act->leaves[tf].addr=*addr;
	} else if(tf != ~0) {
		*addr=act->leaves[tf].addr;
	}
	qunlock( &trielock );
	return tf;
}

unsigned int trie_insert(unsigned char* score, uvlong *addr) {
	return trie_insert0(score,addr,0);
}
unsigned int trie_retrieve(unsigned char* score, uvlong *addr) {
	return trie_insert0(score,addr,1);
}

int		errors;

Channel	*arenadonechan;
Index	*ix;

u64int	arenaentries;
u64int	skipentries;
u64int	indexentries;

static void	arenapartproc(void*);
void
trie_init( void) {
	int i, napart, nfinish;
	Part *p;

	ix = mainindex;
	ix->bloom = nil;

	/* start arena procs */
	p = nil;
	napart = 0;
	nfinish = 0;
	arenadonechan = chancreate(sizeof(void*), 0);
	for(i=0; i<ix->narenas; i++){
		if(ix->arenas[i]->part != p){
			p = ix->arenas[i]->part;
			vtproc(arenapartproc, p);
			++napart;
		}
	}

	/* wait for arena procs to finish */
	for(; nfinish<napart; nfinish++)
		recvp(arenadonechan);
	fprint(2, "%T done arenaentries=%,lld indexed=%,lld (nskip=%,lld)\n",
		arenaentries, indexentries, skipentries);
	unsigned int tsmax = 0, lsmax =0;
	unsigned int tsum = 0, lsum =0;
	unsigned int atsum = 0, alsum =0;
	for(int j=0;j<ntries;j++) {
		atsum += triedata[j].tmax;
		alsum += triedata[j].lmax;
		tsum += triedata[j].ts;
		lsum += triedata[j].ls;
		if(triedata[j].ts > tsmax) tsmax =triedata[j].ts;
		if(triedata[j].ls > lsmax) lsmax =triedata[j].ls;
	}
	fprint(2, "maxdepth:%,d\n", maxdepth);
	fprint(2, "all tsum:%,d, all lsum:%,d\n", atsum, alsum );
	fprint(2, "tsmax:%,d, tsum:%,d, lsmax:%,d, lsum:%,d\n", tsmax, tsum, lsmax, lsum );
	qlock(&trielock);
	free(scotable);
	scotable = 0;
	qunlock(&trielock);
}

/*
 * Read through an arena partition and send each of its IEntries
 * to the appropriate index section.  When finished, send on
 * arenadonechan.
 */
enum
{
	ClumpChunks = 32*1024,
};
static void
arenapartproc(void *v)
{
	int i, j, n, nskip;
	u32int clump;
	uvlong addr, tot;
	Arena *a;
	ClumpInfo *ci, *cis;
	IEntry ie;
	Part *p;

	p = v;
	threadsetname("arenaproc %s", p->name);

	nskip = 0;
	tot = 0;
	cis = MKN(ClumpInfo, ClumpChunks);
	for(i=0; i<ix->narenas; i++){
		a = ix->arenas[i];
		if(a->part != p)
			continue;
		if(a->memstats.clumps)
			fprint(2, "%T arena %s: %d entries\n",
				a->name, a->memstats.clumps);
		/*
		 * Running the loop backwards accesses the
		 * clump info blocks forwards, since they are
		 * stored in reverse order at the end of the arena.
		 * This speeds things slightly.
		 */
		addr = ix->amap[i].start + a->memstats.used;
		for(clump=a->memstats.clumps; clump > 0; clump-=n){
			n = ClumpChunks;
			if(n > clump)
				n = clump;
			if(readclumpinfos(a, clump-n, cis, n) != n){
				fprint(2, "%T arena %s: directory read: %r\n", a->name);
				errors = 1;
				break;
			}
			for(j=n-1; j>=0; j--){
				ci = &cis[j];
				ie.ia.type = ci->type;
				ie.ia.size = ci->uncsize;
				addr -= ci->size + ClumpSize;
				ie.ia.addr = addr;
				ie.ia.blocks = (ci->size + ClumpSize + (1<<ABlockLog)-1) >> ABlockLog;
				scorecp(ie.score, ci->score);
				if(ci->type == VtCorruptType)
					nskip++;
				else{
					(void)trie_insert(ie.score,&ie.ia.addr);
					tot++;
				}
			}
		}
		if(addr != ix->amap[i].start)
			fprint(2, "%T arena %s: clump miscalculation %ulld != %ulld\n", a->name, addr, ix->amap[i].start);
// this error makes mventi unusable
		assert(addr == ix->amap[i].start);
	}
	static Lock l;
	lock(&l);
	arenaentries += tot;
	skipentries += nskip;
	unlock(&l);

	free(cis);
	sendp(arenadonechan, p);
}
