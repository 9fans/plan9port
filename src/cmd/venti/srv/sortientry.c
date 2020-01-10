#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include <bio.h>

typedef struct IEBuck	IEBuck;
typedef struct IEBucks	IEBucks;

enum
{
	ClumpChunks	= 32*1024
};

struct IEBuck
{
	u32int	head;		/* head of chain of chunks on the disk */
	u32int	used;		/* usage of the last chunk */
	u64int	total;		/* total number of bytes in this bucket */
	u8int	*buf;		/* chunk of entries for this bucket */
};

struct IEBucks
{
	Part	*part;
	u64int	off;		/* offset for writing data in the partition */
	u32int	chunks;		/* total chunks written to fd */
	u64int	max;		/* max bytes entered in any one bucket */
	int	bits;		/* number of bits in initial bucket sort */
	int	nbucks;		/* 1 << bits, the number of buckets */
	u32int	size;		/* bytes in each of the buckets chunks */
	u32int	usable;		/* amount usable for IEntry data */
	u8int	*buf;		/* buffer for all chunks */
	u8int	*xbuf;
	IEBuck	*bucks;
};

#define	U32GET(p)	(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3])
#define	U32PUT(p,v)	(p)[0]=(v)>>24;(p)[1]=(v)>>16;(p)[2]=(v)>>8;(p)[3]=(v)

static IEBucks	*initiebucks(Part *part, int bits, u32int size);
static int	flushiebuck(IEBucks *ib, int b, int reset);
static int	flushiebucks(IEBucks *ib);
static u32int	sortiebuck(IEBucks *ib, int b);
static u64int	sortiebucks(IEBucks *ib);
static int	sprayientry(IEBucks *ib, IEntry *ie);
static u32int	readarenainfo(IEBucks *ib, Arena *arena, u64int a, Bloom *b);
static u32int	readiebuck(IEBucks *ib, int b);
static void	freeiebucks(IEBucks *ib);

/*
 * build a sorted file with all IEntries which should be in ix.
 * assumes the arenas' directories are up to date.
 * reads each, converts the entries to index entries,
 * and sorts them.
 */
u64int
sortrawientries(Index *ix, Part *tmp, u64int *base, Bloom *bloom)
{
	IEBucks *ib;
	u64int clumps, sorted;
	u32int n;
	int i, ok;

/* ZZZ should allow configuration of bits, bucket size */
	ib = initiebucks(tmp, 8, 64*1024);
	if(ib == nil){
		seterr(EOk, "can't create sorting buckets: %r");
		return TWID64;
	}
	ok = 0;
	clumps = 0;
	fprint(2, "constructing entry list\n");
	for(i = 0; i < ix->narenas; i++){
		n = readarenainfo(ib, ix->arenas[i], ix->amap[i].start, bloom);
		if(n == TWID32){
			ok = -1;
			break;
		}
		clumps += n;
	}
	fprint(2, "sorting %lld entries\n", clumps);
	if(ok == 0){
		sorted = sortiebucks(ib);
		*base = (u64int)ib->chunks * ib->size;
		if(sorted != clumps){
			fprint(2, "sorting messed up: clumps=%lld sorted=%lld\n", clumps, sorted);
			ok = -1;
		}
	}
	freeiebucks(ib);
	if(ok < 0)
		return TWID64;
	return clumps;
}

#define CHECK(cis)	if(((ulong*)cis)[-4] != 0xA110C09) xabort();

void
xabort(void)
{
	int *x;

	x = 0;
	*x = 0;
}

/*
 * read in all of the arena's clump directory,
 * convert to IEntry format, and bucket sort based
 * on the first few bits.
 */
static u32int
readarenainfo(IEBucks *ib, Arena *arena, u64int a, Bloom *b)
{
	IEntry ie;
	ClumpInfo *ci, *cis;
	u32int clump;
	int i, n, ok, nskip;

	if(arena->memstats.clumps)
		fprint(2, "\tarena %s: %d entries\n", arena->name, arena->memstats.clumps);
	else
		fprint(2, "[%s] ", arena->name);

	cis = MKN(ClumpInfo, ClumpChunks);
	ok = 0;
	nskip = 0;
	memset(&ie, 0, sizeof(IEntry));
	for(clump = 0; clump < arena->memstats.clumps; clump += n){
		n = ClumpChunks;
		if(n > arena->memstats.clumps - clump)
			n = arena->memstats.clumps - clump;
		if(readclumpinfos(arena, clump, cis, n) != n){
			seterr(EOk, "arena directory read failed: %r");
			ok = -1;
			break;
		}

		for(i = 0; i < n; i++){
			ci = &cis[i];
			ie.ia.type = ci->type;
			ie.ia.size = ci->uncsize;
			ie.ia.addr = a;
			a += ci->size + ClumpSize;
			ie.ia.blocks = (ci->size + ClumpSize + (1 << ABlockLog) - 1) >> ABlockLog;
			scorecp(ie.score, ci->score);
			if(ci->type == VtCorruptType){
				if(0) print("! %V %22lld %3d %5d %3d\n",
					ie.score, ie.ia.addr, ie.ia.type, ie.ia.size, ie.ia.blocks);
				nskip++;
			}else
				sprayientry(ib, &ie);
			markbloomfilter(b, ie.score);
		}
	}
	free(cis);
	if(ok < 0)
		return TWID32;
	return clump - nskip;
}

/*
 * initialize the external bucket sorting data structures
 */
static IEBucks*
initiebucks(Part *part, int bits, u32int size)
{
	IEBucks *ib;
	int i;

	ib = MKZ(IEBucks);
	if(ib == nil){
		seterr(EOk, "out of memory");
		return nil;
	}
	ib->bits = bits;
	ib->nbucks = 1 << bits;
	ib->size = size;
	ib->usable = (size - U32Size) / IEntrySize * IEntrySize;
	ib->bucks = MKNZ(IEBuck, ib->nbucks);
	if(ib->bucks == nil){
		seterr(EOk, "out of memory allocation sorting buckets");
		freeiebucks(ib);
		return nil;
	}
	ib->xbuf = MKN(u8int, size * ((1 << bits)+1));
	ib->buf = (u8int*)(((uintptr)ib->xbuf+size-1)&~(uintptr)(size-1));
	if(ib->buf == nil){
		seterr(EOk, "out of memory allocating sorting buckets' buffers");
		freeiebucks(ib);
		return nil;
	}
	for(i = 0; i < ib->nbucks; i++){
		ib->bucks[i].head = TWID32;
		ib->bucks[i].buf = &ib->buf[i * size];
	}
	ib->part = part;
	return ib;
}

static void
freeiebucks(IEBucks *ib)
{
	if(ib == nil)
		return;
	free(ib->bucks);
	free(ib->buf);
	free(ib);
}

/*
 * initial sort: put the entry into the correct bucket
 */
static int
sprayientry(IEBucks *ib, IEntry *ie)
{
	u32int n;
	int b;

	b = hashbits(ie->score, ib->bits);
	n = ib->bucks[b].used;
	if(n + IEntrySize > ib->usable){
		/* should be flushed below, but if flush fails, this can happen */
		seterr(EOk, "out of space in bucket");
		return -1;
	}
	packientry(ie, &ib->bucks[b].buf[n]);
	n += IEntrySize;
	ib->bucks[b].used = n;
	if(n + IEntrySize <= ib->usable)
		return 0;
	return flushiebuck(ib, b, 1);
}

/*
 * finish sorting:
 * for each bucket, read it in and sort it
 * write out the the final file
 */
static u64int
sortiebucks(IEBucks *ib)
{
	u64int tot;
	u32int n;
	int i;

	if(flushiebucks(ib) < 0)
		return TWID64;
	for(i = 0; i < ib->nbucks; i++)
		ib->bucks[i].buf = nil;
	ib->off = (u64int)ib->chunks * ib->size;
	free(ib->xbuf);

	ib->buf = MKN(u8int, ib->max + U32Size);
	if(ib->buf == nil){
		seterr(EOk, "out of memory allocating final sorting buffer; try more buckets");
		return TWID64;
	}
	tot = 0;
	for(i = 0; i < ib->nbucks; i++){
		n = sortiebuck(ib, i);
		if(n == TWID32)
			return TWID64;
		if(n != ib->bucks[i].total/IEntrySize)
			fprint(2, "bucket %d changed count %d => %d\n",
				i, (int)(ib->bucks[i].total/IEntrySize), n);
		tot += n;
	}
	return tot;
}

/*
 * sort from bucket b of ib into the output file to
 */
static u32int
sortiebuck(IEBucks *ib, int b)
{
	u32int n;

	n = readiebuck(ib, b);
	if(n == TWID32)
		return TWID32;
	qsort(ib->buf, n, IEntrySize, ientrycmp);
	if(writepart(ib->part, ib->off, ib->buf, n*IEntrySize) < 0){
		seterr(EOk, "can't write sorted bucket: %r");
		return TWID32;
	}
	ib->off += n * IEntrySize;
	return n;
}

/*
 * write out a single bucket
 */
static int
flushiebuck(IEBucks *ib, int b, int reset)
{
	u32int n;

	if(ib->bucks[b].used == 0)
		return 0;
	n = ib->bucks[b].used;
	U32PUT(&ib->bucks[b].buf[n], ib->bucks[b].head);
	n += U32Size;
	USED(n);
	if(writepart(ib->part, (u64int)ib->chunks * ib->size, ib->bucks[b].buf, ib->size) < 0){
		seterr(EOk, "can't write sorting bucket to file: %r");
xabort();
		return -1;
	}
	ib->bucks[b].head = ib->chunks++;
	ib->bucks[b].total += ib->bucks[b].used;
	if(reset)
		ib->bucks[b].used = 0;
	return 0;
}

/*
 * write out all of the buckets, and compute
 * the maximum size of any bucket
 */
static int
flushiebucks(IEBucks *ib)
{
	int i;

	for(i = 0; i < ib->nbucks; i++){
		if(flushiebuck(ib, i, 0) < 0)
			return -1;
		if(ib->bucks[i].total > ib->max)
			ib->max = ib->bucks[i].total;
	}
	return 0;
}

/*
 * read in the chained buffers for bucket b,
 * and return it's total number of IEntries
 */
static u32int
readiebuck(IEBucks *ib, int b)
{
	u32int head, m, n;

	head = ib->bucks[b].head;
	n = 0;
	m = ib->bucks[b].used;
	if(m == 0)
		m = ib->usable;
	if(0) if(ib->bucks[b].total)
		fprint(2, "\tbucket %d: %lld entries\n", b, ib->bucks[b].total/IEntrySize);
	while(head != TWID32){
		if(readpart(ib->part, (u64int)head * ib->size, &ib->buf[n], m+U32Size) < 0){
			seterr(EOk, "can't read index sort bucket: %r");
			return TWID32;
		}
		n += m;
		head = U32GET(&ib->buf[n]);
		m = ib->usable;
	}
	if(n != ib->bucks[b].total)
		fprint(2, "\tbucket %d: expected %d entries, got %d\n",
			b, (int)ib->bucks[b].total/IEntrySize, n/IEntrySize);
	return n / IEntrySize;
}
