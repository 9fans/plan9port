#include "stdinc.h"
#include "dat.h"
#include "fns.h"

int			queuewrites = 0;

static Packet		*readilump(Lump *u, IAddr *ia, u8int *score, int rac);

Packet*
readlump(u8int *score, int type, u32int size)
{
	Lump *u;
	Packet *p;
	IAddr ia;
	u32int n;
	int rac;

	qlock(&stats.lock);
	stats.lumpreads++;
	qunlock(&stats.lock);
	u = lookuplump(score, type);
	if(u->data != nil){
		n = packetsize(u->data);
		if(n > size){
			seterr(EOk, "read too small: asked for %d need at least %d", size, n);
			putlump(u);

			return nil;
		}
		p = packetdup(u->data, 0, n);
		putlump(u);

		return p;
	}

	if(lookupscore(score, type, &ia, &rac) < 0){
		//ZZZ place to check for someone trying to guess scores
		seterr(EOk, "no block with that score exists");

		putlump(u);
		return nil;
	}
	if(ia.size > size){
		seterr(EOk, "read too small 1: asked for %d need at least %d", size, ia.size);

		putlump(u);
		return nil;
	}

	p = readilump(u, &ia, score, rac);
	putlump(u);

	return p;
}

/*
 * save away a lump, and return it's score.
 * doesn't store duplicates, but checks that the data is really the same.
 */
int
writelump(Packet *p, u8int *score, int type, u32int creator)
{
	Lump *u;
	int ok;

	qlock(&stats.lock);
	stats.lumpwrites++;
	qunlock(&stats.lock);

	packetsha1(p, score);

	u = lookuplump(score, type);
	if(u->data != nil){
		ok = 0;
		if(packetcmp(p, u->data) != 0){
			seterr(EStrange, "score collision");
			ok = -1;
		}
		packetfree(p);
		putlump(u);
		return ok;
	}

	if(queuewrites)
		return queuewrite(u, p, creator);

	ok = writeqlump(u, p, creator);

	putlump(u);
	return ok;
}

int
writeqlump(Lump *u, Packet *p, int creator)
{
	ZBlock *flat;
	Packet *old;
	IAddr ia;
	int ok;
	int rac;

	if(lookupscore(u->score, u->type, &ia, &rac) == 0){
		/*
		 * if the read fails,
		 * assume it was corrupted data and store the block again
		 */
		old = readilump(u, &ia, u->score, rac);
		if(old != nil){
			ok = 0;
			if(packetcmp(p, old) != 0){
				seterr(EStrange, "score collision");
				ok = -1;
			}
			packetfree(p);
			packetfree(old);

			return ok;
		}
		logerr(EAdmin, "writelump: read %V failed, rewriting: %r\n", u->score);
	}

	flat = packet2zblock(p, packetsize(p));
	ok = storeclump(mainindex, flat, u->score, u->type, creator, &ia);
	freezblock(flat);
	if(ok == 0)
		ok = insertscore(u->score, &ia, 1);
	if(ok == 0)
		insertlump(u, p);
	else
		packetfree(p);

	return ok;
}

static void
readahead(u64int a, Arena *arena, u64int aa, int n)
{	
	u8int buf[ClumpSize];
	Clump cl;
	IAddr ia;

	while(n > 0) {
		if (aa >= arena->used)
			break;
		if(readarena(arena, aa, buf, ClumpSize) < ClumpSize)
			break;
		if(unpackclump(&cl, buf) < 0)
			break;
		ia.addr = a;
		ia.type = cl.info.type;
		ia.size = cl.info.uncsize;
		ia.blocks = (cl.info.size + ClumpSize + (1 << ABlockLog) - 1) >> ABlockLog;
		insertscore(cl.info.score, &ia, 0);
		a += ClumpSize + cl.info.size;
		aa += ClumpSize + cl.info.size;
		n--;
	}
}

static Packet*
readilump(Lump *u, IAddr *ia, u8int *score, int rac)
{
	Arena *arena;
	ZBlock *zb;
	Packet *p, *pp;
	Clump cl;
	u64int a, aa;
	u8int sc[VtScoreSize];

	arena = amapitoa(mainindex, ia->addr, &aa);
	if(arena == nil)
		return nil;

	zb = loadclump(arena, aa, ia->blocks, &cl, sc, paranoid);
	if(zb == nil)
		return nil;

	if(ia->size != cl.info.uncsize){
		seterr(EInconsist, "index and clump size mismatch");
		freezblock(zb);
		return nil;
	}
	if(ia->type != cl.info.type){
		seterr(EInconsist, "index and clump type mismatch");
		freezblock(zb);
		return nil;
	}
	if(scorecmp(score, sc) != 0){
		seterr(ECrash, "score mismatch");
		freezblock(zb);
		return nil;
	}

	if(rac == 0) {
		a = ia->addr + ClumpSize + cl.info.size;
		aa += ClumpSize + cl.info.size;
		readahead(a, arena, aa, 20);
	}

	p = zblock2packet(zb, cl.info.uncsize);
	freezblock(zb);
	pp = packetdup(p, 0, packetsize(p));
	insertlump(u, pp);
	return p;
}
