#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static uchar	*data;
static uchar	*data1;
static int	blocksize;
static int	sleepms;
static int	fd;
static int	force;
static vlong	offset0;

void
usage(void)
{
	fprint(2, "usage: reseal [-f] [-b blocksize] [-s ms] arenapart1 [name...]]\n");
	threadexitsall(0);
}

static int
pwriteblock(uchar *buf, int n, vlong off)
{
	int nr, m;

	for(nr = 0; nr < n; nr += m){
		m = n - nr;
		m = pwrite(fd, &buf[nr], m, offset0+off+nr);
		if(m <= 0)
			return -1;
	}
	return 0;
}

static int
preadblock(uchar *buf, int n, vlong off)
{
	int nr, m;

	for(nr = 0; nr < n; nr += m){
		m = n - nr;
		m = pread(fd, &buf[nr], m, offset0+off+nr);
		if(m <= 0){
			if(m == 0)
				werrstr("early eof");
			return -1;
		}
	}
	return 0;
}

static int
loadheader(char *name, ArenaHead *head, Arena *arena, vlong off)
{
	if(preadblock(data, head->blocksize, off + head->size - head->blocksize) < 0){
		fprint(2, "%s: reading arena tail: %r\n", name);
		return -1;
	}

	memset(arena, 0, sizeof *arena);
	if(unpackarena(arena, data) < 0){
		fprint(2, "%s: unpack arena tail: %r\n", name);
		return -1;
	}
	arena->blocksize = head->blocksize;
	arena->base = off + head->blocksize;
	arena->clumpmax = arena->blocksize / ClumpInfoSize;
	arena->size = head->size - 2*head->blocksize;

	if(arena->diskstats.sealed)
		scorecp(arena->score, data + head->blocksize - VtScoreSize);
	return 0;
}

uchar zero[VtScoreSize];

static int
verify(Arena *arena, void *data, uchar *newscore)
{
	vlong e, bs, n, o;
	DigestState ds, ds1;
	uchar score[VtScoreSize];

	/*
	 * now we know how much to read
	 * read everything but the last block, which is special
	 */
	e = arena->size + arena->blocksize;
	o = arena->base - arena->blocksize;
	bs = arena->blocksize;
	memset(&ds, 0, sizeof ds);
	for(n = 0; n < e; n += bs){
		if(preadblock(data, bs, o + n) < 0){
			werrstr("read: %r");
			return -1;
		}
		if(n + bs > e)
			bs = e - n;
		sha1(data, bs, nil, &ds);
	}

	/* last block */
	if(preadblock(data, arena->blocksize, o + e) < 0){
		werrstr("read: %r");
		return -1;
	}
	ds1 = ds;
	sha1(data, bs - VtScoreSize, nil, &ds);
	sha1(zero, VtScoreSize, score, &ds);
	if(scorecmp(score, arena->score) != 0){
		if(!force){
			werrstr("score mismatch: %V != %V", score, arena->score);
			return -1;
		}
		fprint(2, "warning: score mismatch %V != %V\n", score, arena->score);
	}

	/* prepare new last block */
	memset(data, 0, arena->blocksize);
	packarena(arena, data);
	sha1(data, bs, newscore, &ds1);
	scorecp((uchar*)data + arena->blocksize - VtScoreSize, newscore);

	return 0;
}

static void
resealarena(char *name, vlong len)
{
	ArenaHead head;
	Arena arena;
	DigestState s;
	u64int off;
	uchar newscore[VtScoreSize];

	fprint(2, "%s: begin reseal\n", name);

	memset(&s, 0, sizeof s);

	off = seek(fd, 0, 1);

	/*
	 * read a little bit, which will include the header
	 */
	if(preadblock(data, HeadSize, off) < 0){
		fprint(2, "%s: reading header: %r\n", name);
		return;
	}
	if(unpackarenahead(&head, data) < 0){
		fprint(2, "%s: corrupt arena header: %r\n", name);
		return;
	}
	if(head.version != ArenaVersion4 && head.version != ArenaVersion5)
		fprint(2, "%s: warning: unknown arena version %d\n", name, head.version);
	if(len != 0 && len != head.size)
		fprint(2, "%s: warning: unexpected length %lld != %lld\n", name, head.size, len);
	if(strcmp(name, "<stdin>") != 0 && strcmp(head.name, name) != 0)
		fprint(2, "%s: warning: unexpected name %s\n", name, head.name);

	if(loadheader(name, &head, &arena, off) < 0)
		return;

	if(!arena.diskstats.sealed){
		fprint(2, "%s: not sealed\n", name);
		return;
	}

	if(verify(&arena, data, newscore) < 0){
		fprint(2, "%s: failed to verify before reseal: %r\n", name);
		return;
	}

	if(pwriteblock(data, arena.blocksize, arena.base + arena.size) < 0){
		fprint(2, "%s: writing new tail: %r\n", name);
		return;
	}
	scorecp(arena.score, newscore);
	fprint(2, "%s: resealed: %V\n", name, newscore);

	if(verify(&arena, data, newscore) < 0){
		fprint(2, "%s: failed to verify after reseal!: %r\n", name);
		return;
	}

	fprint(2, "%s: verified: %V\n", name, newscore);
}

static int
shouldcheck(char *name, char **s, int n)
{
	int i;

	if(n == 0)
		return 1;

	for(i=0; i<n; i++){
		if(s[i] && strcmp(name, s[i]) == 0){
			s[i] = nil;
			return 1;
		}
	}
	return 0;
}

char *
readap(ArenaPart *ap)
{
	char *table;

	if(preadblock(data, 8192, PartBlank) < 0)
		sysfatal("read arena part header: %r");
	if(unpackarenapart(ap, data) < 0)
		sysfatal("corrupted arena part header: %r");
	fprint(2, "# arena part version=%d blocksize=%d arenabase=%d\n",
		ap->version, ap->blocksize, ap->arenabase);
	ap->tabbase = (PartBlank+HeadSize+ap->blocksize-1)&~(ap->blocksize-1);
	ap->tabsize = ap->arenabase - ap->tabbase;
	table = malloc(ap->tabsize+1);
	if(preadblock((uchar*)table, ap->tabsize, ap->tabbase) < 0)
		sysfatal("reading arena part directory: %r");
	table[ap->tabsize] = 0;
	return table;
}

void
threadmain(int argc, char *argv[])
{
	int i, nline;
	char *p, *q, *table, *f[10], line[256];
	vlong start, stop;
	ArenaPart ap;
	Part *part;

	ventifmtinstall();
	blocksize = MaxIoSize;
	ARGBEGIN{
	case 'b':
		blocksize = unittoull(EARGF(usage()));
		break;
	case 'f':
		force = 1;
		break;
	case 's':
		sleepms = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc < 2)
		usage();

	data = vtmalloc(blocksize);
	if((part = initpart(argv[0], ORDWR)) == nil)
		sysfatal("open partition %s: %r", argv[0]);
	fd = part->fd;
	offset0 = part->offset;

	table = readap(&ap);

	nline = atoi(table);
	p = strchr(table, '\n');
	if(p)
		p++;
	for(i=0; i<nline; i++){
		if(p == nil){
			fprint(2, "warning: unexpected arena table end\n");
			break;
		}
		q = strchr(p, '\n');
		if(q)
			*q++ = 0;
		if(strlen(p) >= sizeof line){
			fprint(2, "warning: long arena table line: %s\n", p);
			p = q;
			continue;
		}
		strcpy(line, p);
		memset(f, 0, sizeof f);
		if(tokenize(line, f, nelem(f)) < 3){
			fprint(2, "warning: bad arena table line: %s\n", p);
			p = q;
			continue;
		}
		p = q;
		if(shouldcheck(f[0], argv+1, argc-1)){
			start = strtoull(f[1], 0, 0);
			stop = strtoull(f[2], 0, 0);
			if(stop <= start){
				fprint(2, "%s: bad start,stop %lld,%lld\n", f[0], stop, start);
				continue;
			}
			if(seek(fd, start, 0) < 0)
				fprint(2, "%s: seek to start: %r\n", f[0]);
			resealarena(f[0], stop - start);
		}
	}
	for(i=2; i<argc; i++)
		if(argv[i] != 0)
			fprint(2, "%s: did not find arena\n", argv[i]);

	threadexitsall(nil);
}
