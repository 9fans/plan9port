#include "stdinc.h"
#include "dat.h"
#include "fns.h"

enum
{
	ClumpChunks	= 32*1024
};

static int missing, wrong;

/*
 * shell sort is plenty good enough
 * because we're going to do a bunch of disk i/o's
 */
static void
sortclumpinfo(ClumpInfo *ci, int *s, int n)
{
	int i, j, m, t;

	for(m = (n + 3) / 5; m > 0; m = (m + 1) / 3){
		for(i = n - m; i-- > 0;){
			for(j = i + m; j < n; j += m){
				if(memcmp(ci[s[j - m]].score, ci[s[j]].score, VtScoreSize) <= 0)
					break;
				t = s[j];
				s[j] = s[j - m];
				s[j - m] = t;
			}
		}
	}
}

int
syncarenaindex(Index *ix, Arena *arena, u32int clump, u64int a, int fix, int *pflush, int check)
{
	Packet *pack;
	IEntry ie;
	IAddr ia;
	ClumpInfo *ci, *cis;
	u64int *addrs;
	int i, n, ok, *s, flush;

	trace(TraceProc, "syncarenaindex enter");

	flush = 0;
	cis = MKN(ClumpInfo, ClumpChunks);
	addrs = MKN(u64int, ClumpChunks);
	s = MKN(int, ClumpChunks);
	ok = 0;
	for(; clump < arena->memstats.clumps; clump += n){
		n = ClumpChunks;
		if(n > arena->memstats.clumps - clump)
			n = arena->memstats.clumps - clump;
		n = readclumpinfos(arena, clump, cis, n);
		if(n <= 0){
			fprint(2, "arena directory read failed\n");
			ok = -1;
			break;
		}

		for(i = 0; i < n; i++){
			addrs[i] = a;
			a += cis[i].size + ClumpSize;
			s[i] = i;
		}

		sortclumpinfo(cis, s, n);

		for(i = 0; i < n; i++){
			ci = &cis[s[i]];
			ia.type = ci->type;
			ia.size = ci->uncsize;
			ia.addr = addrs[s[i]];
			ia.blocks = (ci->size + ClumpSize + (1 << ABlockLog) - 1) >> ABlockLog;

			if(!check)
				goto Add;
			if(loadientry(ix, ci->score, ci->type, &ie) < 0){
				trace(TraceProc, "syncarenaindex missing block %V.%d", ci->score, ci->type);
				missing++;
			if(0)	fprint(2, "missing block type=%d score=%V\n", ci->type, ci->score);
			}else if(iaddrcmp(&ia, &ie.ia) != 0){
				trace(TraceProc, "syncarenaindex mismatched entry");
				fprint(2, "\nmismatched index entry and clump at %d\n", clump + i);
				fprint(2, "\tclump: type=%d size=%d blocks=%d addr=%lld\n", ia.type, ia.size, ia.blocks, ia.addr);
				fprint(2, "\tindex: type=%d size=%d block=%d addr=%lld\n", ie.ia.type, ie.ia.size, ie.ia.blocks, ie.ia.addr);
				pack = readlump(ie.score, ie.ia.type, ie.ia.size, nil);
				packetfree(pack);
				if(pack != nil){
					fprint(2, "duplicated lump\n");
					continue;
				}
				wrong++;
			}else
				continue;
		Add:
			if(!fix){
				ok = -1;
				continue;
			}
			flush = 1;
			trace(TraceProc, "syncarenaindex insert %V", ci->score);
			insertscore(ci->score, &ia, IEDirty);
		}

		if(0 && clump / 1000 != (clump + n) / 1000)
			fprint(2, ".");
	}
	free(cis);
	free(addrs);
	free(s);
	if(flush){
		flushdcache();
		*pflush = 1;
	}
	return ok;
}

int
syncindex(Index *ix, int fix, int mustflush, int check)
{
	Arena *arena;
	AState as;
	u64int a;
	int i, e, e1, ok, ok1, flush;

	ok = 0;
	flush = 0;
	for(i = 0; i < ix->narenas; i++){
		trace(TraceProc, "syncindex start %d", i);
		arena = ix->arenas[i];
		/*
		 * Syncarena will scan through the arena looking for blocks
		 * that have been forgotten.  It will update arena->memstats.used,
		 * so save the currenct copy as the place to start the 
		 * syncarenaindex scan.
		 */
		a = arena->memstats.used;
		e = syncarena(arena, ix->amap[i].start, TWID32, fix, fix);
		e1 = e;
		if(fix)
			e1 &= ~(SyncHeader|SyncCIZero|SyncCIErr);
		if(e1 == SyncHeader)
			fprint(2, "arena %s: header is out-of-date\n", arena->name);
		if(e1)
			ok = -1;
		else{
			/*
			 * use diskstats not memstats here, because diskstats
			 * is what has been indexed; memstats is what has 
			 * made it to disk (confusing names).
			 */
			ok1 = syncarenaindex(ix, arena,
					arena->diskstats.clumps,
					ix->amap[i].start + arena->diskstats.used,
					fix, &flush, check);
			if(ok1 < 0)
				fprint(2, "syncarenaindex: %r\n");
			if(fix && ok1==0 && (e & SyncHeader) && wbarena(arena) < 0)
				fprint(2, "arena=%s header write failed: %r\n", arena->name);
			ok |= ok1;

			as.arena = arena;
			as.aa = ix->amap[i].start + arena->memstats.used;
			as.stats = arena->memstats;
			setdcachestate(&as);
		}
	}
	if(missing || wrong)
		fprint(2, "syncindex: %d missing entries, %d wrong entries (flush=%d)\n", missing, wrong, flush);
	if(fix && wbindex(ix) < 0){
		fprint(2, "can't write back index header for %s: %r\n", ix->name);
		return -1;
	}
	if(fix && flush){
		flushdcache();
		if(mustflush){
			flushicache();
			flushdcache();
		}else
			kickicache();
	}
	return ok;
}
