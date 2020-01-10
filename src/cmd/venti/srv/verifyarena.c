#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	verbose;
static int	fd;
static uchar	*data;
static int	blocksize;
static int	sleepms;
static vlong offset0;

void
usage(void)
{
	fprint(2, "usage: verifyarena [-b blocksize] [-s ms] [-v] [arenapart [name...]]\n");
	threadexitsall(0);
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
readblock(uchar *buf, int n)
{
	int nr, m;

	for(nr = 0; nr < n; nr += m){
		m = n - nr;
		m = read(fd, &buf[nr], m);
		if(m <= 0){
			if(m == 0)
				werrstr("early eof");
			return -1;
		}
	}
	return 0;
}

static void
verifyarena(char *name, vlong len)
{
	Arena arena;
	ArenaHead head;
	DigestState s;
	u64int n, e;
	u32int bs;
	u8int score[VtScoreSize];

	if(verbose)
		fprint(2, "%T verify %s\n", name);

	memset(&arena, 0, sizeof arena);
	memset(&s, 0, sizeof s);

	/*
	 * read a little bit, which will include the header
	 */
	if(readblock(data, HeadSize) < 0){
		fprint(2, "%T %s: reading header: %r\n", name);
		return;
	}
	sha1(data, HeadSize, nil, &s);
	if(unpackarenahead(&head, data) < 0){
		fprint(2, "%T %s: corrupt arena header: %r\n", name);
		return;
	}
	if(head.version != ArenaVersion4 && head.version != ArenaVersion5)
		fprint(2, "%T %s: warning: unknown arena version %d\n", name, head.version);
	if(len != 0 && len != head.size)
		fprint(2, "%T %s: warning: unexpected length %lld != %lld\n", name, head.size, len);
	if(strcmp(name, "<stdin>") != 0 && strcmp(head.name, name) != 0)
		fprint(2, "%T %s: warning: unexpected name %s\n", name, head.name);

	/*
	 * now we know how much to read
	 * read everything but the last block, which is special
	 */
	e = head.size - head.blocksize;
	bs = blocksize;
	for(n = HeadSize; n < e; n += bs){
		if(n + bs > e)
			bs = e - n;
		if(readblock(data, bs) < 0){
			fprint(2, "%T %s: read data: %r\n", name);
			return;
		}
		sha1(data, bs, nil, &s);
		if(sleepms)
			sleep(sleepms);
	}

	/*
	 * read the last block update the sum.
	 * the sum is calculated assuming the slot for the sum is zero.
	 */
	bs = head.blocksize;
	if(readblock(data, bs) < 0){
		fprint(2, "%T %s: read last block: %r\n", name);
		return;
	}
	sha1(data, bs-VtScoreSize, nil, &s);
	sha1(zeroscore, VtScoreSize, nil, &s);
	sha1(nil, 0, score, &s);

	/*
	 * validity check on the trailer
	 */
	arena.blocksize = head.blocksize;
	if(unpackarena(&arena, data) < 0){
		fprint(2, "%T %s: corrupt arena trailer: %r\n", name);
		return;
	}
	scorecp(arena.score, &data[arena.blocksize - VtScoreSize]);

	if(namecmp(arena.name, head.name) != 0){
		fprint(2, "%T %s: wrong name in trailer: %s vs. %s\n",
			name, head.name, arena.name);
		return;
	}
	if(arena.version != head.version){
		fprint(2, "%T %s: wrong version in trailer: %d vs. %d\n",
			name, head.version, arena.version);
		return;
	}
	arena.size = head.size - 2 * head.blocksize;

	/*
	 * check for no checksum or the same
	 */
	if(scorecmp(score, arena.score) == 0) {
		if(verbose)
			fprint(2, "%T %s: verified score\n", name);
	} else if(scorecmp(zeroscore, arena.score) == 0) {
		if(verbose || arena.diskstats.used > 0)
			fprint(2, "%T %s: unsealed %,lld bytes\n", name, arena.diskstats.used);
	} else{
		fprint(2, "%T %s: mismatch checksum - found=%V calculated=%V\n",
			name, arena.score, score);
		return;
	}
	if(verbose > 1)
		printarena(2, &arena);
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

void
threadmain(int argc, char *argv[])
{
	int i, nline;
	char *p, *q, *table, *f[10], line[256];
	vlong start, stop;
	ArenaPart ap;
	Part *part;

	needzeroscore();
	ventifmtinstall();
	blocksize = MaxIoSize;
	ARGBEGIN{
	case 'b':
		blocksize = unittoull(EARGF(usage()));
		break;
	case 's':
		sleepms = atoi(EARGF(usage()));
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	data = vtmalloc(MaxIo + blocksize);
	if((uintptr)data % MaxIo)
		data += MaxIo - (uintptr)data%MaxIo;

	if(argc == 0){
		fd = 0;
		verifyarena("<stdin>", 0);
		threadexitsall(nil);
	}

	if((part = initpart(argv[0], OREAD)) == nil)
		sysfatal("open partition %s: %r", argv[0]);
	fd = part->fd;
	offset0 = part->offset;

	if(preadblock(data, 8192, PartBlank) < 0)
		sysfatal("read arena part header: %r");
	if(unpackarenapart(&ap, data) < 0)
		sysfatal("corrupted arena part header: %r");
	if(verbose)
		fprint(2, "%T # arena part version=%d blocksize=%d arenabase=%d\n",
			ap.version, ap.blocksize, ap.arenabase);
	ap.tabbase = (PartBlank+HeadSize+ap.blocksize-1)&~(ap.blocksize-1);
	ap.tabsize = ap.arenabase - ap.tabbase;
	table = malloc(ap.tabsize+1);
	if(preadblock((uchar*)table, ap.tabsize, ap.tabbase) < 0)
		sysfatal("reading arena part directory: %r");
	table[ap.tabsize] = 0;

	nline = atoi(table);
	p = strchr(table, '\n');
	if(p)
		p++;
	for(i=0; i<nline; i++){
		if(p == nil){
			fprint(2, "%T warning: unexpected arena table end\n");
			break;
		}
		q = strchr(p, '\n');
		if(q)
			*q++ = 0;
		if(strlen(p) >= sizeof line){
			fprint(2, "%T warning: long arena table line: %s\n", p);
			p = q;
			continue;
		}
		strcpy(line, p);
		memset(f, 0, sizeof f);
		if(tokenize(line, f, nelem(f)) < 3){
			fprint(2, "%T warning: bad arena table line: %s\n", p);
			p = q;
			continue;
		}
		p = q;
		if(shouldcheck(f[0], argv+1, argc-1)){
			start = strtoull(f[1], 0, 0);
			stop = strtoull(f[2], 0, 0);
			if(stop <= start){
				fprint(2, "%T %s: bad start,stop %lld,%lld\n", f[0], stop, start);
				continue;
			}
			if(seek(fd, offset0+start, 0) < 0)
				fprint(2, "%T %s: seek to start: %r\n", f[0]);
			verifyarena(f[0], stop - start);
		}
	}
	for(i=1; i<argc; i++)
		if(argv[i] != 0)
			fprint(2, "%T %s: did not find arena\n", argv[i]);

	threadexitsall(nil);
}
