#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	writeclumphead(Arena *arena, u64int aa, Clump *cl);
static int	writeclumpmagic(Arena *arena, u64int aa, u32int magic);

int
clumpinfocmp(ClumpInfo *c, ClumpInfo *d)
{
	return c->type != d->type
		|| c->size != d->size
		|| c->uncsize != d->uncsize
		|| scorecmp(c->score, d->score)!=0;
}

/*
 * synchronize the clump info directory with
 * with the clumps actually stored in the arena.
 * the directory should be at least as up to date
 * as the arena's trailer.
 *
 * checks/updates at most n clumps.
 *
 * returns 0 if ok, flags if error occurred
 */
int
syncarena(Arena *arena, u32int n, int zok, int fix)
{
	ZBlock *lump;
	Clump cl;
	ClumpInfo ci;
	static ClumpInfo zci = { .type = -1 };
	u8int score[VtScoreSize];
	u64int uncsize, used, aa;
	u32int clump, clumps, cclumps, magic;
	int err, flush, broken;

	used = arena->used;
	clumps = arena->clumps;
	cclumps = arena->cclumps;
	uncsize = arena->uncsize;

	flush = 0;
	err = 0;
	for(; n; n--){
		aa = arena->used;
		clump = arena->clumps;
		magic = clumpmagic(arena, aa);
		if(magic == ClumpFreeMagic)
			break;
		if(magic != ClumpMagic){
			fprint(2, "illegal clump magic number=%#8.8ux at clump=%d\n", magic, clump);
			err |= SyncDataErr;
//ZZZ write a zero here?
			if(0 && fix && writeclumpmagic(arena, aa, ClumpFreeMagic) < 0){
				fprint(2, "can't write corrected clump free magic: %r");
				err |= SyncFixErr;
			}
			break;
		}
		arena->clumps++;

		broken = 0;
		lump = loadclump(arena, aa, 0, &cl, score, 0);
		if(lump == nil){
			fprint(2, "clump=%d failed to read correctly: %r\n", clump);
			err |= SyncDataErr;
		}else if(cl.info.type != VtTypeCorrupt){
			scoremem(score, lump->data, cl.info.uncsize);
			if(scorecmp(cl.info.score, score) != 0){
				fprint(2, "clump=%d has mismatched score\n", clump);
				err |= SyncDataErr;
				broken = 1;
			}else if(vttypevalid(cl.info.type) < 0){
				fprint(2, "clump=%d has invalid type %d", clump, cl.info.type);
				err |= SyncDataErr;
				broken = 1;
			}
			if(broken && fix){
				cl.info.type = VtTypeCorrupt;
				if(writeclumphead(arena, aa, &cl) < 0){
					fprint(2, "can't write corrected clump header: %r");
					err |= SyncFixErr;
				}
			}
		}
		freezblock(lump);
		arena->used += ClumpSize + cl.info.size;

		if(!broken && readclumpinfo(arena, clump, &ci)<0){
			fprint(2, "arena directory read failed\n");
			broken = 1;
		}else if(!broken && clumpinfocmp(&ci, &cl.info)!=0){
			if(clumpinfocmp(&ci, &zci) == 0){
				err |= SyncCIZero;
				if(!zok)
					fprint(2, "unwritten clump info for clump=%d\n", clump);
			}else{
				err |= SyncCIErr;
				fprint(2, "bad clump info for clump=%d\n", clump);
				fprint(2, "\texpected score=%V type=%d size=%d uncsize=%d\n",
					cl.info.score, cl.info.type, cl.info.size, cl.info.uncsize);
				fprint(2, "\tfound score=%V type=%d size=%d uncsize=%d\n",
					ci.score, ci.type, ci.size, ci.uncsize);
			}
			broken = 1;
		}
		if(broken && fix){
			flush = 1;
			ci = cl.info;
			if(writeclumpinfo(arena, clump, &ci) < 0){
				fprint(2, "can't write correct clump directory: %r\n");
				err |= SyncFixErr;
			}
		}

		arena->uncsize += cl.info.uncsize;
		if(cl.info.size < cl.info.uncsize)
			arena->cclumps++;
	}

	if(flush){
		arena->wtime = now();
		if(arena->ctime == 0 && arena->clumps)
			arena->ctime = arena->wtime;
		if(flushciblocks(arena) < 0){
			fprint(2, "can't flush arena directory cache: %r");
			err |= SyncFixErr;
		}
	}

	if(used != arena->used
	|| clumps != arena->clumps
	|| cclumps != arena->cclumps
	|| uncsize != arena->uncsize)
		err |= SyncHeader;

	return err;
}

static int
writeclumphead(Arena *arena, u64int aa, Clump *cl)
{
	ZBlock *zb;
	int bad;

	zb = alloczblock(ClumpSize, 0);
	if(zb == nil)
		return -1;
	bad = packclump(cl, zb->data)<0
		|| writearena(arena, aa, zb->data, ClumpSize) != ClumpSize;
	freezblock(zb);
	return bad ? -1 : 0;
}

static int
writeclumpmagic(Arena *arena, u64int aa, u32int magic)
{
	u8int buf[U32Size];

	packmagic(magic, buf);
	return writearena(arena, aa, buf, U32Size) == U32Size;
}
