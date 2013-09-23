#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static void	checkDirs(Fsck*);
static void	checkEpochs(Fsck*);
static void	checkLeak(Fsck*);
static void	closenop(Fsck*, Block*, u32int);
static void	clrenop(Fsck*, Block*, int);
static void	clrinop(Fsck*, char*, MetaBlock*, int, Block*);
static void	error(Fsck*, char*, ...);
static int	getBit(uchar*, u32int);
static int	printnop(char*, ...);
static void	setBit(uchar*, u32int);
static int	walkEpoch(Fsck *chk, Block *b, uchar score[VtScoreSize],
			int type, u32int tag, u32int epoch);
static void	warn(Fsck*, char*, ...);

#pragma varargck argpos error 2
#pragma varargck argpos printnop 1
#pragma varargck argpos warn 2

static Fsck*
checkInit(Fsck *chk)
{
	chk->cache = chk->fs->cache;
	chk->nblocks = cacheLocalSize(chk->cache, PartData);;
	chk->bsize = chk->fs->blockSize;
	chk->walkdepth = 0;
	chk->hint = 0;
	chk->quantum = chk->nblocks/100;
	if(chk->quantum == 0)
		chk->quantum = 1;
	if(chk->print == nil)
		chk->print = printnop;
	if(chk->clre == nil)
		chk->clre = clrenop;
	if(chk->close == nil)
		chk->close = closenop;
	if(chk->clri == nil)
		chk->clri = clrinop;
	return chk;
}

/*
 * BUG: Should merge checkEpochs and checkDirs so that
 * bad blocks are only reported once, and so that errors in checkEpochs
 * can have the affected file names attached, and so that the file system
 * is only read once.
 *
 * Also should summarize the errors instead of printing for every one
 * (e.g., XXX bad or unreachable blocks in /active/usr/rsc/foo).
 */

void
fsCheck(Fsck *chk)
{
	Block *b;
	Super super;

	checkInit(chk);
	b = superGet(chk->cache, &super);
	if(b == nil){
		chk->print("could not load super block: %R");
		return;
	}
	blockPut(b);

	chk->hint = super.active;
	checkEpochs(chk);

	chk->smap = vtMemAllocZ(chk->nblocks/8+1);
	checkDirs(chk);
	vtMemFree(chk->smap);
}

static void checkEpoch(Fsck*, u32int);

/*
 * Walk through all the blocks in the write buffer.
 * Then we can look for ones we missed -- those are leaks.
 */
static void
checkEpochs(Fsck *chk)
{
	u32int e;
	uint nb;

	nb = chk->nblocks;
	chk->amap = vtMemAllocZ(nb/8+1);
	chk->emap = vtMemAllocZ(nb/8+1);
	chk->xmap = vtMemAllocZ(nb/8+1);
	chk->errmap = vtMemAllocZ(nb/8+1);

	for(e = chk->fs->ehi; e >= chk->fs->elo; e--){
		memset(chk->emap, 0, chk->nblocks/8+1);
		memset(chk->xmap, 0, chk->nblocks/8+1);
		checkEpoch(chk, e);
	}
	checkLeak(chk);
	vtMemFree(chk->amap);
	vtMemFree(chk->emap);
	vtMemFree(chk->xmap);
	vtMemFree(chk->errmap);
}

static void
checkEpoch(Fsck *chk, u32int epoch)
{
	u32int a;
	Block *b;
	Entry e;
	Label l;

	chk->print("checking epoch %ud...\n", epoch);

	for(a=0; a<chk->nblocks; a++){
		if(!readLabel(chk->cache, &l, (a+chk->hint)%chk->nblocks)){
			error(chk, "could not read label for addr 0x%.8#ux", a);
			continue;
		}
		if(l.tag == RootTag && l.epoch == epoch)
			break;
	}

	if(a == chk->nblocks){
		chk->print("could not find root block for epoch %ud", epoch);
		return;
	}

	a = (a+chk->hint)%chk->nblocks;
	b = cacheLocalData(chk->cache, a, BtDir, RootTag, OReadOnly, 0);
	if(b == nil){
		error(chk, "could not read root block 0x%.8#ux: %R", a);
		return;
	}

	/* no one should point at root blocks */
	setBit(chk->amap, a);
	setBit(chk->emap, a);
	setBit(chk->xmap, a);

	/*
	 * First entry is the rest of the file system.
	 * Second entry is link to previous epoch root,
	 * just a convenience to help the search.
	 */
	if(!entryUnpack(&e, b->data, 0)){
		error(chk, "could not unpack root block 0x%.8#ux: %R", a);
		blockPut(b);
		return;
	}
	walkEpoch(chk, b, e.score, BtDir, e.tag, epoch);
	if(entryUnpack(&e, b->data, 1))
		chk->hint = globalToLocal(e.score);
	blockPut(b);
}

/*
 * When b points at bb, need to check:
 *
 * (i) b.e in [bb.e, bb.eClose)
 * (ii) if b.e==bb.e,  then no other b' in e points at bb.
 * (iii) if !(b.state&Copied) and b.e==bb.e then no other b' points at bb.
 * (iv) if b is active then no other active b' points at bb.
 * (v) if b is a past life of b' then only one of b and b' is active
 *	(too hard to check)
 */
static int
walkEpoch(Fsck *chk, Block *b, uchar score[VtScoreSize], int type, u32int tag,
	u32int epoch)
{
	int i, ret;
	u32int addr, ep;
	Block *bb;
	Entry e;

	if(b && chk->walkdepth == 0 && chk->printblocks)
		chk->print("%V %d %#.8ux %#.8ux\n", b->score, b->l.type,
			b->l.tag, b->l.epoch);

	if(!chk->useventi && globalToLocal(score) == NilBlock)
		return 1;

	chk->walkdepth++;

	bb = cacheGlobal(chk->cache, score, type, tag, OReadOnly);
	if(bb == nil){
		error(chk, "could not load block %V type %d tag %ux: %R",
			score, type, tag);
		chk->walkdepth--;
		return 0;
	}
	if(chk->printblocks)
		chk->print("%*s%V %d %#.8ux %#.8ux\n", chk->walkdepth*2, "",
			score, type, tag, bb->l.epoch);

	ret = 0;
	addr = globalToLocal(score);
	if(addr == NilBlock){
		ret = 1;
		goto Exit;
	}

	if(b){
		/* (i) */
		if(b->l.epoch < bb->l.epoch || bb->l.epochClose <= b->l.epoch){
			error(chk, "walk: block %#ux [%ud, %ud) points at %#ux [%ud, %ud)",
				b->addr, b->l.epoch, b->l.epochClose,
				bb->addr, bb->l.epoch, bb->l.epochClose);
			goto Exit;
		}

		/* (ii) */
		if(b->l.epoch == epoch && bb->l.epoch == epoch){
			if(getBit(chk->emap, addr)){
				error(chk, "walk: epoch join detected: addr %#ux %L",
					bb->addr, &bb->l);
				goto Exit;
			}
			setBit(chk->emap, addr);
		}

		/* (iii) */
		if(!(b->l.state&BsCopied) && b->l.epoch == bb->l.epoch){
			if(getBit(chk->xmap, addr)){
				error(chk, "walk: copy join detected; addr %#ux %L",
					bb->addr, &bb->l);
				goto Exit;
			}
			setBit(chk->xmap, addr);
		}
	}

	/* (iv) */
	if(epoch == chk->fs->ehi){
		/*
		 * since epoch==fs->ehi is first, amap is same as
		 * ``have seen active''
		 */
		if(getBit(chk->amap, addr)){
			error(chk, "walk: active join detected: addr %#ux %L",
				bb->addr, &bb->l);
			goto Exit;
		}
		if(bb->l.state&BsClosed)
			error(chk, "walk: addr %#ux: block is in active tree but is closed",
				addr);
	}else
		if(!getBit(chk->amap, addr))
			if(!(bb->l.state&BsClosed)){
				// error(chk, "walk: addr %#ux: block is not in active tree, not closed (%d)",
				// addr, bb->l.epochClose);
				chk->close(chk, bb, epoch+1);
				chk->nclose++;
			}

	if(getBit(chk->amap, addr)){
		ret = 1;
		goto Exit;
	}
	setBit(chk->amap, addr);

	if(chk->nseen++%chk->quantum == 0)
		chk->print("check: visited %d/%d blocks (%.0f%%)\n",
			chk->nseen, chk->nblocks, chk->nseen*100./chk->nblocks);

	b = nil;		/* make sure no more refs to parent */
	USED(b);

	switch(type){
	default:
		/* pointer block */
		for(i = 0; i < chk->bsize/VtScoreSize; i++)
			if(!walkEpoch(chk, bb, bb->data + i*VtScoreSize,
			    type-1, tag, epoch)){
				setBit(chk->errmap, bb->addr);
				chk->clrp(chk, bb, i);
				chk->nclrp++;
			}
		break;
	case BtData:
		break;
	case BtDir:
		for(i = 0; i < chk->bsize/VtEntrySize; i++){
			if(!entryUnpack(&e, bb->data, i)){
				// error(chk, "walk: could not unpack entry: %ux[%d]: %R",
				//	addr, i);
				setBit(chk->errmap, bb->addr);
				chk->clre(chk, bb, i);
				chk->nclre++;
				continue;
			}
			if(!(e.flags & VtEntryActive))
				continue;
if(0)			fprint(2, "%x[%d] tag=%x snap=%d score=%V\n",
				addr, i, e.tag, e.snap, e.score);
			ep = epoch;
			if(e.snap != 0){
				if(e.snap >= epoch){
					// error(chk, "bad snap in entry: %ux[%d] snap = %ud: epoch = %ud",
					//	addr, i, e.snap, epoch);
					setBit(chk->errmap, bb->addr);
					chk->clre(chk, bb, i);
					chk->nclre++;
					continue;
				}
				continue;
			}
			if(e.flags & VtEntryLocal){
				if(e.tag < UserTag)
				if(e.tag != RootTag || tag != RootTag || i != 1){
					// error(chk, "bad tag in entry: %ux[%d] tag = %ux",
					//	addr, i, e.tag);
					setBit(chk->errmap, bb->addr);
					chk->clre(chk, bb, i);
					chk->nclre++;
					continue;
				}
			}else
				if(e.tag != 0){
					// error(chk, "bad tag in entry: %ux[%d] tag = %ux",
					//	addr, i, e.tag);
					setBit(chk->errmap, bb->addr);
					chk->clre(chk, bb, i);
					chk->nclre++;
					continue;
				}
			if(!walkEpoch(chk, bb, e.score, entryType(&e),
			    e.tag, ep)){
				setBit(chk->errmap, bb->addr);
				chk->clre(chk, bb, i);
				chk->nclre++;
			}
		}
		break;
	}

	ret = 1;

Exit:
	chk->walkdepth--;
	blockPut(bb);
	return ret;
}

/*
 * We've just walked the whole write buffer.  Notice blocks that
 * aren't marked available but that we didn't visit.  They are lost.
 */
static void
checkLeak(Fsck *chk)
{
	u32int a, nfree, nlost;
	Block *b;
	Label l;

	nfree = 0;
	nlost = 0;

	for(a = 0; a < chk->nblocks; a++){
		if(!readLabel(chk->cache, &l, a)){
			error(chk, "could not read label: addr 0x%ux %d %d: %R",
				a, l.type, l.state);
			continue;
		}
		if(getBit(chk->amap, a))
			continue;
		if(l.state == BsFree || l.epochClose <= chk->fs->elo ||
		    l.epochClose == l.epoch){
			nfree++;
			setBit(chk->amap, a);
			continue;
		}
		if(l.state&BsClosed)
			continue;
		nlost++;
//		warn(chk, "unreachable block: addr 0x%ux type %d tag 0x%ux "
//			"state %s epoch %ud close %ud", a, l.type, l.tag,
//			bsStr(l.state), l.epoch, l.epochClose);
		b = cacheLocal(chk->cache, PartData, a, OReadOnly);
		if(b == nil){
			error(chk, "could not read block 0x%#.8ux", a);
			continue;
		}
		chk->close(chk, b, 0);
		chk->nclose++;
		setBit(chk->amap, a);
		blockPut(b);
	}
	chk->print("fsys blocks: total=%ud used=%ud(%.1f%%) free=%ud(%.1f%%) lost=%ud(%.1f%%)\n",
		chk->nblocks,
		chk->nblocks - nfree-nlost,
		100.*(chk->nblocks - nfree - nlost)/chk->nblocks,
		nfree, 100.*nfree/chk->nblocks,
		nlost, 100.*nlost/chk->nblocks);
}


/*
 * Check that all sources in the tree are accessible.
 */
static Source *
openSource(Fsck *chk, Source *s, char *name, uchar *bm, u32int offset,
	u32int gen, int dir, MetaBlock *mb, int i, Block *b)
{
	Source *r;

	r = nil;
	if(getBit(bm, offset)){
		warn(chk, "multiple references to source: %s -> %d",
			name, offset);
		goto Err;
	}
	setBit(bm, offset);

	r = sourceOpen(s, offset, OReadOnly, 0);
	if(r == nil){
		warn(chk, "could not open source: %s -> %d: %R", name, offset);
		goto Err;
	}

	if(r->gen != gen){
		warn(chk, "source has been removed: %s -> %d", name, offset);
		goto Err;
	}

	if(r->dir != dir){
		warn(chk, "dir mismatch: %s -> %d", name, offset);
		goto Err;
	}
	return r;
Err:
	chk->clri(chk, name, mb, i, b);
	chk->nclri++;
	if(r)
		sourceClose(r);
	return nil;
}

typedef struct MetaChunk MetaChunk;
struct MetaChunk {
	ushort	offset;
	ushort	size;
	ushort	index;
};

static int
offsetCmp(void *s0, void *s1)
{
	MetaChunk *mc0, *mc1;

	mc0 = s0;
	mc1 = s1;
	if(mc0->offset < mc1->offset)
		return -1;
	if(mc0->offset > mc1->offset)
		return 1;
	return 0;
}

/*
 * Fsck that MetaBlock has reasonable header, sorted entries,
 */
static int
chkMetaBlock(MetaBlock *mb)
{
	MetaChunk *mc;
	int oo, o, n, i;
	uchar *p;

	mc = vtMemAlloc(mb->nindex*sizeof(MetaChunk));
	p = mb->buf + MetaHeaderSize;
	for(i = 0; i < mb->nindex; i++){
		mc[i].offset = p[0]<<8 | p[1];
		mc[i].size =   p[2]<<8 | p[3];
		mc[i].index = i;
		p += MetaIndexSize;
	}

	qsort(mc, mb->nindex, sizeof(MetaChunk), offsetCmp);

	/* check block looks ok */
	oo = MetaHeaderSize + mb->maxindex*MetaIndexSize;
	o = oo;
	n = 0;
	for(i = 0; i < mb->nindex; i++){
		o = mc[i].offset;
		n = mc[i].size;
		if(o < oo)
			goto Err;
		oo += n;
	}
	if(o+n > mb->size || mb->size - oo != mb->free)
		goto Err;

	vtMemFree(mc);
	return 1;

Err:
if(0){
	fprint(2, "metaChunks failed!\n");
	oo = MetaHeaderSize + mb->maxindex*MetaIndexSize;
	for(i=0; i<mb->nindex; i++){
		fprint(2, "\t%d: %d %d\n", i, mc[i].offset,
			mc[i].offset + mc[i].size);
		oo += mc[i].size;
	}
	fprint(2, "\tused=%d size=%d free=%d free2=%d\n",
		oo, mb->size, mb->free, mb->size - oo);
}
	vtMemFree(mc);
	return 0;
}

static void
scanSource(Fsck *chk, char *name, Source *r)
{
	u32int a, nb, o;
	Block *b;
	Entry e;

	if(!chk->useventi && globalToLocal(r->score)==NilBlock)
		return;
	if(!sourceGetEntry(r, &e)){
		error(chk, "could not get entry for %s", name);
		return;
	}
	a = globalToLocal(e.score);
	if(!chk->useventi && a==NilBlock)
		return;
	if(getBit(chk->smap, a))
		return;
	setBit(chk->smap, a);

	nb = (sourceGetSize(r) + r->dsize-1) / r->dsize;
	for(o = 0; o < nb; o++){
		b = sourceBlock(r, o, OReadOnly);
		if(b == nil){
			error(chk, "could not read block in data file %s", name);
			continue;
		}
		if(b->addr != NilBlock && getBit(chk->errmap, b->addr)){
			warn(chk, "previously reported error in block %ux is in file %s",
				b->addr, name);
		}
		blockPut(b);
	}
}

/*
 * Walk the source tree making sure that the BtData
 * sources containing directory entries are okay.
 */
static void
chkDir(Fsck *chk, char *name, Source *source, Source *meta)
{
	int i;
	u32int a1, a2, nb, o;
	char *s, *nn;
	uchar *bm;
	Block *b, *bb;
	DirEntry de;
	Entry e1, e2;
	MetaBlock mb;
	MetaEntry me;
	Source *r, *mr;

	if(!chk->useventi && globalToLocal(source->score)==NilBlock &&
	    globalToLocal(meta->score)==NilBlock)
		return;

	if(!sourceLock2(source, meta, OReadOnly)){
		warn(chk, "could not lock sources for %s: %R", name);
		return;
	}
	if(!sourceGetEntry(source, &e1) || !sourceGetEntry(meta, &e2)){
		warn(chk, "could not load entries for %s: %R", name);
		return;
	}
	a1 = globalToLocal(e1.score);
	a2 = globalToLocal(e2.score);
	if((!chk->useventi && a1==NilBlock && a2==NilBlock)
	|| (getBit(chk->smap, a1) && getBit(chk->smap, a2))){
		sourceUnlock(source);
		sourceUnlock(meta);
		return;
	}
	setBit(chk->smap, a1);
	setBit(chk->smap, a2);

	bm = vtMemAllocZ(sourceGetDirSize(source)/8 + 1);

	nb = (sourceGetSize(meta) + meta->dsize - 1)/meta->dsize;
	for(o = 0; o < nb; o++){
		b = sourceBlock(meta, o, OReadOnly);
		if(b == nil){
			error(chk, "could not read block in meta file: %s[%ud]: %R",
				name, o);
			continue;
		}
if(0)		fprint(2, "source %V:%d block %d addr %d\n", source->score,
			source->offset, o, b->addr);
		if(b->addr != NilBlock && getBit(chk->errmap, b->addr))
			warn(chk, "previously reported error in block %ux is in %s",
				b->addr, name);

		if(!mbUnpack(&mb, b->data, meta->dsize)){
			error(chk, "could not unpack meta block: %s[%ud]: %R",
				name, o);
			blockPut(b);
			continue;
		}
		if(!chkMetaBlock(&mb)){
			error(chk, "bad meta block: %s[%ud]: %R", name, o);
			blockPut(b);
			continue;
		}
		s = nil;
		for(i=mb.nindex-1; i>=0; i--){
			meUnpack(&me, &mb, i);
			if(!deUnpack(&de, &me)){
				error(chk,
				  "could not unpack dir entry: %s[%ud][%d]: %R",
					name, o, i);
				continue;
			}
			if(s && strcmp(s, de.elem) <= 0)
				error(chk,
			   "dir entry out of order: %s[%ud][%d] = %s last = %s",
					name, o, i, de.elem, s);
			vtMemFree(s);
			s = vtStrDup(de.elem);
			nn = smprint("%s/%s", name, de.elem);
			if(nn == nil){
				error(chk, "out of memory");
				continue;
			}
			if(chk->printdirs)
				if(de.mode&ModeDir)
					chk->print("%s/\n", nn);
			if(chk->printfiles)
				if(!(de.mode&ModeDir))
					chk->print("%s\n", nn);
			if(!(de.mode & ModeDir)){
				r = openSource(chk, source, nn, bm, de.entry,
					de.gen, 0, &mb, i, b);
				if(r != nil){
					if(sourceLock(r, OReadOnly)){
						scanSource(chk, nn, r);
						sourceUnlock(r);
					}
					sourceClose(r);
				}
				deCleanup(&de);
				free(nn);
				continue;
			}

			r = openSource(chk, source, nn, bm, de.entry,
				de.gen, 1, &mb, i, b);
			if(r == nil){
				deCleanup(&de);
				free(nn);
				continue;
			}

			mr = openSource(chk, source, nn, bm, de.mentry,
				de.mgen, 0, &mb, i, b);
			if(mr == nil){
				sourceClose(r);
				deCleanup(&de);
				free(nn);
				continue;
			}

			if(!(de.mode&ModeSnapshot) || chk->walksnapshots)
				chkDir(chk, nn, r, mr);

			sourceClose(mr);
			sourceClose(r);
			deCleanup(&de);
			free(nn);
			deCleanup(&de);

		}
		vtMemFree(s);
		blockPut(b);
	}

	nb = sourceGetDirSize(source);
	for(o=0; o<nb; o++){
		if(getBit(bm, o))
			continue;
		r = sourceOpen(source, o, OReadOnly, 0);
		if(r == nil)
			continue;
		warn(chk, "non referenced entry in source %s[%d]", name, o);
		if((bb = sourceBlock(source, o/(source->dsize/VtEntrySize),
		    OReadOnly)) != nil){
			if(bb->addr != NilBlock){
				setBit(chk->errmap, bb->addr);
				chk->clre(chk, bb, o%(source->dsize/VtEntrySize));
				chk->nclre++;
			}
			blockPut(bb);
		}
		sourceClose(r);
	}

	sourceUnlock(source);
	sourceUnlock(meta);
	vtMemFree(bm);
}

static void
checkDirs(Fsck *chk)
{
	Source *r, *mr;

	sourceLock(chk->fs->source, OReadOnly);
	r = sourceOpen(chk->fs->source, 0, OReadOnly, 0);
	mr = sourceOpen(chk->fs->source, 1, OReadOnly, 0);
	sourceUnlock(chk->fs->source);
	chkDir(chk, "", r, mr);

	sourceClose(r);
	sourceClose(mr);
}

static void
setBit(uchar *bmap, u32int addr)
{
	if(addr == NilBlock)
		return;

	bmap[addr>>3] |= 1 << (addr & 7);
}

static int
getBit(uchar *bmap, u32int addr)
{
	if(addr == NilBlock)
		return 0;

	return (bmap[addr>>3] >> (addr & 7)) & 1;
}

static void
error(Fsck *chk, char *fmt, ...)
{
	char buf[256];
	va_list arg;
	static int nerr;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);

	chk->print("error: %s\n", buf);

//	if(nerr++ > 20)
//		vtFatal("too many errors");
}

static void
warn(Fsck *chk, char *fmt, ...)
{
	char buf[256];
	va_list arg;
	static int nerr;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);

	chk->print("error: %s\n", buf);
}

static void
clrenop(Fsck*, Block*, int)
{
}

static void
closenop(Fsck*, Block*, u32int)
{
}

static void
clrinop(Fsck*, char*, MetaBlock*, int, Block*)
{
}

static int
printnop(char*, ...)
{
	return 0;
}
