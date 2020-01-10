#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>
#include <avl.h>
#include <bin.h>

enum
{
	// XXX What to do here?
	VtMaxLumpSize = 65535,
};

int changes;
int rewrite;
int ignoreerrors;
int fast;
int verbose;
int nskip;
int nwrite;

VtConn *zsrc, *zdst;
uchar zeroscore[VtScoreSize];	/* all zeros */

typedef struct ScoreTree ScoreTree;
struct ScoreTree
{
	Avl avl;
	uchar score[VtScoreSize];
	int type;
};

Avltree *scoretree;
Bin *scorebin;

static int
scoretreecmp(Avl *va, Avl *vb)
{
	ScoreTree *a, *b;
	int i;

	a = (ScoreTree*)va;
	b = (ScoreTree*)vb;

	i = memcmp(a->score, b->score, VtScoreSize);
	if(i != 0)
		return i;
	return a->type - b->type;
}

static int
havevisited(uchar score[VtScoreSize], int type)
{
	ScoreTree a;

	if(scoretree == nil)
		return 0;
	memmove(a.score, score, VtScoreSize);
	a.type = type;
	return lookupavl(scoretree, &a.avl) != nil;
}

static void
markvisited(uchar score[VtScoreSize], int type)
{
	ScoreTree *a;
	Avl *old;

	if(scoretree == nil)
		return;
	a = binalloc(&scorebin, sizeof *a, 1);
	memmove(a->score, score, VtScoreSize);
	a->type = type;
	insertavl(scoretree, &a->avl, &old);
}

void
usage(void)
{
	fprint(2, "usage: copy [-fimrVv] [-t type] srchost dsthost score\n");
	threadexitsall("usage");
}

void
walk(uchar score[VtScoreSize], uint type, int base, int depth)
{
	int i, n;
	uchar *buf;
	uchar nscore[VtScoreSize];
	VtEntry e;
	VtRoot root;

	if(verbose){
		for(i = 0; i < depth; i++)
			fprint(2, " ");
		fprint(2, "-> %d %d %d %V\n", depth, type, base, score);
	}

	if(memcmp(score, vtzeroscore, VtScoreSize) == 0 || memcmp(score, zeroscore, VtScoreSize) == 0)
		return;

	if(havevisited(score, type)){
		nskip++;
		return;
	}

	buf = vtmallocz(VtMaxLumpSize);
	if(fast && vtread(zdst, score, type, buf, VtMaxLumpSize) >= 0){
		if(verbose)
			fprint(2, "skip %V\n", score);
		free(buf);
		return;
	}

	n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
	if(n < 0){
		if(rewrite){
			changes++;
			memmove(score, vtzeroscore, VtScoreSize);
		}else if(!ignoreerrors)
			sysfatal("reading block %V (type %d): %r", score, type);
		return;
	}

	switch(type){
	case VtRootType:
		if(vtrootunpack(&root, buf) < 0){
			fprint(2, "warning: could not unpack root in %V %d\n", score, type);
			break;
		}
		walk(root.prev, VtRootType, 0, depth+1);
		walk(root.score, VtDirType, 0, depth+1);
		if(rewrite)
			vtrootpack(&root, buf);	/* walk might have changed score */
		break;

	case VtDirType:
		for(i=0; i*VtEntrySize < n; i++){
			if(vtentryunpack(&e, buf, i) < 0){
				fprint(2, "warning: could not unpack entry #%d in %V %d\n", i, score, type);
				continue;
			}
			if(!(e.flags & VtEntryActive))
				continue;
			walk(e.score, e.type, e.type&VtTypeBaseMask, depth+1);
			/*
			 * Don't repack unless we're rewriting -- some old
			 * vac files have psize==0 and dsize==0, and these
			 * get rewritten by vtentryunpack to have less strange
			 * block sizes.  So vtentryunpack; vtentrypack does not
			 * guarantee to preserve the exact bytes in buf.
			 */
			if(rewrite)
				vtentrypack(&e, buf, i);
		}
		break;

	case VtDataType:
		break;

	default:	/* pointers */
		for(i=0; i<n; i+=VtScoreSize)
			if(memcmp(buf+i, vtzeroscore, VtScoreSize) != 0)
				walk(buf+i, type-1, base, depth+1);
		break;
	}

	nwrite++;
	if(vtwrite(zdst, nscore, type, buf, n) < 0){
		/* figure out score for better error message */
		/* can't use input argument - might have changed contents */
		n = vtzerotruncate(type, buf, n);
		sha1(buf, n, score, nil);
		sysfatal("writing block %V (type %d): %r", score, type);
	}
	if(!rewrite && memcmp(score, nscore, VtScoreSize) != 0)
		sysfatal("not rewriting: wrote %V got %V", score, nscore);

	if((type !=0 || base !=0) && verbose){
		n = vtzerotruncate(type, buf, n);
		sha1(buf, n, score, nil);

		for(i = 0; i < depth; i++)
			fprint(2, " ");
		fprint(2, "<- %V\n", score);
	}

	markvisited(score, type);
	free(buf);
}

void
threadmain(int argc, char *argv[])
{
	int type, n;
	uchar score[VtScoreSize];
	uchar *buf;
	char *prefix;

	fmtinstall('F', vtfcallfmt);
	fmtinstall('V', vtscorefmt);

	type = -1;
	ARGBEGIN{
	case 'V':
		chattyventi++;
		break;
	case 'f':
		fast = 1;
		break;
	case 'i':
		if(rewrite)
			usage();
		ignoreerrors = 1;
		break;
	case 'm':
		scoretree = mkavltree(scoretreecmp);
		break;
	case 'r':
		if(ignoreerrors)
			usage();
		rewrite = 1;
		break;
	case 't':
		type = atoi(EARGF(usage()));
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 3)
		usage();

	if(vtparsescore(argv[2], &prefix, score) < 0)
		sysfatal("could not parse score: %r");

	buf = vtmallocz(VtMaxLumpSize);

	zsrc = vtdial(argv[0]);
	if(zsrc == nil)
		sysfatal("could not dial src server: %r");
	if(vtconnect(zsrc) < 0)
		sysfatal("vtconnect src: %r");

	zdst = vtdial(argv[1]);
	if(zdst == nil)
		sysfatal("could not dial dst server: %r");
	if(vtconnect(zdst) < 0)
		sysfatal("vtconnect dst: %r");

	if(type != -1){
		n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
		if(n < 0)
			sysfatal("could not read block: %r");
	}else{
		for(type=0; type<VtMaxType; type++){
			n = vtread(zsrc, score, type, buf, VtMaxLumpSize);
			if(n >= 0)
				break;
		}
		if(type == VtMaxType)
			sysfatal("could not find block %V of any type", score);
	}

	walk(score, type, VtDirType, 0);
	if(changes)
		print("%s:%V (%d pointers rewritten)\n", prefix, score, changes);

	if(verbose)
		print("%d skipped, %d written\n", nskip, nwrite);

	if(vtsync(zdst) < 0)
		sysfatal("could not sync dst server: %r");

	threadexitsall(0);
}
