#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "whack.h"

/*
 * Write a lump to disk.  Updates ia with an index address
 * for the newly-written lump.  Upon return, the lump will
 * have been placed in the disk cache but will likely not be on disk yet.
 */
int
storeclump(Index *ix, ZBlock *zb, u8int *sc, int type, u32int creator, IAddr *ia)
{
	ZBlock *cb;
	Clump cl;
	u64int a;
	u8int bh[VtScoreSize];
	int size, dsize;

	trace(TraceLump, "storeclump enter", sc, type);
	size = zb->len;
	if(size > VtMaxLumpSize){
		seterr(EStrange, "lump too large");
		return -1;
	}
	if(vttypevalid(type) < 0){
		seterr(EStrange, "invalid lump type");
		return -1;
	}

	if(0){
		scoremem(bh, zb->data, size);
		if(scorecmp(sc, bh) != 0){
			seterr(ECorrupt, "storing clump: corrupted; expected=%V got=%V, size=%d", sc, bh, size);
			return -1;
		}
	}

	cb = alloczblock(size + ClumpSize + U32Size, 0, 0);
	if(cb == nil)
		return -1;

	cl.info.type = type;
	cl.info.uncsize = size;
	cl.creator = creator;
	cl.time = now();
	scorecp(cl.info.score, sc);

	trace(TraceLump, "storeclump whackblock");
	dsize = whackblock(&cb->data[ClumpSize], zb->data, size);
	if(dsize > 0 && dsize < size){
		cl.encoding = ClumpECompress;
	}else{
		if(dsize > size){
			fprint(2, "whack error: dsize=%d size=%d\n", dsize, size);
			abort();
		}
		cl.encoding = ClumpENone;
		dsize = size;
		memmove(&cb->data[ClumpSize], zb->data, size);
	}
	memset(cb->data+ClumpSize+dsize, 0, 4);
	cl.info.size = dsize;

	a = writeiclump(ix, &cl, cb->data);
	trace(TraceLump, "storeclump exit %lld", a);
	freezblock(cb);
	if(a == TWID64)
		return -1;

	ia->addr = a;
	ia->type = type;
	ia->size = size;
	ia->blocks = (dsize + ClumpSize + (1 << ABlockLog) - 1) >> ABlockLog;

/*
	qlock(&stats.lock);
	stats.clumpwrites++;
	stats.clumpbwrites += size;
	stats.clumpbcomp += dsize;
	qunlock(&stats.lock);
*/

	return 0;
}

u32int
clumpmagic(Arena *arena, u64int aa)
{
	u8int buf[U32Size];

	if(readarena(arena, aa, buf, U32Size) == TWID32)
		return TWID32;
	return unpackmagic(buf);
}

/*
 * fetch a block based at addr.
 * score is filled in with the block's score.
 * blocks is roughly the length of the clump on disk;
 * if zero, the length is unknown.
 */
ZBlock*
loadclump(Arena *arena, u64int aa, int blocks, Clump *cl, u8int *score, int verify)
{
	Unwhack uw;
	ZBlock *zb, *cb;
	u8int bh[VtScoreSize], *buf;
	u32int n;
	int nunc;

/*
	qlock(&stats.lock);
	stats.clumpreads++;
	qunlock(&stats.lock);
*/

	if(blocks <= 0)
		blocks = 1;

	trace(TraceLump, "loadclump enter");

	cb = alloczblock(blocks << ABlockLog, 0, 0);
	if(cb == nil)
		return nil;
	n = readarena(arena, aa, cb->data, blocks << ABlockLog);
	if(n < ClumpSize){
		if(n != 0)
			seterr(ECorrupt, "loadclump read less than a header");
		freezblock(cb);
		return nil;
	}
	trace(TraceLump, "loadclump unpack");
	if(unpackclump(cl, cb->data, arena->clumpmagic) < 0){
		seterr(ECorrupt, "loadclump %s %llud: %r", arena->name, aa);
		freezblock(cb);
		return nil;
	}
	if(cl->info.type == VtCorruptType){
		seterr(EOk, "clump is marked corrupt");
		freezblock(cb);
		return nil;
	}
	n -= ClumpSize;
	if(n < cl->info.size){
		freezblock(cb);
		n = cl->info.size;
		cb = alloczblock(n, 0, 0);
		if(cb == nil)
			return nil;
		if(readarena(arena, aa + ClumpSize, cb->data, n) != n){
			seterr(ECorrupt, "loadclump read too little data");
			freezblock(cb);
			return nil;
		}
		buf = cb->data;
	}else
		buf = cb->data + ClumpSize;

	scorecp(score, cl->info.score);

	zb = alloczblock(cl->info.uncsize, 0, 0);
	if(zb == nil){
		freezblock(cb);
		return nil;
	}
	switch(cl->encoding){
	case ClumpECompress:
		trace(TraceLump, "loadclump decompress");
		unwhackinit(&uw);
		nunc = unwhack(&uw, zb->data, cl->info.uncsize, buf, cl->info.size);
		if(nunc != cl->info.uncsize){
			if(nunc < 0)
				seterr(ECorrupt, "decompression of %llud failed: %s", aa, uw.err);
			else
				seterr(ECorrupt, "decompression of %llud gave partial block: %d/%d\n", aa, nunc, cl->info.uncsize);
			freezblock(cb);
			freezblock(zb);
			return nil;
		}
		break;
	case ClumpENone:
		if(cl->info.size != cl->info.uncsize){
			seterr(ECorrupt, "loading clump: bad uncompressed size for uncompressed block %llud", aa);
			freezblock(cb);
			freezblock(zb);
			return nil;
		}
		scoremem(bh, buf, cl->info.uncsize);
		if(scorecmp(cl->info.score, bh) != 0)
			seterr(ECorrupt, "pre-copy sha1 wrong at %s %llud: expected=%V got=%V", arena->name, aa, cl->info.score, bh);
		memmove(zb->data, buf, cl->info.uncsize);
		break;
	default:
		seterr(ECorrupt, "unknown encoding in loadlump %llud", aa);
		freezblock(cb);
		freezblock(zb);
		return nil;
	}
	freezblock(cb);

	if(verify){
		trace(TraceLump, "loadclump verify");
		scoremem(bh, zb->data, cl->info.uncsize);
		if(scorecmp(cl->info.score, bh) != 0){
			seterr(ECorrupt, "loading clump: corrupted at %s %llud; expected=%V got=%V", arena->name, aa, cl->info.score, bh);
			freezblock(zb);
			return nil;
		}
		if(vttypevalid(cl->info.type) < 0){
			seterr(ECorrupt, "loading lump at %s %llud: invalid lump type %d", arena->name, aa, cl->info.type);
			freezblock(zb);
			return nil;
		}
	}

	trace(TraceLump, "loadclump exit");
/*
	qlock(&stats.lock);
	stats.clumpbreads += cl->info.size;
	stats.clumpbuncomp += cl->info.uncsize;
	qunlock(&stats.lock);
*/
	return zb;
}
