#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int extra, missing, wrong;

static void
phdr(DBlock *eb)
{
	static int did;

	if(!did){
		did = 1;
		print("# diff actual correct\n");
	}
	print("%s block 0x%llux\n", eb->part->name, eb->addr);
}

static void
pie(IEntry *ie, char c)
{
	print("%c %V %22lld %3d %5d %3d\n",
		c, ie->score, ie->ia.addr, ie->ia.type, ie->ia.size, ie->ia.blocks);
}

static int
checkbucket(Index *ix, u32int buck, IBucket *ib)
{
	ISect *is;
	DBlock *eb;
	IBucket eib;
	IEntry ie, eie;
	int i, ei, ok, c, hdr;

	is = ix->sects[indexsect0(ix, buck)];
	if(buck < is->start || buck >= is->stop){
		seterr(EAdmin, "cannot find index section for bucket %lud\n", (ulong)buck);
		return -1;
	}
	buck -= is->start;
	eb = getdblock(is->part, is->blockbase + ((u64int)buck << is->blocklog), OREAD);
	if(eb == nil)
		return -1;
	unpackibucket(&eib, eb->data, is->bucketmagic);

	ok = 0;
	ei = 0;
	hdr = 0;
	for(i = 0; i < ib->n; i++){
		while(ei < eib.n){
			c = ientrycmp(&ib->data[i * IEntrySize], &eib.data[ei * IEntrySize]);
			if(c == 0){
				unpackientry(&ie, &ib->data[i * IEntrySize]);
				unpackientry(&eie, &eib.data[ei * IEntrySize]);
				if(iaddrcmp(&ie.ia, &eie.ia) != 0){
					if(!hdr){
						phdr(eb);
						hdr = 1;
					}
					wrong++;
					pie(&eie, '<');
					pie(&ie, '>');
				}
				ei++;
				goto cont;
			}
			if(c < 0)
				break;
			if(!hdr){
				phdr(eb);
				hdr = 1;
			}
			unpackientry(&eie, &eib.data[ei*IEntrySize]);
			extra++;
			pie(&eie, '<');
			ei++;
			ok = -1;
		}
		if(!hdr){
			phdr(eb);
			hdr = 1;
		}
		unpackientry(&ie, &ib->data[i*IEntrySize]);
		missing++;
		pie(&ie, '>');
		ok = -1;
	cont:;
	}
	for(; ei < eib.n; ei++){
		if(!hdr){
			phdr(eb);
			hdr = 1;
		}
		unpackientry(&eie, &eib.data[ei*IEntrySize]);
		pie(&eie, '<');
		ok = -1;
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

/* ZZZ make buffer size configurable */
	b = alloczblock(ix->blocksize, 0, ix->blocksize);
	z = alloczblock(ix->blocksize, 1, ix->blocksize);
	ies = initiestream(part, off, clumps, 64*1024);
	if(b == nil || z == nil || ies == nil){
		werrstr("allocating: %r");
		ok = -1;
		goto out;
	}
	ok = 0;
	next = 0;
	memset(&ib, 0, sizeof ib);
	ib.data = b->data;
	zib.data = z->data;
	zib.n = 0;
	zib.buck = 0;
	for(;;){
		buck = buildbucket(ix, ies, &ib, ix->blocksize-IBucketSize);
		found += ib.n;
		if(zero){
			for(; next != buck; next++){
				if(next == ix->buckets){
					if(buck != TWID32){
						ok = -1;
						werrstr("internal error: bucket out of range");
					}
					if(ok < 0)
						werrstr("%d spurious entries, %d missing, %d wrong", extra, missing, wrong);
					goto out;
				}
				bok = checkbucket(ix, next, &zib);
				if(bok < 0)
					ok = -1;
			}
		}
		if(buck >= ix->buckets){
			if(buck == TWID32)
				break;
			werrstr("internal error: bucket out of range");
			ok = -1;
			goto out;
		}
		bok = checkbucket(ix, buck, &ib);
		if(bok < 0)
			ok = -1;
		next = buck + 1;
	}
out:
	freeiestream(ies);
	freezblock(z);
	freezblock(b);
	return ok;
}

int
checkbloom(Bloom *b1, Bloom *b2, int fix)
{
	u32int *a1, *a2;
	int i, n, extra, missing;

	if(b1==nil && b2==nil)
		return 0;
	if(b1==nil || b2==nil){
		werrstr("nil/non-nil");
		return -1;
	}
	wbbloomhead(b1);
	wbbloomhead(b2);
	if(memcmp(b1->data, b2->data, BloomHeadSize) != 0){
		werrstr("bloom header mismatch");
		return -1;
	}
	a1 = (u32int*)b1->data;
	a2 = (u32int*)b2->data;
	n = b1->size/4;
	extra = 0;
	missing = 0;
	for(i=BloomHeadSize/4; i<n; i++){
		if(a1[i] != a2[i]){
// print("%.8ux/%.8ux.", a1[i], a2[i]);
			extra   += countbits(a1[i] & ~a2[i]);
			missing += countbits(a2[i] & ~a1[i]);
		}
	}
	if(extra || missing)
		fprint(2, "bloom filter: %d spurious bits, %d missing bits\n",
			extra, missing);
	else
		fprint(2, "bloom filter: correct\n");
	if(!fix && missing){
		werrstr("missing bits");
		return -1;
	}
	if(fix && (missing || extra)){
		memmove(b1->data, b2->data, b1->size);
		return writebloom(b1);
	}
	return 0;
}


void
usage(void)
{
	fprint(2, "usage: checkindex [-f] [-B blockcachesize] config tmp\n");
	threadexitsall(0);
}

Config conf;

void
threadmain(int argc, char *argv[])
{
	Bloom *oldbloom, *newbloom;
	Part *part;
	u64int clumps, base;
	u32int bcmem;
	int fix, skipz, ok;

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

	if(argc != 2)
		usage();

	ventifmtinstall();

	part = initpart(argv[1], ORDWR|ODIRECT);
	if(part == nil)
		sysfatal("can't initialize temporary partition: %r");

	if(!fix)
		readonly = 1;

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");
	if(mainindex->bloom && loadbloom(mainindex->bloom) < 0)
		sysfatal("can't load bloom filter: %r");
	oldbloom = mainindex->bloom;
	newbloom = nil;
	if(oldbloom){
		newbloom = vtmallocz(sizeof *newbloom);
		bloominit(newbloom, oldbloom->size, nil);
		newbloom->data = vtmallocz(oldbloom->size);
	}
	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	fprint(2, "checkindex: building entry list\n");
	clumps = sortrawientries(mainindex, part, &base, newbloom);
	if(clumps == TWID64)
		sysfatal("can't build sorted index: %r");
	fprint(2, "checkindex: checking %lld entries at %lld\n", clumps, base);
	ok = 0;
	if(checkindex(mainindex, part, base, clumps, !skipz) < 0){
		fprint(2, "checkindex: %r\n");
		ok = -1;
	}
	if(checkbloom(oldbloom, newbloom, fix) < 0){
		fprint(2, "checkbloom: %r\n");
		ok = -1;
	}
	if(ok < 0)
		sysfatal("errors found");
	fprint(2, "checkindex: index is correct\n");
	threadexitsall(0);
}
