/*
 * Rebuild the Venti index from scratch.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"

/*
 * Write a single bucket.  Could profit from a big buffer here
 * so that we can absorb sporadic runs of blocks into one write,
 * avoiding disk seeks.
 */
static int
writebucket(Index *ix, u32int buck, IBucket *ib, ZBlock *b)
{
	ISect *is;

	is = ix->sects[indexsect0(ix, buck)];
	if(buck < is->start || buck >= is->stop){
		seterr(EAdmin, "cannot find index section for bucket %lud\n", (ulong)buck);
		return -1;
	}
	buck -= is->start;

/*
	qlock(&stats.lock);
	stats.indexwrites++;
	qunlock(&stats.lock);
*/
	packibucket(ib, b->data, is->bucketmagic);
	return writepart(is->part, is->blockbase + ((u64int)buck << is->blocklog), b->data, is->blocksize);
}

static int
buildindex(Index *ix, Part *part, u64int off, u64int clumps, int zero)
{
	IEStream *ies;
	IBucket ib, zib;
	ZBlock *z, *b;
	u32int next, buck;
	int ok;
	uint nbuck;
	u64int found = 0;

/*ZZZ make buffer size configurable */
	b = alloczblock(ix->blocksize, 0, ix->blocksize);
	z = alloczblock(ix->blocksize, 1, ix->blocksize);
	ies = initiestream(part, off, clumps, 64*1024);
	if(b == nil || z == nil || ies == nil){
		ok = 0;
		goto breakout;
		return -1;
	}
	ok = 0;
	next = 0;
	memset(&ib, 0, sizeof ib);
	ib.data = b->data + IBucketSize;
	zib.data = z->data + IBucketSize;
	zib.n = 0;
	nbuck = 0;
	for(;;){
		buck = buildbucket(ix, ies, &ib, ix->blocksize-IBucketSize);
		found += ib.n;
		if(zero){
			for(; next != buck; next++){
				if(next == ix->buckets){
					if(buck != TWID32){
						fprint(2, "bucket out of range\n");
						ok = -1;
					}
					goto breakout;
				}
				if(writebucket(ix, next, &zib, z) < 0){
					fprint(2, "can't write zero bucket to buck=%d: %r", next);
					ok = -1;
				}
			}
		}
		if(buck >= ix->buckets){
			if(buck == TWID32)
				break;
			fprint(2, "bucket out of range\n");
			ok = -1;
			goto breakout;
		}
		if(writebucket(ix, buck, &ib, b) < 0){
			fprint(2, "bad bucket found=%lld: %r\n", found);
			ok = -1;
		}
		next = buck + 1;
		if(++nbuck%10000 == 0)
			fprint(2, "\t%,d buckets written...\n", nbuck);
	}
breakout:;
	fprint(2, "wrote index with %lld entries\n", found);
	freeiestream(ies);
	freezblock(z);
	freezblock(b);
	return ok;
}

void
usage(void)
{
	fprint(2, "usage: buildindex [-Z] [-B blockcachesize] config tmppart\n");
	threadexitsall(0);
}

Config conf;

void
threadmain(int argc, char *argv[])
{
	Part *part;
	u64int clumps, base;
	u32int bcmem;
	int zero;

	zero = 1;
	bcmem = 0;
	ventifmtinstall();
	ARGBEGIN{
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	case 'Z':
		zero = 0;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 2)
		usage();

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	fprint(2, "building a new index %s using %s for temporary storage\n", mainindex->name, argv[1]);

	part = initpart(argv[1], ORDWR|ODIRECT);
	if(part == nil)
		sysfatal("can't initialize temporary partition: %r");

	clumps = sortrawientries(mainindex, part, &base, mainindex->bloom);
	if(clumps == TWID64)
		sysfatal("can't build sorted index: %r");
	fprint(2, "found and sorted index entries for clumps=%lld at %lld\n", clumps, base);

	if(buildindex(mainindex, part, base, clumps, zero) < 0)
		sysfatal("can't build new index: %r");
	
	if(mainindex->bloom)
		writebloom(mainindex->bloom);

	threadexitsall(0);
}
