#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "whack.h"

static int disksummary(HConnect*);
static int diskarenapart(HConnect*, char*, Part*);
static int diskbloom(HConnect*, char*, Part*);
static int diskisect(HConnect*, char*, Part*);

int
hdisk(HConnect *c)
{
	char *disk, *type;
	Part *p;
	int ret;

	if(hsethtml(c) < 0)
		return -1;

	disk = hargstr(c, "disk", "");
	if(!disk[0])
		return disksummary(c);
	if((p = initpart(disk, OREAD)) == nil){
		hprint(&c->hout, "open %s: %r", disk);
		return 0;
	}

	type = hargstr(c, "type", "");
	switch(type[0]){
	case 'a':
		ret = diskarenapart(c, disk, p);
		break;
	case 'b':
		ret = diskbloom(c, disk, p);
		break;
	case 'i':
		ret = diskisect(c, disk, p);
		break;
	default:
		hprint(&c->hout, "unknown disk type %s", type);
		return 0;
	}
	freepart(p);
	return ret;
}

static int
disksummary(HConnect *c)
{
	int i;
	Index *ix;
	Part *p;

	hprint(&c->hout, "<h1>venti disks</h1>\n");
	hprint(&c->hout, "<pre>\n");
	ix = mainindex;
	p = nil;
	for(i=0; i<ix->narenas; i++){
		if(ix->arenas[i]->part == p)
			continue;
		p = ix->arenas[i]->part;
		hprint(&c->hout, "<a href=\"/disk?disk=%s&type=a\">%s</a> %s\n", p->name, p->name, ix->arenas[i]->name);
	}
	hprint(&c->hout, "\n");
	p = nil;
	for(i=0; i<ix->nsects; i++){
		if(ix->sects[i]->part == p)
			continue;
		p = ix->sects[i]->part;
		hprint(&c->hout, "<a href=\"/disk?disk=%s&type=i\">%s</a> %s\n", p->name, p->name, ix->sects[i]->name);
	}
	hprint(&c->hout, "\n");
	if(ix->bloom){
		p = ix->bloom->part;
		hprint(&c->hout, "<a href=\"/disk?disk=%s&type=b\">%s</a> %s\n", p->name, p->name, "bloom filter");
	}
	return 0;
}

static char*
readap(Part *p, ArenaPart *ap)
{
	uchar *blk;
	char *table;

	blk = vtmalloc(8192);
	if(readpart(p, PartBlank, blk, 8192) != 8192)
		return nil;
	if(unpackarenapart(ap, blk) < 0){
		werrstr("corrupt arena part header: %r");
		return nil;
	}
	vtfree(blk);
	ap->tabbase = (PartBlank+HeadSize+ap->blocksize-1)&~(ap->blocksize-1);
	ap->tabsize = ap->arenabase - ap->tabbase;
	table = vtmalloc(ap->tabsize+1);
	if(readpart(p, ap->tabbase, (uchar*)table, ap->tabsize) != ap->tabsize){
		werrstr("reading arena part directory: %r");
		return nil;
	}
	table[ap->tabsize] = 0;
	return table;
}

static int
xfindarena(char *table, char *name, vlong *start, vlong *end)
{
	int i, nline;
	char *p, *q, *f[4], line[256];

	nline = atoi(table);
	p = strchr(table, '\n');
	if(p)
		p++;
	for(i=0; i<nline; i++){
		if(p == nil)
			break;
		q = strchr(p, '\n');
		if(q)
			*q++ = 0;
		if(strlen(p) >= sizeof line){
			p = q;
			continue;
		}
		strcpy(line, p);
		memset(f, 0, sizeof f);
		if(tokenize(line, f, nelem(f)) < 3){
			p = q;
			continue;
		}
		if(strcmp(f[0], name) == 0){
			*start = strtoull(f[1], 0, 0);
			*end = strtoull(f[2], 0, 0);
			return 0;
		}
		p = q;
	}
	return -1;
}

static void
diskarenatable(HConnect *c, char *disk, char *table)
{
	char *p, *q;
	int i, nline;
	char *f[4], line[256], base[256];

	hprint(&c->hout, "<h2>table</h2>\n");
	hprint(&c->hout, "<pre>\n");
	nline = atoi(table);
	snprint(base, sizeof base, "/disk?disk=%s&type=a", disk);
	p = strchr(table, '\n');
	if(p)
		p++;
	for(i=0; i<nline; i++){
		if(p == nil){
			hprint(&c->hout, "<b><i>unexpected end of table</i></b>\n");
			break;
		}
		q = strchr(p, '\n');
		if(q)
			*q++ = 0;
		if(strlen(p) >= sizeof line){
			hprint(&c->hout, "%s\n", p);
			p = q;
			continue;
		}
		strcpy(line, p);
		memset(f, 0, sizeof f);
		if(tokenize(line, f, 3) < 3){
			hprint(&c->hout, "%s\n", p);
			p = q;
			continue;
		}
		p = q;
		hprint(&c->hout, "<a href=\"%s&arena=%s\">%s</a> %s %s\n",
			base, f[0], f[0], f[1], f[2]);
	}
	hprint(&c->hout, "</pre>\n");
}

static char*
fmttime(char *buf, ulong time)
{
	strcpy(buf, ctime(time));
	buf[28] = 0;
	return buf;
}


static int diskarenaclump(HConnect*, Arena*, vlong, char*);
static int diskarenatoc(HConnect*, Arena*);

static int
diskarenapart(HConnect *c, char *disk, Part *p)
{
	char *arenaname;
	ArenaPart ap;
	ArenaHead head;
	Arena arena;
	char *table;
	char *score;
	char *clump;
	uchar *blk;
	vlong start, end, off;
	char tbuf[60];

	hprint(&c->hout, "<h1>arena partition %s</h1>\n", disk);

	if((table = readap(p, &ap)) == nil){
		hprint(&c->hout, "%r\n");
		goto out;
	}

	hprint(&c->hout, "<pre>\n");
	hprint(&c->hout, "version=%d blocksize=%d base=%d\n",
		ap.version, ap.blocksize, ap.arenabase);
	hprint(&c->hout, "</pre>\n");

	arenaname = hargstr(c, "arena", "");
	if(arenaname[0] == 0){
		diskarenatable(c, disk, table);
		goto out;
	}

	if(xfindarena(table, arenaname, &start, &end) < 0){
		hprint(&c->hout, "no such arena %s\n", arenaname);
		goto out;
	}

	hprint(&c->hout, "<h2>arena %s</h2>\n", arenaname);
	hprint(&c->hout, "<pre>start=%#llx end=%#llx<pre>\n", start, end);
	if(end < start || end - start < HeadSize){
		hprint(&c->hout, "bad size %#llx\n", end - start);
		goto out;
	}

	// read arena header, tail
	blk = vtmalloc(HeadSize);
	if(readpart(p, start, blk, HeadSize) != HeadSize){
		hprint(&c->hout, "reading header: %r\n");
		vtfree(blk);
		goto out;
	}
	if(unpackarenahead(&head, blk) < 0){
		hprint(&c->hout, "corrupt arena header: %r\n");
		// hhex(blk, HeadSize);
		vtfree(blk);
		goto out;
	}
	vtfree(blk);

	hprint(&c->hout, "head:\n<pre>\n");
	hprint(&c->hout, "version=%d name=%s blocksize=%d size=%#llx clumpmagic=%#ux\n",
		head.version, head.name, head.blocksize, head.size,
		head.clumpmagic);
	hprint(&c->hout, "</pre><br><br>\n");

	if(head.blocksize > MaxIoSize || head.blocksize >= end - start){
		hprint(&c->hout, "corrupt block size %d\n", head.blocksize);
		goto out;
	}

	blk = vtmalloc(head.blocksize);
	if(readpart(p, end - head.blocksize, blk, head.blocksize) < 0){
		hprint(&c->hout, "reading tail: %r\n");
		vtfree(blk);
		goto out;
	}
	memset(&arena, 0, sizeof arena);
	arena.part = p;
	arena.blocksize = head.blocksize;
	arena.clumpmax = head.blocksize / ClumpInfoSize;
	arena.base = start + head.blocksize;
	arena.size = end - start - 2 * head.blocksize;
	if(unpackarena(&arena, blk) < 0){
		vtfree(blk);
		goto out;
	}
	scorecp(arena.score, blk+head.blocksize - VtScoreSize);

	vtfree(blk);

	hprint(&c->hout, "tail:\n<pre>\n");
	hprint(&c->hout, "version=%d name=%s\n", arena.version, arena.name);
	hprint(&c->hout, "ctime=%d %s\n", arena.ctime, fmttime(tbuf, arena.ctime));
	hprint(&c->hout, "wtime=%d %s\n", arena.wtime, fmttime(tbuf, arena.wtime));
	hprint(&c->hout, "clumpmagic=%#ux\n", arena.clumpmagic);
	hprint(&c->hout, "score %V\n", arena.score);
	hprint(&c->hout, "diskstats:\n");
	hprint(&c->hout, "\tclumps=%,d cclumps=%,d used=%,lld uncsize=%,lld sealed=%d\n",
		arena.diskstats.clumps, arena.diskstats.cclumps,
		arena.diskstats.used, arena.diskstats.uncsize,
		arena.diskstats.sealed);
	hprint(&c->hout, "memstats:\n");
	hprint(&c->hout, "\tclumps=%,d cclumps=%,d used=%,lld uncsize=%,lld sealed=%d\n",
		arena.memstats.clumps, arena.memstats.cclumps,
		arena.memstats.used, arena.memstats.uncsize,
		arena.memstats.sealed);
	if(arena.clumpmax == 0){
		hprint(&c->hout, "bad clumpmax\n");
		goto out;
	}

	score = hargstr(c, "score", "");
	clump = hargstr(c, "clump", "");

	if(clump[0]){
		off = strtoull(clump, 0, 0);
		diskarenaclump(c, &arena, off, score[0] ? score : nil);
	}else if(score[0]){
		diskarenaclump(c, &arena, -1, score);
	}else{
		diskarenatoc(c, &arena);
	}

out:
	free(table);
	return 0;
}

static vlong
findintoc(HConnect *c, Arena *arena, uchar *score)
{
	uchar *blk;
	int i;
	vlong off;
	vlong coff;
	ClumpInfo ci;

	blk = vtmalloc(arena->blocksize);
	off = arena->base + arena->size;
	coff = 0;
	for(i=0; i<arena->memstats.clumps; i++){
		if(i%arena->clumpmax == 0){
			off -= arena->blocksize;
			if(readpart(arena->part, off, blk, arena->blocksize) != arena->blocksize){
				if(c)
					hprint(&c->hout, "<i>clump info directory at %#llx: %r</i>\n<br>\n",
						off);
				break;
			}
		}
		unpackclumpinfo(&ci, blk+(i%arena->clumpmax)*ClumpInfoSize);
		if(scorecmp(ci.score, score) == 0){
			vtfree(blk);
			return coff;
		}
		coff += ClumpSize + ci.size;
	}
	vtfree(blk);
	return -1;
}


static int
diskarenatoc(HConnect *c, Arena *arena)
{
	uchar *blk;
	int i;
	vlong off;
	vlong coff;
	ClumpInfo ci;
	char base[512];
	int cib;

	snprint(base, sizeof base, "/disk?disk=%s&type=a&arena=%s",
		arena->part->name, arena->name);

	blk = vtmalloc(arena->blocksize);
	off = arena->base + arena->size;
	hprint(&c->hout, "<h2>table of contents</h2>\n");
	hprint(&c->hout, "<pre>\n");
	hprint(&c->hout, "%5s %6s %7s %s\n", "type", "size", "uncsize", "score");
	coff = 0;
	cib = hargint(c, "cib", 0);

	for(i=0; i<arena->memstats.clumps; i++){
		if(i%arena->clumpmax == 0){
			off -= arena->blocksize;
			if(readpart(arena->part, off, blk, arena->blocksize) != arena->blocksize){
				hprint(&c->hout, "<i>clump info directory at %#llx: %r</i>\n<br>\n",
					off);
				i += arena->clumpmax-1;
				coff = -1;
				continue;
			}
		}
		unpackclumpinfo(&ci, blk+(i%arena->clumpmax)*ClumpInfoSize);
		if(i/arena->clumpmax == cib || i%arena->clumpmax == 0){
			hprint(&c->hout, "%5d %6d %7d %V",
				ci.type, ci.size, ci.uncsize, ci.score);
			if(coff >= 0)
				hprint(&c->hout, " at <a href=\"%s&clump=%#llx&score=%V\">%#llx</a>",
					base, coff, ci.score, coff);
			if(i/arena->clumpmax != cib)
				hprint(&c->hout, "  <font size=-1><a href=\"%s&cib=%d\">more</a></font>", base, i/arena->clumpmax);
			hprint(&c->hout, "\n");
		}
		if(coff >= 0)
			coff += ClumpSize + ci.size;
	}
	hprint(&c->hout, "</pre>\n");
	return 0;
}

#define	U32GET(p)	((u32int)(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3]))
static int
diskarenaclump(HConnect *c, Arena *arena, vlong off, char *scorestr)
{
	uchar *blk, *blk2;
	Clump cl;
	char err[ERRMAX];
	uchar xscore[VtScoreSize], score[VtScoreSize];
	Unwhack uw;
	int n;

	if(scorestr){
		if(vtparsescore(scorestr, nil, score) < 0){
			hprint(&c->hout, "bad score %s: %r\n", scorestr);
			return -1;
		}
		if(off < 0){
			off = findintoc(c, arena, score);
			if(off < 0){
				hprint(&c->hout, "score %V not found in arena %s\n", score, arena->name);
				return -1;
			}
			hprint(&c->hout, "score %V at %#llx\n", score, off);
		}
	}else
		memset(score, 0, sizeof score);

	if(off < 0){
		hprint(&c->hout, "bad offset %#llx\n", off);
		return -1;
	}

	off += arena->base;

	blk = vtmalloc(ClumpSize + VtMaxLumpSize);
	if(readpart(arena->part, off, blk, ClumpSize + VtMaxLumpSize) != ClumpSize + VtMaxLumpSize){
		hprint(&c->hout, "reading at %#llx: %r\n", off);
		vtfree(blk);
		return -1;
	}

	if(unpackclump(&cl, blk, arena->clumpmagic) < 0){
		hprint(&c->hout, "unpackclump: %r\n<br>");
		rerrstr(err, sizeof err);
		if(strstr(err, "magic")){
			hprint(&c->hout, "trying again with magic=%#ux<br>\n", U32GET(blk));
			if(unpackclump(&cl, blk, U32GET(blk)) < 0){
				hprint(&c->hout, "unpackclump: %r\n<br>\n");
				goto error;
			}
		}else
			goto error;
	}

	hprint(&c->hout, "<pre>type=%d size=%d uncsize=%d score=%V\n", cl.info.type, cl.info.size, cl.info.uncsize, cl.info.score);
	hprint(&c->hout, "encoding=%d creator=%d time=%d %s</pre>\n", cl.encoding, cl.creator, cl.time, fmttime(err, cl.time));

	if(cl.info.type == VtCorruptType)
		hprint(&c->hout, "clump is marked corrupt<br>\n");

	if(cl.info.size >= VtMaxLumpSize){
		hprint(&c->hout, "clump too big\n");
		goto error;
	}

	switch(cl.encoding){
	case ClumpECompress:
		blk2 = vtmalloc(VtMaxLumpSize);
		unwhackinit(&uw);
		n = unwhack(&uw, blk2, cl.info.uncsize, blk+ClumpSize, cl.info.size);
		if(n < 0){
			hprint(&c->hout, "decompression failed\n");
			vtfree(blk2);
			goto error;
		}
		if(n != cl.info.uncsize){
			hprint(&c->hout, "got wrong amount: %d wanted %d\n", n, cl.info.uncsize);
			// hhex(blk2, n);
			vtfree(blk2);
			goto error;
		}
		scoremem(xscore, blk2, cl.info.uncsize);
		vtfree(blk2);
		break;
	case ClumpENone:
		scoremem(xscore, blk+ClumpSize, cl.info.size);
		break;
	}

	hprint(&c->hout, "score=%V<br>\n", xscore);
	if(scorestr && scorecmp(score, xscore) != 0)
		hprint(&c->hout, "score does NOT match expected %V\n", score);

	vtfree(blk);
	return 0;

error:
	// hhex(blk, ClumpSize + VtMaxLumpSize);
	vtfree(blk);
	return -1;
}

static int
diskbloom(HConnect *c, char *disk, Part *p)
{
	USED(c);
	USED(disk);
	USED(p);
	return 0;
}

static int
diskisect(HConnect *c, char *disk, Part *p)
{
	USED(c);
	USED(disk);
	USED(p);
	return 0;
}

static void
debugamap(HConnect *c)
{
	int i;
	AMap *amap;

	hprint(&c->hout, "<h2>arena map</h2>\n");
	hprint(&c->hout, "<pre>\n");

	amap = mainindex->amap;
	for(i=0; i<mainindex->narenas; i++)
		hprint(&c->hout, "%s %#llx %#llx\n",
			amap[i].name, amap[i].start, amap[i].stop);
}

static void
debugread(HConnect *c, u8int *score)
{
	int type;
	Lump *u;
	IAddr ia;
	IEntry ie;
	int i;
	Arena *arena;
	u64int aa;
	ZBlock *zb;
	Clump cl;
	vlong off;
	u8int sc[VtScoreSize];

	if(scorecmp(score, zeroscore) == 0){
		hprint(&c->hout, "zero score\n");
		return;
	}

	hprint(&c->hout, "<h2>index search %V</h2><pre>\n", score);
	if(icachelookup(score, -1, &ia) < 0)
		hprint(&c->hout, "  icache: not found\n");
	else
		hprint(&c->hout, "  icache: addr=%#llx size=%d type=%d blocks=%d\n",
			ia.addr, ia.size, ia.type, ia.blocks);

	if(loadientry(mainindex, score, -1, &ie) < 0)
		hprint(&c->hout, "  idisk: not found\n");
	else
		hprint(&c->hout, "  idisk: addr=%#llx size=%d type=%d blocks=%d\n",
			ie.ia.addr, ie.ia.size, ie.ia.type, ie.ia.blocks);

	hprint(&c->hout, "</pre><h2>lookup %V</h2>\n", score);
	hprint(&c->hout, "<pre>\n");

	for(type=0; type < VtMaxType; type++){
		hprint(&c->hout, "%V type %d:", score, type);
		u = lookuplump(score, type);
		if(u->data != nil)
			hprint(&c->hout, " +cache");
		else
			hprint(&c->hout, " -cache");
		putlump(u);

		if(lookupscore(score, type, &ia) < 0){
			hprint(&c->hout, " -lookup\n");
			continue;
		}
		hprint(&c->hout, "\n  lookupscore: addr=%#llx size=%d blocks=%d\n",
			ia.addr, ia.size, ia.blocks);

		arena = amapitoa(mainindex, ia.addr, &aa);
		if(arena == nil){
			hprint(&c->hout, "  amapitoa failed: %r\n");
			continue;
		}

		hprint(&c->hout, "  amapitoa: aa=%#llx arena="
			"<a href=\"/disk?disk=%s&type=a&arena=%s&score=%V\">%s</a>\n",
			aa, arena->part->name, arena->name, score, arena->name);
		zb = loadclump(arena, aa, ia.blocks, &cl, sc, 1);
		if(zb == nil){
			hprint(&c->hout, "  loadclump failed: %r\n");
			continue;
		}

		hprint(&c->hout, "  loadclump: uncsize=%d type=%d score=%V\n",
			cl.info.uncsize, cl.info.type, sc);
		if(ia.size != cl.info.uncsize || ia.type != cl.info.type || scorecmp(score, sc) != 0){
			hprint(&c->hout, "    clump info mismatch\n");
			continue;
		}
	}

	if(hargstr(c, "brute", "")[0] == 'y'){
		hprint(&c->hout, "</pre>\n");
		hprint(&c->hout, "<h2>brute force arena search %V</h2>\n", score);
		hprint(&c->hout, "<pre>\n");

		for(i=0; i<mainindex->narenas; i++){
			arena = mainindex->arenas[i];
			hprint(&c->hout, "%s...\n", arena->name);
			hflush(&c->hout);
			off = findintoc(nil, arena, score);
			if(off >= 0)
				hprint(&c->hout, "%s %#llx (%#llx)\n", arena->name, off, mainindex->amap[i].start + off);
		}
	}

	hprint(&c->hout, "</pre>\n");
}

static void
debugmem(HConnect *c)
{
	Index *ix;

	ix = mainindex;
	hprint(&c->hout, "<h2>memory</h2>\n");

	hprint(&c->hout, "<pre>\n");
	hprint(&c->hout, "ix=%p\n", ix);
	hprint(&c->hout, "\tarenas=%p\n", ix->arenas);
	if(ix->narenas > 0)
		hprint(&c->hout, "\tarenas[...] = %p...%p\n", ix->arenas[0], ix->arenas[ix->narenas-1]);
	hprint(&c->hout, "\tsmap=%p\n", ix->smap);
	hprint(&c->hout, "\tamap=%p\n", ix->amap);
	hprint(&c->hout, "\tbloom=%p\n", ix->bloom);
	hprint(&c->hout, "\tbloom->data=%p\n", ix->bloom ? ix->bloom->data : nil);
	hprint(&c->hout, "\tisects=%p\n", ix->sects);
	if(ix->nsects > 0)
		hprint(&c->hout, "\tsects[...] = %p...%p\n", ix->sects[0], ix->sects[ix->nsects-1]);
}

int
hdebug(HConnect *c)
{
	char *scorestr, *op;
	u8int score[VtScoreSize];

	if(hsethtml(c) < 0)
		return -1;
	hprint(&c->hout, "<h1>venti debug</h1>\n");

	op = hargstr(c, "op", "");
	if(!op[0]){
		hprint(&c->hout, "no op\n");
		return 0;
	}

	if(strcmp(op, "amap") == 0){
		debugamap(c);
		return 0;
	}

	if(strcmp(op, "mem") == 0){
		debugmem(c);
		return 0;
	}

	if(strcmp(op, "read") == 0){
		scorestr = hargstr(c, "score", "");
		if(vtparsescore(scorestr, nil, score) < 0){
			hprint(&c->hout, "bad score %s: %r\n", scorestr);
			return 0;
		}
		debugread(c, score);
		return 0;
	}

	hprint(&c->hout, "unknown op %s", op);
	return 0;
}
