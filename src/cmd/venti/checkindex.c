#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int
checkbucket(Index *ix, u32int buck, IBucket *ib)
{
	ISect *is;
	DBlock *eb;
	IBucket eib;
	IEntry ie, eie;
	int i, ei, ok, c;

	is = findibucket(ix, buck, &buck);
	eb = getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), 1);
	if(eb == nil)
		return -1;
	unpackibucket(&eib, eb->data);

	ok = 0;
	ei = 0;
	for(i = 0; i < ib->n; i++){
		while(ei < eib.n){
			c = ientrycmp(&ib->data[i * IEntrySize], &eib.data[ei * IEntrySize]);
			if(c == 0){
				unpackientry(&ie, &ib->data[i * IEntrySize]);
				unpackientry(&eie, &eib.data[ei * IEntrySize]);
				if(iaddrcmp(&ie.ia, &eie.ia) != 0){
					fprint(2, "bad entry in index for score=%V\n", &ib->data[i * IEntrySize]);
					fprint(2, "\taddr=%lld type=%d size=%d blocks=%d\n",
						ie.ia.addr, ie.ia.type, ie.ia.size, ie.ia.blocks);
					fprint(2, "\taddr=%lld type=%d size=%d blocks=%d\n",
						eie.ia.addr, eie.ia.type, eie.ia.size, eie.ia.blocks);
				}
				ei++;
				goto cont;
			}
			if(c < 0)
				break;
if(1)
			fprint(2, "spurious entry in index for score=%V type=%d\n",
				&eib.data[ei * IEntrySize], eib.data[ei * IEntrySize + IEntryTypeOff]);
			ei++;
			ok = -1;
		}
		fprint(2, "missing entry in index for score=%V type=%d\n",
			&ib->data[i * IEntrySize], ib->data[i * IEntrySize + IEntryTypeOff]);
		ok = -1;
	cont:;
	}
	for(; ei < eib.n; ei++){
if(1)		fprint(2, "spurious entry in index for score=%V; found %d entries expected %d\n",
			&eib.data[ei * IEntrySize], eib.n, ib->n);
		ok = -1;
		break;
	}
	putdblock(eb);
	return ok;
}

int
checkindex(Index *ix, Part *part, u64int off, u64int clumps, int zero)
{
	IEStream *ies;
	IBucket ib, zib;
	ZBlock *z, *b;
	u32int next, buck;
	int ok, bok;
u64int found = 0;

//ZZZ make buffer size configurable
	b = alloczblock(ix->blocksize, 0);
	z = alloczblock(ix->blocksize, 1);
	ies = initiestream(part, off, clumps, 64*1024);
	if(b == nil || z == nil || ies == nil){
		ok = -1;
		goto breakout;
		return -1;
	}
	ok = 0;
	next = 0;
	ib.data = b->data;
	zib.data = z->data;
	zib.n = 0;
	zib.depth = 0;
	for(;;){
		buck = buildbucket(ix, ies, &ib);
		found += ib.n;
		if(zero){
			for(; next != buck; next++){
				if(next == ix->buckets){
					if(buck != TWID32)
						fprint(2, "bucket out of range\n");
					goto breakout;
				}
				bok = checkbucket(ix, next, &zib);
				if(bok < 0){
					fprint(2, "bad bucket=%d found: %r\n", next);
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
		bok = checkbucket(ix, buck, &ib);
		if(bok < 0){
			fprint(2, "bad bucket found=%lld: %r\n", found);
			ok = -1;
		}
		next = buck + 1;
	}
breakout:;
fprint(2, "found %lld entries in sorted list\n", found);
	freeiestream(ies);
	freezblock(z);
	freezblock(b);
	return ok;
}

void
usage(void)
{
	fprint(2, "usage: checkindex [-f] [-B blockcachesize] config tmp\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	Part *part;
	u64int clumps, base;
	u32int bcmem;
	int fix, skipz;

	fix = 0;
	bcmem = 0;
	skipz = 0;
	ARGBEGIN{
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	case 'f':
		fix++;
		break;
	case 'Z':
		skipz = 1;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(!fix)
		readonly = 1;

	if(argc != 2)
		usage();

	if(initventi(argv[0]) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	part = initpart(argv[1], 1);
	if(part == nil)
		sysfatal("can't initialize temporary partition: %r");

	clumps = sortrawientries(mainindex, part, &base);
	if(clumps == TWID64)
		sysfatal("can't build sorted index: %r");
	fprint(2, "found and sorted index entries for clumps=%lld at %lld\n", clumps, base);
	checkindex(mainindex, part, base, clumps, !skipz);
	
	threadexitsall(0);
}
