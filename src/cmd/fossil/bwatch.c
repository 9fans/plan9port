#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

/*
 * Lock watcher.  Check that locking of blocks is always down.
 *
 * This is REALLY slow, and it won't work when the blocks aren't
 * arranged in a tree (e.g., after the first snapshot).  But it's great
 * for debugging.
 */
enum
{
	MaxLock = 16,
	HashSize = 1009,
};

/*
 * Thread-specific watch state.
 */
typedef struct WThread WThread;
struct WThread
{
	Block *b[MaxLock];	/* blocks currently held */
	uint nb;
	uint pid;
};

typedef struct WMap WMap;
typedef struct WEntry WEntry;

struct WEntry
{
	uchar c[VtScoreSize];
	uchar p[VtScoreSize];
	int off;

	WEntry *cprev;
	WEntry *cnext;
	WEntry *pprev;
	WEntry *pnext;
};

struct WMap
{
	QLock lk;

	WEntry *hchild[HashSize];
	WEntry *hparent[HashSize];
};

static WMap map;
static void **wp;
static uint blockSize;
static WEntry *pool;
uint bwatchDisabled;

static uint
hash(uchar score[VtScoreSize])
{
	uint i, h;

	h = 0;
	for(i=0; i<VtScoreSize; i++)
		h = h*37 + score[i];
	return h%HashSize;
}

#include <pool.h>
static void
freeWEntry(WEntry *e)
{
	memset(e, 0, sizeof(WEntry));
	e->pnext = pool;
	pool = e;
}

static WEntry*
allocWEntry(void)
{
	int i;
	WEntry *w;

	w = pool;
	if(w == nil){
		w = vtmallocz(1024*sizeof(WEntry));
		for(i=0; i<1024; i++)
			freeWEntry(&w[i]);
		w = pool;
	}
	pool = w->pnext;
	memset(w, 0, sizeof(WEntry));
	return w;
}

/*
 * remove all dependencies with score as a parent
 */
static void
_bwatchResetParent(uchar *score)
{
	WEntry *w, *next;
	uint h;

	h = hash(score);
	for(w=map.hparent[h]; w; w=next){
		next = w->pnext;
		if(memcmp(w->p, score, VtScoreSize) == 0){
			if(w->pnext)
				w->pnext->pprev = w->pprev;
			if(w->pprev)
				w->pprev->pnext = w->pnext;
			else
				map.hparent[h] = w->pnext;
			if(w->cnext)
				w->cnext->cprev = w->cprev;
			if(w->cprev)
				w->cprev->cnext = w->cnext;
			else
				map.hchild[hash(w->c)] = w->cnext;
			freeWEntry(w);
		}
	}
}
/*
 * and child
 */
static void
_bwatchResetChild(uchar *score)
{
	WEntry *w, *next;
	uint h;

	h = hash(score);
	for(w=map.hchild[h]; w; w=next){
		next = w->cnext;
		if(memcmp(w->c, score, VtScoreSize) == 0){
			if(w->pnext)
				w->pnext->pprev = w->pprev;
			if(w->pprev)
				w->pprev->pnext = w->pnext;
			else
				map.hparent[hash(w->p)] = w->pnext;
			if(w->cnext)
				w->cnext->cprev = w->cprev;
			if(w->cprev)
				w->cprev->cnext = w->cnext;
			else
				map.hchild[h] = w->cnext;
			freeWEntry(w);
		}
	}
}

static uchar*
parent(uchar c[VtScoreSize], int *off)
{
	WEntry *w;
	uint h;

	h = hash(c);
	for(w=map.hchild[h]; w; w=w->cnext)
		if(memcmp(w->c, c, VtScoreSize) == 0){
			*off = w->off;
			return w->p;
		}
	return nil;
}

static void
addChild(uchar p[VtEntrySize], uchar c[VtEntrySize], int off)
{
	uint h;
	WEntry *w;

	w = allocWEntry();
	memmove(w->p, p, VtScoreSize);
	memmove(w->c, c, VtScoreSize);
	w->off = off;

	h = hash(p);
	w->pnext = map.hparent[h];
	if(w->pnext)
		w->pnext->pprev = w;
	map.hparent[h] = w;

	h = hash(c);
	w->cnext = map.hchild[h];
	if(w->cnext)
		w->cnext->cprev = w;
	map.hchild[h] = w;
}

void
bwatchReset(uchar score[VtScoreSize])
{
	qlock(&map.lk);
	_bwatchResetParent(score);
	_bwatchResetChild(score);
	qunlock(&map.lk);
}

void
bwatchInit(void)
{
	wp = privalloc();
	*wp = nil;
}

void
bwatchSetBlockSize(uint bs)
{
	blockSize = bs;
}

static WThread*
getWThread(void)
{
	WThread *w;

	w = *wp;
	if(w == nil || w->pid != getpid()){
		w = vtmallocz(sizeof(WThread));
		*wp = w;
		w->pid = getpid();
	}
	return w;
}

/*
 * Derive dependencies from the contents of b.
 */
void
bwatchDependency(Block *b)
{
	int i, epb, ppb;
	Entry e;

	if(bwatchDisabled)
		return;

	qlock(&map.lk);
	_bwatchResetParent(b->score);

	switch(b->l.type){
	case BtData:
		break;

	case BtDir:
		epb = blockSize / VtEntrySize;
		for(i=0; i<epb; i++){
			entryUnpack(&e, b->data, i);
			if(!(e.flags & VtEntryActive))
				continue;
			addChild(b->score, e.score, i);
		}
		break;

	default:
		ppb = blockSize / VtScoreSize;
		for(i=0; i<ppb; i++)
			addChild(b->score, b->data+i*VtScoreSize, i);
		break;
	}
	qunlock(&map.lk);
}

static int
depth(uchar *s)
{
	int d, x;

	d = -1;
	while(s){
		d++;
		s = parent(s, &x);
	}
	return d;
}

static int
lockConflicts(uchar xhave[VtScoreSize], uchar xwant[VtScoreSize])
{
	uchar *have, *want;
	int havedepth, wantdepth, havepos, wantpos;

	have = xhave;
	want = xwant;

	havedepth = depth(have);
	wantdepth = depth(want);

	/*
	 * walk one or the other up until they're both
 	 * at the same level.
	 */
	havepos = -1;
	wantpos = -1;
	have = xhave;
	want = xwant;
	while(wantdepth > havedepth){
		wantdepth--;
		want = parent(want, &wantpos);
	}
	while(havedepth > wantdepth){
		havedepth--;
		have = parent(have, &havepos);
	}

	/*
	 * walk them up simultaneously until we reach
	 * a common ancestor.
	 */
	while(have && want && memcmp(have, want, VtScoreSize) != 0){
		have = parent(have, &havepos);
		want = parent(want, &wantpos);
	}

	/*
	 * not part of same tree.  happens mainly with
	 * newly allocated blocks.
	 */
	if(!have || !want)
		return 0;

	/*
	 * never walked want: means we want to lock
	 * an ancestor of have.  no no.
	 */
	if(wantpos == -1)
		return 1;

	/*
	 * never walked have: means we want to lock a
	 * child of have.  that's okay.
	 */
	if(havepos == -1)
		return 0;

	/*
	 * walked both: they're from different places in the tree.
	 * require that the left one be locked before the right one.
	 * (this is questionable, but it puts a total order on the block tree).
	 */
	return havepos < wantpos;
}

static void
stop(void)
{
	int fd;
	char buf[32];

	snprint(buf, sizeof buf, "#p/%d/ctl", getpid());
	fd = open(buf, OWRITE);
	write(fd, "stop", 4);
	close(fd);
}

/*
 * Check whether the calling thread can validly lock b.
 * That is, check that the calling thread doesn't hold
 * locks for any of b's children.
 */
void
bwatchLock(Block *b)
{
	int i;
	WThread *w;

	if(bwatchDisabled)
		return;

	if(b->part != PartData)
		return;

	qlock(&map.lk);
	w = getWThread();
	for(i=0; i<w->nb; i++){
		if(lockConflicts(w->b[i]->score, b->score)){
			fprint(2, "%d: have block %V; shouldn't lock %V\n",
				w->pid, w->b[i]->score, b->score);
			stop();
		}
	}
	qunlock(&map.lk);
	if(w->nb >= MaxLock){
		fprint(2, "%d: too many blocks held\n", w->pid);
		stop();
	}else
		w->b[w->nb++] = b;
}

/*
 * Note that the calling thread is about to unlock b.
 */
void
bwatchUnlock(Block *b)
{
	int i;
	WThread *w;

	if(bwatchDisabled)
		return;

	if(b->part != PartData)
		return;

	w = getWThread();
	for(i=0; i<w->nb; i++)
		if(w->b[i] == b)
			break;
	if(i>=w->nb){
		fprint(2, "%d: unlock of unlocked block %V\n", w->pid, b->score);
		stop();
	}else
		w->b[i] = w->b[--w->nb];
}
