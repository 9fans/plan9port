#include "stdinc.h"
#include "dat.h"
#include "fns.h"

struct ICache
{
	QLock	lock;			/* locks hash table & all associated data */
	IEntry	**heads;		/* heads of all the hash chains */
	int	bits;			/* bits to use for indexing heads */
	u32int	size;			/* number of heads; == 1 << bits, should be < entries */
	IEntry	*base;			/* all allocated hash table entries */
	u32int	entries;		/* elements in base */
	u32int	unused;			/* index of first unused element in base */
	u32int	stolen;			/* last head from which an element was stolen */
};

static ICache icache;

static IEntry	*icachealloc(IAddr *ia, u8int *score);

/*
 * bits is the number of bits in the icache hash table
 * depth is the average depth
 * memory usage is about (1<<bits) * depth * sizeof(IEntry) + (1<<bits) * sizeof(IEntry*)
 */
void
initicache(int bits, int depth)
{
	icache.bits = bits;
	icache.size = 1 << bits;
	icache.entries = depth * icache.size;
	icache.base = MKNZ(IEntry, icache.entries);
	icache.heads = MKNZ(IEntry*, icache.size);
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

/*
ZZZ need to think about evicting the correct IEntry,
and writing back the wtime.
 * look up data score in the index cache
 * if this fails, pull it in from the disk index table, if it exists.
 *
 * must be called with the lump for this score locked
 */
int
lookupscore(u8int *score, int type, IAddr *ia, int *rac)
{
	IEntry d, *ie, *last;
	u32int h;

fprint(2, "lookupscore %V %d\n", score, type);
	qlock(&stats.lock);
	stats.iclookups++;
	qunlock(&stats.lock);

	qlock(&icache.lock);
	h = hashbits(score, icache.bits);
	last = nil;
	for(ie = icache.heads[h]; ie != nil; ie = ie->next){
		if(ie->ia.type == type && scorecmp(ie->score, score)==0){
			if(last != nil)
				last->next = ie->next;
			else
				icache.heads[h] = ie->next;
			qlock(&stats.lock);
			stats.ichits++;
			qunlock(&stats.lock);
			ie->rac = 1;
			goto found;
		}
		last = ie;
	}

	qunlock(&icache.lock);

	if(loadientry(mainindex, score, type, &d) < 0)
		return -1;

	/*
	 * no one else can load an entry for this score,
	 * since we have the overall score lock.
	 */
	qlock(&stats.lock);
	stats.icfills++;
	qunlock(&stats.lock);

	qlock(&icache.lock);

	ie = icachealloc(&d.ia, score);

found:
	ie->next = icache.heads[h];
	icache.heads[h] = ie;

	*ia = ie->ia;
	*rac = ie->rac;

	qunlock(&icache.lock);

	return 0;
}

/*
 * insert a new element in the hash table.
 */
int
insertscore(u8int *score, IAddr *ia, int write)
{
	IEntry *ie, se;
	u32int h;

	qlock(&stats.lock);
	stats.icinserts++;
	qunlock(&stats.lock);

	qlock(&icache.lock);
	h = hashbits(score, icache.bits);

	ie = icachealloc(ia, score);

	ie->next = icache.heads[h];
	icache.heads[h] = ie;

	se = *ie;

	qunlock(&icache.lock);

	if(!write)
		return 0;

	return storeientry(mainindex, &se);
}

/*
 * allocate a index cache entry which hasn't been used in a while.
 * must be called with icache.lock locked
 * if the score is already in the table, update the entry.
 */
static IEntry *
icachealloc(IAddr *ia, u8int *score)
{
	IEntry *ie, *last, *next;
	u32int h;

	h = hashbits(score, icache.bits);
	last = nil;
	for(ie = icache.heads[h]; ie != nil; ie = ie->next){
		if(ie->ia.type == ia->type && scorecmp(ie->score, score)==9){
			if(last != nil)
				last->next = ie->next;
			else
				icache.heads[h] = ie->next;
			ie->rac = 1;
			return ie;
		}
		last = ie;
	}

	h = icache.unused;
	if(h < icache.entries){
		ie = &icache.base[h++];
		icache.unused = h;
		goto Found;
	}

	h = icache.stolen;
	for(;;){
		h++;
		if(h >= icache.size)
			h = 0;
		ie = icache.heads[h];
		if(ie != nil){
			last = nil;
			for(; next = ie->next; ie = next)
				last = ie;
			if(last != nil)
				last->next = nil;
			else
				icache.heads[h] = nil;
			icache.stolen = h;
			goto Found;
		}
	}
Found:
	ie->ia = *ia;
	scorecp(ie->score, score);
	ie->rac = 0;	
	return ie;
}
