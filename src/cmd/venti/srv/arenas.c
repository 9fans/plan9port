#include "stdinc.h"
#include "dat.h"
#include "fns.h"

typedef struct AHash	AHash;

/*
 * hash table for finding arena's based on their names.
 */
struct AHash
{
	AHash	*next;
	Arena	*arena;
};

enum
{
	AHashSize	= 512
};

static AHash	*ahash[AHashSize];

static u32int
hashstr(char *s)
{
	u32int h;
	int c;

	h = 0;
	for(; c = *s; s++){
		c ^= c << 6;
		h += (c << 11) ^ (c >> 1);
		c = *s;
		h ^= (c << 14) + (c << 7) + (c << 4) + c;
	}
	return h;
}

int
addarena(Arena *arena)
{
	AHash *a;
	u32int h;

	h = hashstr(arena->name) & (AHashSize - 1);
	a = MK(AHash);
	if(a == nil)
		return -1;
	a->arena = arena;
	a->next = ahash[h];
	ahash[h] = a;
	return 0;
}

Arena*
findarena(char *name)
{
	AHash *a;
	u32int h;

	h = hashstr(name) & (AHashSize - 1);
	for(a = ahash[h]; a != nil; a = a->next)
		if(strcmp(a->arena->name, name) == 0)
			return a->arena;
	return nil;
}

int
delarena(Arena *arena)
{
	AHash *a, *last;
	u32int h;

	h = hashstr(arena->name) & (AHashSize - 1);
	last = nil;
	for(a = ahash[h]; a != nil; a = a->next){
		if(a->arena == arena){
			if(last != nil)
				last->next = a->next;
			else
				ahash[h] = a->next;
			free(a);
			return 0;
		}
		last = a;
	}
	return -1;
}

ArenaPart*
initarenapart(Part *part)
{
	AMapN amn;
	ArenaPart *ap;
	ZBlock *b;
	u32int i;
	int ok;

	b = alloczblock(HeadSize, 0, 0);
	if(b == nil || readpart(part, PartBlank, b->data, HeadSize) < 0){
		seterr(EAdmin, "can't read arena partition header: %r");
		return nil;
	}

	ap = MKZ(ArenaPart);
	if(ap == nil){
		freezblock(b);
		return nil;
	}
	ap->part = part;
	ok = unpackarenapart(ap, b->data);
	freezblock(b);
	if(ok < 0){
		freearenapart(ap, 0);
		return nil;
	}

	ap->tabbase = (PartBlank + HeadSize + ap->blocksize - 1) & ~(ap->blocksize - 1);
	if(ap->version != ArenaPartVersion){
		seterr(ECorrupt, "unknown arena partition version %d", ap->version);
		freearenapart(ap, 0);
		return nil;
	}
	if(ap->blocksize & (ap->blocksize - 1)){
		seterr(ECorrupt, "illegal non-power-of-2 block size %d\n", ap->blocksize);
		freearenapart(ap, 0);
		return nil;
	}
	if(ap->tabbase >= ap->arenabase){
		seterr(ECorrupt, "arena partition table overlaps with arena storage");
		freearenapart(ap, 0);
		return nil;
	}
	ap->tabsize = ap->arenabase - ap->tabbase;
	partblocksize(part, ap->blocksize);
	ap->size = ap->part->size & ~(u64int)(ap->blocksize - 1);

	if(readarenamap(&amn, part, ap->tabbase, ap->tabsize) < 0){
		freearenapart(ap, 0);
		return nil;
	}
	ap->narenas = amn.n;
	ap->map = amn.map;
	if(okamap(ap->map, ap->narenas, ap->arenabase, ap->size, "arena table") < 0){
		freearenapart(ap, 0);
		return nil;
	}

	ap->arenas = MKNZ(Arena*, ap->narenas);
	for(i = 0; i < ap->narenas; i++){
		debugarena = i;
		ap->arenas[i] = initarena(part, ap->map[i].start, ap->map[i].stop - ap->map[i].start, ap->blocksize);
		if(ap->arenas[i] == nil){
			seterr(ECorrupt, "%s: %r", ap->map[i].name);
			freearenapart(ap, 1);
			return nil;
		}
		if(namecmp(ap->map[i].name, ap->arenas[i]->name) != 0){
			seterr(ECorrupt, "arena name mismatches with expected name: %s vs. %s",
				ap->map[i].name, ap->arenas[i]->name);
			freearenapart(ap, 1);
			return nil;
		}
		if(findarena(ap->arenas[i]->name)){
			seterr(ECorrupt, "duplicate arena name %s in %s",
				ap->map[i].name, ap->part->name);
			freearenapart(ap, 1);
			return nil;
		}
	}

	for(i = 0; i < ap->narenas; i++) {
		debugarena = i;
		addarena(ap->arenas[i]);
	}
	debugarena = -1;

	return ap;
}

ArenaPart*
newarenapart(Part *part, u32int blocksize, u32int tabsize)
{
	ArenaPart *ap;

	if(blocksize & (blocksize - 1)){
		seterr(ECorrupt, "illegal non-power-of-2 block size %d\n", blocksize);
		return nil;
	}
	ap = MKZ(ArenaPart);
	if(ap == nil)
		return nil;

	ap->version = ArenaPartVersion;
	ap->part = part;
	ap->blocksize = blocksize;
	partblocksize(part, blocksize);
	ap->size = part->size & ~(u64int)(blocksize - 1);
	ap->tabbase = (PartBlank + HeadSize + blocksize - 1) & ~(blocksize - 1);
	ap->arenabase = (ap->tabbase + tabsize + blocksize - 1) & ~(blocksize - 1);
	ap->tabsize = ap->arenabase - ap->tabbase;
	ap->narenas = 0;

	if(wbarenapart(ap) < 0){
		freearenapart(ap, 0);
		return nil;
	}

	return ap;
}

int
wbarenapart(ArenaPart *ap)
{
	ZBlock *b;

	if(okamap(ap->map, ap->narenas, ap->arenabase, ap->size, "arena table") < 0)
		return -1;
	b = alloczblock(HeadSize, 1, 0);
	if(b == nil)
/* ZZZ set error message? */
		return -1;

	if(packarenapart(ap, b->data) < 0){
		seterr(ECorrupt, "can't make arena partition header: %r");
		freezblock(b);
		return -1;
	}
	if(writepart(ap->part, PartBlank, b->data, HeadSize) < 0 ||
	   flushpart(ap->part) < 0){
		seterr(EAdmin, "can't write arena partition header: %r");
		freezblock(b);
		return -1;
	}
	freezblock(b);

	return wbarenamap(ap->map, ap->narenas, ap->part, ap->tabbase, ap->tabsize);
}

void
freearenapart(ArenaPart *ap, int freearenas)
{
	int i;

	if(ap == nil)
		return;
	if(freearenas){
		for(i = 0; i < ap->narenas; i++){
			if(ap->arenas[i] == nil)
				continue;
			delarena(ap->arenas[i]);
			freearena(ap->arenas[i]);
		}
	}
	free(ap->map);
	free(ap->arenas);
	free(ap);
}

int
okamap(AMap *am, int n, u64int start, u64int stop, char *what)
{
	u64int last;
	u32int i;

	last = start;
	for(i = 0; i < n; i++){
		if(am[i].start < last){
			if(i == 0)
				seterr(ECorrupt, "invalid start address in %s", what);
			else
				seterr(ECorrupt, "overlapping ranges in %s", what);
			return -1;
		}
		if(am[i].stop < am[i].start){
			seterr(ECorrupt, "invalid range in %s", what);
			return -1;
		}
		last = am[i].stop;
	}
	if(last > stop){
		seterr(ECorrupt, "invalid ending address in %s", what);
		return -1;
	}
	return 0;
}

int
maparenas(AMap *am, Arena **arenas, int n, char *what)
{
	u32int i;

	for(i = 0; i < n; i++){
		arenas[i] = findarena(am[i].name);
		if(arenas[i] == nil){
			seterr(EAdmin, "can't find arena '%s' for '%s'\n", am[i].name, what);
			return -1;
		}
	}
	return 0;
}

int
readarenamap(AMapN *amn, Part *part, u64int base, u32int size)
{
	IFile f;
	u32int ok;

	if(partifile(&f, part, base, size) < 0)
		return -1;
	ok = parseamap(&f, amn);
	freeifile(&f);
	return ok;
}

int
wbarenamap(AMap *am, int n, Part *part, u64int base, u64int size)
{
	Fmt f;
	ZBlock *b;

	b = alloczblock(size, 1, part->blocksize);
	if(b == nil)
		return -1;

	fmtzbinit(&f, b);

	if(outputamap(&f, am, n) < 0){
		seterr(ECorrupt, "arena set size too small");
		freezblock(b);
		return -1;
	}
	if(writepart(part, base, b->data, size) < 0 || flushpart(part) < 0){
		seterr(EAdmin, "can't write arena set: %r");
		freezblock(b);
		return -1;
	}
	freezblock(b);
	return 0;
}

/*
 * amap: n '\n' amapelem * n
 * n: u32int
 * amapelem: name '\t' astart '\t' astop '\n'
 * astart, astop: u64int
 */
int
parseamap(IFile *f, AMapN *amn)
{
	AMap *am;
	u64int v64;
	u32int v;
	char *s, *t, *flds[4];
	int i, n;

	/*
	 * arenas
	 */
	if(ifileu32int(f, &v) < 0){
		seterr(ECorrupt, "syntax error: bad number of elements in %s", f->name);
		return -1;
	}
	n = v;
	if(n > MaxAMap){
		seterr(ECorrupt, "illegal number of elements %d in %s",
			n, f->name);
		return -1;
	}
	am = MKNZ(AMap, n);
	if(am == nil){
		fprint(2, "out of memory\n");
		return -1;
	}
	for(i = 0; i < n; i++){
		s = ifileline(f);
		if(s)
			t = estrdup(s);
		else
			t = nil;
		if(s == nil || getfields(s, flds, 4, 0, "\t") != 3){
			fprint(2, "early eof after %d of %d, %s:#%d: %s\n", i, n, f->name, f->pos, t);
			free(t);
			return -1;
		}
		free(t);
		if(nameok(flds[0]) < 0)
			return -1;
		namecp(am[i].name, flds[0]);
		if(stru64int(flds[1], &v64) < 0){
			seterr(ECorrupt, "syntax error: bad arena base address in %s", f->name);
			free(am);
			return -1;
		}
		am[i].start = v64;
		if(stru64int(flds[2], &v64) < 0){
			seterr(ECorrupt, "syntax error: bad arena size in %s", f->name);
			free(am);
			return -1;
		}
		am[i].stop = v64;
	}

	amn->map = am;
	amn->n = n;
	return 0;
}

int
outputamap(Fmt *f, AMap *am, int n)
{
	int i;

	if(fmtprint(f, "%ud\n", n) < 0)
		return -1;
	for(i = 0; i < n; i++)
		if(fmtprint(f, "%s\t%llud\t%llud\n", am[i].name, am[i].start, am[i].stop) < 0)
			return -1;
	return 0;
}
