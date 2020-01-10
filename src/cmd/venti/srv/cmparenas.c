#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static int	verbose;
static int	fd;
static int	fd1;
static uchar	*data;
static uchar	*data1;
static int	blocksize;
static int	sleepms;

void
usage(void)
{
	fprint(2, "usage: cmparenas [-b blocksize] [-s ms] [-v] arenapart1 arenapart2 [name...]]\n");
	threadexitsall(0);
}

static int
preadblock(int fd, uchar *buf, int n, vlong off)
{
	int nr, m;

	for(nr = 0; nr < n; nr += m){
		m = n - nr;
		m = pread(fd, &buf[nr], m, off+nr);
		if(m <= 0){
			if(m == 0)
				werrstr("early eof");
			return -1;
		}
	}
	return 0;
}

static int
readblock(int fd, uchar *buf, int n)
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

static int
printheader(char *name, ArenaHead *head, int fd)
{
	Arena arena;
	vlong baseoff, lo, hi, off;
	int clumpmax;

	off = seek(fd, 0, 1);
	seek(fd, off + head->size - head->blocksize, 0);
	if(readblock(fd, data, head->blocksize) < 0){
		fprint(2, "%s: reading arena tail: %r\n", name);
		return -1;
	}
	seek(fd, off, 0);

	memset(&arena, 0, sizeof arena);
	if(unpackarena(&arena, data) < 0){
		fprint(2, "%s: unpack arena tail: %r\n", name);
		return -1;
	}
	arena.blocksize = head->blocksize;
	arena.base = off + head->blocksize;
	arena.clumpmax = arena.blocksize / ClumpInfoSize;
	arena.size = head->size - 2*head->blocksize;

	fprint(2, "%s: base=%llx size=%llx blocksize=%x\n", name, off, head->size, head->blocksize);

	baseoff = head->blocksize;
	fprint(2, "\t%llx-%llx: head\n", (vlong)0, baseoff);
	lo = baseoff;
	hi = baseoff + arena.diskstats.used;
	fprint(2, "\t%llx-%llx: data (%llx)\n", lo, hi, hi - lo);
	hi = head->size - head->blocksize;
	clumpmax = head->blocksize / ClumpInfoSize;
	if(clumpmax > 0)
		lo = hi - (u64int)arena.diskstats.clumps/clumpmax * head->blocksize;
	else
		lo = hi;
	fprint(2, "\t%llx-%llx: clumps (%llx)\n", lo, hi, hi - lo);
	fprint(2, "\t%llx-%llx: tail\n", hi, hi + head->blocksize);

	fprint(2, "arena:\n");
	printarena(2, &arena);
	return 0;
}

static void
cmparena(char *name, vlong len)
{
	ArenaHead head;
	DigestState s;
	u64int n, e;
	u32int bs;
	int i, j;
	char buf[20];

	fprint(2, "cmp %s\n", name);

	memset(&s, 0, sizeof s);

	/*
	 * read a little bit, which will include the header
	 */
	if(readblock(fd, data, HeadSize) < 0){
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

	if(readblock(fd1, data1, HeadSize) < 0){
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

	seek(fd, -HeadSize, 1);
	seek(fd1, -HeadSize, 1);

	if(printheader(name, &head, fd) < 0)
		return;

	/*
	 * now we know how much to read
	 * read everything but the last block, which is special
	 */
	e = head.size;
	bs = blocksize;
	for(n = 0; n < e; n += bs){
		if(n + bs > e)
			bs = e - n;
		if(readblock(fd, data, bs) < 0){
			fprint(2, "%s: read data: %r\n", name);
			return;
		}
		if(readblock(fd1, data1, bs) < 0){
			fprint(2, "%s: read data: %r\n", name);
			return;
		}
		if(memcmp(data, data1, bs) != 0){
			print("mismatch at %llx\n", n);
			for(i=0; i<bs; i+=16){
				if(memcmp(data+i, data1+i, 16) != 0){
					snprint(buf, sizeof buf, "%llx", n+i);
					print("%s ", buf);
					for(j=0; j<16; j++){
						print(" %.2ux", data[i+j]);
						if(j == 7)
							print(" -");
					}
					print("\n");
					print("%*s ", (int)strlen(buf), "");
					for(j=0; j<16; j++){
						print(" %.2ux", data1[i+j]);
						if(j == 7)
							print(" -");
					}
					print("\n");
				}
			}
		}
	}
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
readap(int fd, ArenaPart *ap)
{
	char *table;

	if(preadblock(fd, data, 8192, PartBlank) < 0)
		sysfatal("read arena part header: %r");
	if(unpackarenapart(ap, data) < 0)
		sysfatal("corrupted arena part header: %r");
	fprint(2, "# arena part version=%d blocksize=%d arenabase=%d\n",
		ap->version, ap->blocksize, ap->arenabase);
	ap->tabbase = (PartBlank+HeadSize+ap->blocksize-1)&~(ap->blocksize-1);
	ap->tabsize = ap->arenabase - ap->tabbase;
	table = malloc(ap->tabsize+1);
	if(preadblock(fd, (uchar*)table, ap->tabsize, ap->tabbase) < 0)
		sysfatal("reading arena part directory: %r");
	table[ap->tabsize] = 0;
	return table;
}

void
threadmain(int argc, char *argv[])
{
	int i, nline;
	char *p, *q, *table, *table1, *f[10], line[256];
	vlong start, stop;
	ArenaPart ap;
	ArenaPart ap1;

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

	if(argc < 2)
		usage();

	data = vtmalloc(blocksize);
	data1 = vtmalloc(blocksize);
	if((fd = open(argv[0], OREAD)) < 0)
		sysfatal("open %s: %r", argv[0]);
	if((fd1 = open(argv[1], OREAD)) < 0)
		sysfatal("open %s: %r", argv[0]);

	table = readap(fd, &ap);
	table1 = readap(fd1, &ap1);
	if(strcmp(table, table1) != 0)
		sysfatal("arena partitions do not have identical tables");

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
			if(seek(fd1, start, 0) < 0)
				fprint(2, "%s: seek to start: %r\n", f[0]);
			cmparena(f[0], stop - start);
		}
	}
	for(i=2; i<argc; i++)
		if(argv[i] != 0)
			fprint(2, "%s: did not find arena\n", argv[i]);

	threadexitsall(nil);
}
