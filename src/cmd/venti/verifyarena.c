#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	verbose;

void
usage(void)
{
	fprint(2, "usage: verifyarena [-v]\n");
	threadexitsall(0);
}

static void
readblock(uchar *buf, int n)
{
	int nr, m;

	for(nr = 0; nr < n; nr += m){
		m = n - nr;
		m = read(0, &buf[nr], m);
		if(m <= 0)
			sysfatal("can't read arena from standard input: %r");
	}
}

static void
verifyarena(void)
{
	Arena arena;
	ArenaHead head;
	ZBlock *b;
	DigestState s;
	u64int n, e;
	u32int bs;
	u8int score[VtScoreSize];

	fprint(2, "verify arena from standard input\n");

	memset(&arena, 0, sizeof arena);
	memset(&s, 0, sizeof s);

	/*
	 * read the little bit, which will included the header
	 */
	bs = MaxIoSize;
	b = alloczblock(bs, 0);
	readblock(b->data, HeadSize);
	sha1(b->data, HeadSize, nil, &s);
	if(unpackarenahead(&head, b->data) < 0)
		sysfatal("corrupted arena header: %r");
	if(head.version != ArenaVersion)
		fprint(2, "warning: unknown arena version %d\n", head.version);

	/*
	 * now we know how much to read
	 * read everything but the last block, which is special
	 */
	e = head.size - head.blocksize;
	for(n = HeadSize; n < e; n += bs){
		if(n + bs > e)
			bs = e - n;
		readblock(b->data, bs);
		sha1(b->data, bs, nil, &s);
	}

	/*
	 * read the last block update the sum.
	 * the sum is calculated assuming the slot for the sum is zero.
	 */
	bs = head.blocksize;
	readblock(b->data, bs);
	sha1(b->data, bs-VtScoreSize, nil, &s);
	sha1(zeroscore, VtScoreSize, nil, &s);
	sha1(nil, 0, score, &s);

	/*
	 * validity check on the trailer
	 */
	arena.blocksize = head.blocksize;
	if(unpackarena(&arena, b->data) < 0)
		sysfatal("corrupted arena trailer: %r");
	scorecp(arena.score, &b->data[arena.blocksize - VtScoreSize]);

	if(namecmp(arena.name, head.name) != 0)
		sysfatal("arena header and trailer names clash: %s vs. %s\n", head.name, arena.name);
	if(arena.version != head.version)
		sysfatal("arena header and trailer versions clash: %d vs. %d\n", head.version, arena.version);
	arena.size = head.size - 2 * head.blocksize;

	/*
	 * check for no checksum or the same
	 */
	if(scorecmp(score, arena.score) != 0){
		if(scorecmp(zeroscore, arena.score) != 0)
			fprint(2, "warning: mismatched checksums for arena=%s, found=%V calculated=%V",
				arena.name, arena.score, score);
		scorecp(arena.score, score);
	}else
		fprint(2, "matched score\n");

	printarena(2, &arena);
}

void
threadmain(int argc, char *argv[])
{
	fmtinstall('V', vtscorefmt);
	statsinit();

	ARGBEGIN{
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	readonly = 1;

	if(argc != 0)
		usage();

	verifyarena();
	threadexitsall(0);
}
