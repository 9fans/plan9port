#include "stdinc.h"
#include <bio.h>
#include "vac.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

int num = 1000;
int length = 20*1024;
int block= 1024;
int bush = 4;
int iter = 10000;
Biobuf *bout;
int maxdepth;

Source *mkroot(Cache*);
void new(Source*, int trace, int);
int delete(Source*);
void dump(Source*, int indent, ulong nentry);
void dumpone(Source *s);
int count(Source *s, int);
void stats(Source *s);

void
main(int argc, char *argv[])
{
	int i;
	Cache *c;
	char *host = nil;
	VtSession *z;
	int csize = 10000;
	Source *r;
	ulong t;

	t = time(0);
	fprint(1, "time = %lud\n", t);

	srand(t);

	ARGBEGIN{
	case 'i':
		iter = atoi(ARGF());
		break;
	case 'n':
		num = atoi(ARGF());
		break;
	case 'l':
		length = atoi(ARGF());
		break;
	case 'b':	
		block = atoi(ARGF());
		break;
	case 'h':
		host = ARGF();
		break;
	case 'u':
		bush = atoi(ARGF());
		break;
	case 'c':
		csize = atoi(ARGF());
		break;
	}ARGEND;

	vtAttach();

	bout = vtMemAllocZ(sizeof(Biobuf));
	Binit(bout, 1, OWRITE);

	fmtinstall('V', vtScoreFmt);
	fmtinstall('R', vtErrFmt);

	z = vtDial(host);
	if(z == nil)
		vtFatal("could not connect to server: %s", vtGetError());

	if(!vtConnect(z, 0))
		sysfatal("vtConnect: %r");

	c = cacheAlloc(z, block, csize);
	r = mkroot(c);
	for(i=0; i<num; i++)
		new(r, 0, 0);

	for(i=0; i<iter; i++) {
if(i % 10000 == 0)
stats(r);
		new(r, 0, 0);
		delete(r);
	}

	fprint(2, "count = %d top = %lud\n", count(r, 0), sourceGetDirSize(r));
//	cacheCheck(c);
fprint(2, "deleting\n");
	for(i=0; i<num; i++)
		delete(r);

//	dump(r, 0, 0);
	
	lumpDecRef(r->lump, 0);
	sourceRemove(r);
	cacheCheck(c);

	vtClose(z);
	vtDetach();

	exits(0);
}


Source *
mkroot(Cache *c)
{
	Lump *u;
	VtEntry *dir;
	Source *r;

	u = cacheAllocLump(c, VtDirType, cacheGetBlockSize(c), 1);
	dir = (VtEntry*)u->data;
	vtPutUint16(dir->psize, cacheGetBlockSize(c));
	vtPutUint16(dir->dsize, cacheGetBlockSize(c));
	dir->flag = VtEntryActive|VtEntryDir;
	memmove(dir->score, vtZeroScore, VtScoreSize);
	
	r = sourceAlloc(c, u, 0, 0);
	vtUnlock(u->lk);
	if(r == nil)
		sysfatal("could not create root source: %R");
	return r;
}

void
new(Source *s, int trace, int depth)
{
	int i, n;
	Source *ss;
	
	if(depth > maxdepth)
		maxdepth = depth;

	n = sourceGetDirSize(s);
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, nrand(n), 0);
		if(ss == nil)
			continue;
		if(ss->dir && frand() < 1./bush) {
			if(trace) {
				int j;
				for(j=0; j<trace; j++)
					Bprint(bout, " ");
				Bprint(bout, "decend %d\n", i);
			}
			new(ss, trace?trace+1:0, depth+1);
			sourceFree(ss);
			return;
		}
		sourceFree(ss);
	}
	ss = sourceCreate(s, s->psize, s->dsize, 1+frand()>.5, 0);
	if(ss == nil)
		fprint(2, "could not create directory: %R\n");
	if(trace) {
		int j;
		for(j=1; j<trace; j++)
			Bprint(bout, " ");
		Bprint(bout, "create %d %V\n", ss->entry, ss->lump->score);
	}
	sourceFree(ss);
}

int
delete(Source *s)
{
	int i, n;
	Source *ss;
	
	assert(s->dir);

	n = sourceGetDirSize(s);
	/* check if empty */
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, i, 1);
		if(ss != nil) {
			sourceFree(ss);
			break;
		}
	}
	if(i == n)
		return 0;
		
	for(;;) {
		ss = sourceOpen(s, nrand(n), 0);
		if(ss == nil)
			continue;
		if(ss->dir && delete(ss)) {
			sourceFree(ss);
			return 1;
		}
		if(1)
			break;
		sourceFree(ss);
	}


	sourceRemove(ss);
	return 1;
}

void
dumpone(Source *s)
{
	ulong i, n;
	Source *ss;

	Bprint(bout, "gen %4lud depth %d %V", s->gen, s->depth, s->lump->score);
	if(!s->dir) {
		Bprint(bout, " data size: %llud\n", s->size);
		return;
	}
	n = sourceGetDirSize(s);
	Bprint(bout, " dir size: %lud\n", n);
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, i, 1);
		if(ss == nil) {
fprint(2, "%lud: %R\n", i);
			continue;
		}
		Bprint(bout, "\t%lud %d %llud %V\n", i, ss->dir, ss->size, ss->lump->score);
		sourceFree(ss);
	}
	return;
}


void
dump(Source *s, int ident, ulong entry)
{
	ulong i, n;
	Source *ss;

	for(i=0; i<ident; i++)
		Bprint(bout, " ");
	Bprint(bout, "%4lud: gen %4lud depth %d", entry, s->gen, s->depth);
	if(!s->dir) {
		Bprint(bout, " data size: %llud\n", s->size);
		return;
	}
	n = sourceGetDirSize(s);
	Bprint(bout, " dir size: %lud\n", n);
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, i, 1);
		if(ss == nil)
			continue;
		dump(ss, ident+1, i);
		sourceFree(ss);
	}
	return;
}

int
count(Source *s, int rec)
{
	ulong i, n;
	int c;
	Source *ss;

	if(!s->dir)
		return 0;
	n = sourceGetDirSize(s);
	c = 0;
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, i, 1);
		if(ss == nil)
			continue;
		if(rec)
			c += count(ss, rec);
		c++;
		sourceFree(ss);
	}
	return c;
}

void
stats(Source *s)
{
	int n, i, c, cc, max;
	Source *ss;

	cc = 0;
	max = 0;
	n = sourceGetDirSize(s);
	for(i=0; i<n; i++) {
		ss = sourceOpen(s, i, 1);
		if(ss == nil)
			continue;
		cc++;
		c = count(ss, 1);
		if(c > max)
			max = c;
		sourceFree(ss);
	}
fprint(2, "count = %d top = %d depth=%d maxcount %d\n", cc, n, maxdepth, max);
}
