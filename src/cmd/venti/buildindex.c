#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int
writebucket(Index *ix, u32int buck, IBucket *ib, ZBlock *b)
{
	ISect *is;

	is = findisect(ix, buck);
	if(is == nil){
		seterr(EAdmin, "bad math in writebucket");
		return -1;
	}
	if(buck < is->start || buck >= is->stop)
		seterr(EAdmin, "index write out of bounds: %d not in [%d,%d)\n",
				buck, is->start, is->stop);
	buck -= is->start;
	qlock(&stats.lock);
	stats.indexwrites++;
	qunlock(&stats.lock);
	packibucket(ib, b->data);
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
	u64int found = 0;

//ZZZ make buffer size configurable
	b = alloczblock(ix->blocksize, 0);
	z = alloczblock(ix->blocksize, 1);
	ies = initiestream(part, off, clumps, 64*1024);
	if(b == nil || z == nil || ies == nil){
		ok = 0;
		goto breakout;
		return -1;
	}
	ok = 0;
	next = 0;
	ib.data = b->data + IBucketSize;
	zib.data = z->data + IBucketSize;
	zib.n = 0;
	zib.next = 0;
	for(;;){
		buck = buildbucket(ix, ies, &ib);
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
	}
breakout:;
	fprint(2, "constructed index with %lld entries\n", found);
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

void
threadmain(int argc, char *argv[])
{
	Part *part;
	u64int clumps, base;
	u32int bcmem;
	int zero;

	zero = 1;
	bcmem = 0;
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

	if(initventi(argv[0]) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	fprint(2, "building a new index %s using %s for temporary storage\n", mainindex->name, argv[1]);

	part = initpart(argv[1], 1);
	if(part == nil)
		sysfatal("can't initialize temporary partition: %r");

	clumps = sortrawientries(mainindex, part, &base);
	if(clumps == TWID64)
		sysfatal("can't build sorted index: %r");
	fprint(2, "found and sorted index entries for clumps=%lld at %lld\n", clumps, base);

	if(buildindex(mainindex, part, base, clumps, zero) < 0)
		sysfatal("can't build new index: %r");
	
	threadexitsall(0);
}
