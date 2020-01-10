#include "stdinc.h"
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "error.h"

int num = 100;
int length = 20*1024;
int block= 1024;
int bush = 4;
int iter = 100;
Biobuf *bout;
int maxdepth;

Source *mkroot(Cache*);
void new(Source*, int trace, int);
int delete(Source*);
int count(Source *s, int);
void stats(Source *s);
void dump(Source *s, int ident, ulong entry);
static void bench(Source *r);

void
main(int argc, char *argv[])
{
	int i;
	Fs *fs;
	int csize = 1000;
	ulong t;
	Source *r;

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

	fs = fsOpen(argv[0], nil, csize, OReadWrite);
	if(fs == nil)
		sysfatal("could not open fs: %r");

	t = time(0);

	srand(0);

	r = fs->source;
	dump(r, 0, 0);

	fprint(2, "count = %d\n", count(r, 1));
	for(i=0; i<num; i++)
		new(r, 0, 0);

	for(i=0; i<iter; i++){
		if(i % 10000 == 0)
			stats(r);
		new(r, 0, 0);
		delete(r);
	}

//	dump(r, 0, 0);

	fprint(2, "count = %d\n", count(r, 1));
//	cacheCheck(c);

	fprint(2, "deleting\n");
	for(i=0; i<num; i++)
		delete(r);
//	dump(r, 0, 0);

	fprint(2, "count = %d\n", count(r, 1));
	fprint(2, "total time = %ld\n", time(0)-t);

	fsClose(fs);
	vtDetach();
	exits(0);
}

static void
bench(Source *r)
{
	vlong t;
	Entry e;
	int i;

	t = nsec();

	for(i=0; i<1000000; i++)
		sourceGetEntry(r, &e);

	fprint(2, "%f\n", 1e-9*(nsec() - t));
}

void
new(Source *s, int trace, int depth)
{
	int i, n;
	Source *ss;
	Entry e;

	if(depth > maxdepth)
		maxdepth = depth;

	Bflush(bout);

	n = sourceGetDirSize(s);
	for(i=0; i<n; i++){
		ss = sourceOpen(s, nrand(n), OReadWrite);
		if(ss == nil || !sourceGetEntry(ss, &e))
			continue;
		if((e.flags & VtEntryDir) && frand() < 1./bush){
			if(trace){
				int j;
				for(j=0; j<trace; j++)
					Bprint(bout, " ");
				Bprint(bout, "decend %d\n", i);
			}
			new(ss, trace?trace+1:0, depth+1);
			sourceClose(ss);
			return;
		}
		sourceClose(ss);
	}
	ss = sourceCreate(s, s->dsize, 1+frand()>.5, 0);
	if(ss == nil){
		Bprint(bout, "could not create directory: %R\n");
		return;
	}
	if(trace){
		int j;
		for(j=1; j<trace; j++)
			Bprint(bout, " ");
		Bprint(bout, "create %d\n", ss->offset);
	}
	sourceClose(ss);
}

int
delete(Source *s)
{
	int i, n;
	Source *ss;

	n = sourceGetDirSize(s);
	/* check if empty */
	for(i=0; i<n; i++){
		ss = sourceOpen(s, i, OReadWrite);
		if(ss != nil){
			sourceClose(ss);
			break;
		}
	}
	if(i == n)
		return 0;

	for(;;){
		ss = sourceOpen(s, nrand(n), OReadWrite);
		if(ss == nil)
			continue;
		if(s->dir && delete(ss)){
			sourceClose(ss);
			return 1;
		}
		if(1)
			break;
		sourceClose(ss);
	}


	sourceRemove(ss);
	return 1;
}

void
dump(Source *s, int ident, ulong entry)
{
	ulong i, n;
	Source *ss;
	Entry e;

	for(i=0; i<ident; i++)
		Bprint(bout, " ");

	if(!sourceGetEntry(s, &e)){
		fprint(2, "sourceGetEntry failed: %r\n");
		return;
	}

	Bprint(bout, "%4lud: gen %4ud depth %d tag=%x score=%V",
		entry, e.gen, e.depth, e.tag, e.score);
	if(!s->dir){
		Bprint(bout, " data size: %llud\n", e.size);
		return;
	}
	n = sourceGetDirSize(s);
	Bprint(bout, " dir size: %lud\n", n);
	for(i=0; i<n; i++){
		ss = sourceOpen(s, i, 1);
		if(ss == nil)
			continue;
		dump(ss, ident+1, i);
		sourceClose(ss);
	}
	return;
}

int
count(Source *s, int rec)
{
	ulong i, n;
	int c;
	Source *ss;

	n = sourceGetDirSize(s);
	c = 0;
	for(i=0; i<n; i++){
		ss = sourceOpen(s, i, OReadOnly);
		if(ss == nil)
			continue;
		if(rec)
			c += count(ss, rec);
		c++;
		sourceClose(ss);
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
	for(i=0; i<n; i++){
		ss = sourceOpen(s, i, 1);
		if(ss == nil)
			continue;
		cc++;
		c = count(ss, 1);
		if(c > max)
			max = c;
		sourceClose(ss);
	}
fprint(2, "count = %d top = %d depth=%d maxcount %d\n", cc, n, maxdepth, max);
}
