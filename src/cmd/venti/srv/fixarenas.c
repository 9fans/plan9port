/*
 * Check and fix an arena partition.
 *
 * This is a lot grittier than the rest of Venti because
 * it can't just give up if a byte here or there is wrong.
 *
 * The rule here (hopefully followed!) is that block corruption
 * only ever has a local effect -- there are no blocks that you
 * can wipe out that will cause large portions of
 * uncorrupted data blocks to be useless.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "whack.h"

#define ROUNDUP(x,n)		(((x)+(n)-1)&~((n)-1))

#pragma varargck type "z" uvlong
#pragma varargck type "z" vlong
#pragma varargck type "t" uint

enum
{
	K = 1024,
	M = 1024*1024,
	G = 1024*1024*1024,

	Block = 4096,
};

int debugsha1;

int verbose;
Part *part;
char *file;
char *basename;
char *dumpbase;
int fix;
int badreads;
int unseal;
uchar zero[MaxDiskBlock];

Arena lastarena;
ArenaPart ap;
uvlong arenasize;
int nbadread;
int nbad;
uvlong partend;
void checkarena(vlong, int);

void
usage(void)
{
	fprint(2, "usage: fixarenas [-fv] [-a arenasize] [-b blocksize] file [ranges]\n");
	threadexitsall(0);
}

/*
 * Format number in simplest way that is okay with unittoull.
 */
static int
zfmt(Fmt *fmt)
{
	vlong x;

	x = va_arg(fmt->args, vlong);
	if(x == 0)
		return fmtstrcpy(fmt, "0");
	if(x%G == 0)
		return fmtprint(fmt, "%lldG", x/G);
	if(x%M == 0)
		return fmtprint(fmt, "%lldM", x/M);
	if(x%K == 0)
		return fmtprint(fmt, "%lldK", x/K);
	return fmtprint(fmt, "%lld", x);
}

/*
 * Format time like ctime without newline.
 */
static int
tfmt(Fmt *fmt)
{
	uint t;
	char buf[30];

	t = va_arg(fmt->args, uint);
	strcpy(buf, ctime(t));
	buf[28] = 0;
	return fmtstrcpy(fmt, buf);
}

/*
 * Coalesce messages about unreadable sectors into larger ranges.
 * bad(0, 0) flushes the buffer.
 */
static void
bad(char *msg, vlong o, int len)
{
	static vlong lb0, lb1;
	static char *lmsg;

	if(msg == nil)
		msg = lmsg;
	if(o == -1){
		lmsg = nil;
		lb0 = 0;
		lb1 = 0;
		return;
	}
	if(lb1 != o || (msg && lmsg && strcmp(msg, lmsg) != 0)){
		if(lb0 != lb1)
			print("%s %#llux+%#llux (%,lld+%,lld)\n",
				lmsg, lb0, lb1-lb0, lb0, lb1-lb0);
		lb0 = o;
	}
	lmsg = msg;
	lb1 = o+len;
}

/*
 * Read in the len bytes of data at the offset.  If can't for whatever reason,
 * fill it with garbage but print an error.
 */
static uchar*
readdisk(uchar *buf, vlong offset, int len)
{
	int i, j, k, n;

	if(offset >= partend){
		memset(buf, 0xFB, len);
		return buf;
	}

	if(offset+len > partend){
		memset(buf, 0xFB, len);
		len = partend - offset;
	}

	if(readpart(part, offset, buf, len) >= 0)
		return buf;

	/*
	 * The read failed.  Clear the buffer to nonsense, and
	 * then try reading in smaller pieces.  If that fails,
	 * read in even smaller pieces.  And so on down to sectors.
	 */
	memset(buf, 0xFD, len);
	for(i=0; i<len; i+=64*K){
		n = 64*K;
		if(i+n > len)
			n = len-i;
		if(readpart(part, offset+i, buf+i, n) >= 0)
			continue;
		for(j=i; j<len && j<i+64*K; j+=4*K){
			n = 4*K;
			if(j+n > len)
				n = len-j;
			if(readpart(part, offset+j, buf+j, n) >= 0)
				continue;
			for(k=j; k<len && k<j+4*K; k+=512){
				if(readpart(part, offset+k, buf+k, 512) >= 0)
					continue;
				bad("disk read failed at", k, 512);
				badreads++;
			}
		}
	}
	bad(nil, 0, 0);
	return buf;
}

/*
 * Buffer to support running SHA1 hash of the disk.
 */
typedef struct Shabuf Shabuf;
struct Shabuf
{
	int fd;
	vlong offset;
	DigestState state;
	int rollback;
	vlong r0;
	DigestState *hist;
	int nhist;
};

void
sbdebug(Shabuf *sb, char *file)
{
	int fd;

	if(sb->fd > 0){
		close(sb->fd);
		sb->fd = 0;
	}
	if((fd = create(file, OWRITE, 0666)) < 0)
		return;
	if(fd == 0){
		fd = dup(fd, -1);
		close(0);
	}
	sb->fd = fd;
}

void
sbupdate(Shabuf *sb, uchar *p, vlong offset, int len)
{
	int n, x;
	vlong o;

	if(sb->rollback && !sb->hist){
		sb->r0 = offset;
		sb->nhist = 1;
		sb->hist = vtmalloc(sb->nhist*sizeof *sb->hist);
		memset(sb->hist, 0, sizeof sb->hist[0]);
	}
	if(sb->r0 == 0)
		sb->r0 = offset;

	if(sb->offset < offset || sb->offset >= offset+len){
		if(0) print("sbupdate %p %#llux+%d but offset=%#llux\n",
			p, offset, len, sb->offset);
		return;
	}
	x = sb->offset - offset;
	if(0) print("sbupdate %p %#llux+%d skip %d\n",
		sb, offset, len, x);
	if(x){
		p += x;
		offset += x;
		len -= x;
	}
	assert(sb->offset == offset);

	if(sb->fd > 0)
		pwrite(sb->fd, p, len, offset - sb->r0);

	if(!sb->rollback){
		sha1(p, len, nil, &sb->state);
		sb->offset += len;
		return;
	}

	/* save state every 4M so we can roll back quickly */
	o = offset - sb->r0;
	while(len > 0){
		n = 4*M - o%(4*M);
		if(n > len)
			n = len;
		sha1(p, n, nil, &sb->state);
		sb->offset += n;
		o += n;
		p += n;
		len -= n;
		if(o%(4*M) == 0){
			x = o/(4*M);
			if(x >= sb->nhist){
				if(x != sb->nhist)
					print("oops! x=%d nhist=%d\n", x, sb->nhist);
				sb->nhist += 32;
				sb->hist = vtrealloc(sb->hist, sb->nhist*sizeof *sb->hist);
			}
			sb->hist[x] = sb->state;
		}
	}
}

void
sbdiskhash(Shabuf *sb, vlong eoffset)
{
	static uchar dbuf[4*M];
	int n;

	while(sb->offset < eoffset){
		n = sizeof dbuf;
		if(sb->offset+n > eoffset)
			n = eoffset - sb->offset;
		readdisk(dbuf, sb->offset, n);
		sbupdate(sb, dbuf, sb->offset, n);
	}
}

void
sbrollback(Shabuf *sb, vlong offset)
{
	int x;
	vlong o;
	Dir d;

	if(!sb->rollback || !sb->r0){
		print("cannot rollback sha\n");
		return;
	}
	if(offset >= sb->offset)
		return;
	o = offset - sb->r0;
	x = o/(4*M);
	if(x >= sb->nhist){
		print("cannot rollback sha\n");
		return;
	}
	sb->state = sb->hist[x];
	sb->offset = sb->r0 + x*4*M;
	assert(sb->offset <= offset);

	if(sb->fd > 0){
		nulldir(&d);
		d.length = sb->offset - sb->r0;
		dirfwstat(sb->fd, &d);
	}
}

void
sbscore(Shabuf *sb, uchar *score)
{
	if(sb->hist){
		free(sb->hist);
		sb->hist = nil;
	}
	sha1(nil, 0, score, &sb->state);
}

/*
 * If we're fixing arenas, then editing this memory edits the disk!
 * It will be written back out as new data is paged in.
 */
uchar buf[4*M];
uchar sbuf[4*M];
vlong bufoffset;
int buflen;

static void pageout(void);
static uchar*
pagein(vlong offset, int len)
{
	pageout();
	if(offset >= partend){
		memset(buf, 0xFB, sizeof buf);
		return buf;
	}

	if(offset+len > partend){
		memset(buf, 0xFB, sizeof buf);
		len = partend - offset;
	}
	bufoffset = offset;
	buflen = len;
	readdisk(buf, offset, len);
	memmove(sbuf, buf, len);
	return buf;
}

static void
pageout(void)
{
	if(buflen==0 || !fix || memcmp(buf, sbuf, buflen) == 0){
		buflen = 0;
		return;
	}
	if(writepart(part, bufoffset, buf, buflen) < 0)
		print("disk write failed at %#llux+%#ux (%,lld+%,d)\n",
			bufoffset, buflen, bufoffset, buflen);
	buflen = 0;
}

static void
zerorange(vlong offset, int len)
{
	int i;
	vlong ooff;
	int olen;
	enum { MinBlock = 4*K, MaxBlock = 8*K };

	if(0)
	if(bufoffset <= offset && offset+len <= bufoffset+buflen){
		memset(buf+(offset-bufoffset), 0, len);
		return;
	}

	ooff = bufoffset;
	olen = buflen;

	i = offset%MinBlock;
	if(i+len < MaxBlock){
		pagein(offset-i, (len+MinBlock-1)&~(MinBlock-1));
		memset(buf+i, 0, len);
	}else{
		pagein(offset-i, MaxBlock);
		memset(buf+i, 0, MaxBlock-i);
		offset += MaxBlock-i;
		len -= MaxBlock-i;
		while(len >= MaxBlock){
			pagein(offset, MaxBlock);
			memset(buf, 0, MaxBlock);
			offset += MaxBlock;
			len -= MaxBlock;
		}
		pagein(offset, (len+MinBlock-1)&~(MinBlock-1));
		memset(buf, 0, len);
	}
	pagein(ooff, olen);
}

/*
 * read/write integers
 *
static void
p16(uchar *p, u16int u)
{
	p[0] = (u>>8) & 0xFF;
	p[1] = u & 0xFF;
}
*/

static u16int
u16(uchar *p)
{
	return (p[0]<<8)|p[1];
}

static void
p32(uchar *p, u32int u)
{
	p[0] = (u>>24) & 0xFF;
	p[1] = (u>>16) & 0xFF;
	p[2] = (u>>8) & 0xFF;
	p[3] = u & 0xFF;
}

static u32int
u32(uchar *p)
{
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

/*
static void
p64(uchar *p, u64int u)
{
	p32(p, u>>32);
	p32(p, u);
}
*/

static u64int
u64(uchar *p)
{
	return ((u64int)u32(p)<<32) | u32(p+4);
}

static int
vlongcmp(const void *va, const void *vb)
{
	vlong a, b;

	a = *(vlong*)va;
	b = *(vlong*)vb;
	if(a < b)
		return -1;
	if(b > a)
		return 1;
	return 0;
}

/* D and S are in draw.h */
#define D VD
#define S VS

enum
{
	D = 0x10000,
	Z = 0x20000,
	S = 0x30000,
	T = 0x40000,
	N = 0xFFFF
};
typedef struct Info Info;
struct Info
{
	int len;
	char *name;
};

Info partinfo[] = {
	4,	"magic",
	D|4,	"version",
	Z|4,	"blocksize",
	4,	"arenabase",
	0
};

Info headinfo4[] = {
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	Z|4,	"blocksize",
	Z|8,	"size",
	0
};

Info headinfo5[] = {
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	Z|4,	"blocksize",
	Z|8,	"size",
	4,	"clumpmagic",
	0
};

Info tailinfo4[] = {
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	D|4,	"clumps",
	D|4,	"cclumps",
	T|4,	"ctime",
	T|4,	"wtime",
	D|8,	"used",
	D|8,	"uncsize",
	1,	"sealed",
	0
};

Info tailinfo4a[] = {
	/* tailinfo 4 */
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	D|4,	"clumps",
	D|4,	"cclumps",
	T|4,	"ctime",
	T|4,	"wtime",
	D|8,	"used",
	D|8,	"uncsize",
	1,	"sealed",

	/* mem stats */
	1,	"extension",
	D|4,	"mem.clumps",
	D|4,	"mem.cclumps",
	D|8,	"mem.used",
	D|8,	"mem.uncsize",
	1,	"mem.sealed",
	0
};

Info tailinfo5[] = {
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	D|4,	"clumps",
	D|4,	"cclumps",
	T|4,	"ctime",
	T|4,	"wtime",
	4,	"clumpmagic",
	D|8,	"used",
	D|8,	"uncsize",
	1,	"sealed",
	0
};

Info tailinfo5a[] = {
	/* tailinfo 5 */
	4,	"magic",
	D|4,	"version",
	S|ANameSize,	"name",
	D|4,	"clumps",
	D|4,	"cclumps",
	T|4,	"ctime",
	T|4,	"wtime",
	4,	"clumpmagic",
	D|8,	"used",
	D|8,	"uncsize",
	1,	"sealed",

	/* mem stats */
	1,	"extension",
	D|4,	"mem.clumps",
	D|4,	"mem.cclumps",
	D|8,	"mem.used",
	D|8,	"mem.uncsize",
	1,	"mem.sealed",
	0
};

void
showdiffs(uchar *want, uchar *have, int len, Info *info)
{
	int n;

	while(len > 0 && (n=info->len&N) > 0){
		if(memcmp(have, want, n) != 0){
			switch(info->len){
			case 1:
				print("\t%s: correct=%d disk=%d\n",
					info->name, *want, *have);
				break;
			case 4:
				print("\t%s: correct=%#ux disk=%#ux\n",
					info->name, u32(want), u32(have));
				break;
			case D|4:
				print("\t%s: correct=%,ud disk=%,ud\n",
					info->name, u32(want), u32(have));
				break;
			case T|4:
				print("\t%s: correct=%t\n\t\tdisk=%t\n",
					info->name, u32(want), u32(have));
				break;
			case Z|4:
				print("\t%s: correct=%z disk=%z\n",
					info->name, (uvlong)u32(want), (uvlong)u32(have));
				break;
			case D|8:
				print("\t%s: correct=%,lld disk=%,lld\n",
					info->name, u64(want), u64(have));
				break;
			case Z|8:
				print("\t%s: correct=%z disk=%z\n",
					info->name, u64(want), u64(have));
				break;
			case S|ANameSize:
				print("\t%s: correct=%s disk=%.*s\n",
					info->name, (char*)want,
					utfnlen((char*)have, ANameSize-1),
					(char*)have);
				break;
			default:
				print("\t%s: correct=%.*H disk=%.*H\n",
					info->name, n, want, n, have);
				break;
			}
		}
		have += n;
		want += n;
		len -= n;
		info++;
	}
	if(len > 0 && memcmp(have, want, len) != 0){
		if(memcmp(want, zero, len) != 0)
			print("!!\textra want data in showdiffs (bug in fixarenas)\n");
		else
			print("\tnon-zero data on disk after structure\n");
		if(verbose > 1){
			print("want: %.*H\n", len, want);
			print("have: %.*H\n", len, have);
		}
	}
}

/*
 * Does part begin with an arena?
 */
int
isonearena(void)
{
	return u32(pagein(0, Block)) == ArenaHeadMagic;
}

static int tabsizes[] = { 16*1024, 64*1024, 512*1024, 768*1024, };
/*
 * Poke around on the disk to guess what the ArenaPart numbers are.
 */
void
guessgeometry(void)
{
	int i, j, n, bestn, ndiff, nhead, ntail;
	uchar *p, *ep, *sp;
	u64int diff[100], head[20], tail[20];
	u64int offset, bestdiff;

	ap.version = ArenaPartVersion;

	if(arenasize == 0 || ap.blocksize == 0){
		/*
		 * The ArenaPart block at offset PartBlank may be corrupt or just wrong.
		 * Instead, look for the individual arena headers and tails, which there
		 * are many of, and once we've seen enough, infer the spacing.
		 *
		 * Of course, nothing in the file format requires that arenas be evenly
		 * spaced, but fmtarenas always does that for us.
		 */
		nhead = 0;
		ntail = 0;
		for(offset=PartBlank; offset<partend; offset+=4*M){
			p = pagein(offset, 4*M);
			for(sp=p, ep=p+4*M; p<ep; p+=K){
				if(u32(p) == ArenaHeadMagic && nhead < nelem(head)){
					if(verbose)
						print("arena head at %#llx\n", offset+(p-sp));
					head[nhead++] = offset+(p-sp);
				}
				if(u32(p) == ArenaMagic && ntail < nelem(tail)){
					tail[ntail++] = offset+(p-sp);
					if(verbose)
						print("arena tail at %#llx\n", offset+(p-sp));
				}
			}
			if(nhead == nelem(head) && ntail == nelem(tail))
				break;
		}
		if(nhead < 3 && ntail < 3)
			sysfatal("too few intact arenas: %d heads, %d tails", nhead, ntail);

		/*
		 * Arena size is likely the most common
		 * inter-head or inter-tail spacing.
		 */
		ndiff = 0;
		for(i=1; i<nhead; i++)
			diff[ndiff++] = head[i] - head[i-1];
		for(i=1; i<ntail; i++)
			diff[ndiff++] = tail[i] - tail[i-1];
		qsort(diff, ndiff, sizeof diff[0], vlongcmp);
		bestn = 0;
		bestdiff = 0;
		for(i=1, n=1; i<=ndiff; i++, n++){
			if(i==ndiff || diff[i] != diff[i-1]){
				if(n > bestn){
					bestn = n;
					bestdiff = diff[i-1];
				}
				n = 0;
			}
		}
		print("arena size likely %z (%d of %d)\n", bestdiff, bestn, ndiff);
		if(arenasize != 0 && arenasize != bestdiff)
			print("using user-specified size %z instead\n", arenasize);
		else
			arenasize = bestdiff;

		/*
		 * The arena tail for an arena is arenasize-blocksize from the head.
		 */
		ndiff = 0;
		for(i=j=0; i<nhead && j<ntail; ){
			if(tail[j] < head[i]){
				j++;
				continue;
			}
			if(tail[j] < head[i]+arenasize){
				diff[ndiff++] = head[i]+arenasize - tail[j];
				j++;
				continue;
			}
			i++;
		}
		if(ndiff < 3)
			sysfatal("too few intact arenas: %d head, tail pairs", ndiff);
		qsort(diff, ndiff, sizeof diff[0], vlongcmp);
		bestn = 0;
		bestdiff = 0;
		for(i=1, n=1; i<=ndiff; i++, n++){
			if(i==ndiff || diff[i] != diff[i-1]){
				if(n > bestn){
					bestn = n;
					bestdiff = diff[i-1];
				}
				n = 0;
			}
		}
		print("block size likely %z (%d of %d)\n", bestdiff, bestn, ndiff);
		if(ap.blocksize != 0 && ap.blocksize != bestdiff)
			print("using user-specified size %z instead\n", (vlong)ap.blocksize);
		else
			ap.blocksize = bestdiff;
		if(ap.blocksize == 0 || ap.blocksize&(ap.blocksize-1))
			sysfatal("block size not a power of two");
		if(ap.blocksize > MaxDiskBlock)
			sysfatal("block size too big (max=%d)", MaxDiskBlock);

		/*
		 * Use head/tail information to deduce arena base.
		 */
		ndiff = 0;
		for(i=0; i<nhead; i++)
			diff[ndiff++] = head[i]%arenasize;
		for(i=0; i<ntail; i++)
			diff[ndiff++] = (tail[i]+ap.blocksize)%arenasize;
		qsort(diff, ndiff, sizeof diff[0], vlongcmp);
		bestn = 0;
		bestdiff = 0;
		for(i=1, n=1; i<=ndiff; i++, n++){
			if(i==ndiff || diff[i] != diff[i-1]){
				if(n > bestn){
					bestn = n;
					bestdiff = diff[i-1];
				}
				n = 0;
			}
		}
		ap.arenabase = bestdiff;
	}

	ap.tabbase = ROUNDUP(PartBlank+HeadSize, ap.blocksize);
	/*
	 * XXX pick up table, check arenabase.
	 * XXX pick up table, record base name.
	 */

	/*
	 * Somewhat standard computation.
	 * Fmtarenas used to use 64k tab, now uses 512k tab.
	 */
	if(ap.arenabase == 0){
		print("trying standard arena bases...\n");
		for(i=0; i<nelem(tabsizes); i++){
			ap.arenabase = ROUNDUP(PartBlank+HeadSize+tabsizes[i], ap.blocksize);
			p = pagein(ap.arenabase, Block);
			if(u32(p) == ArenaHeadMagic)
				break;
		}
	}
	p = pagein(ap.arenabase, Block);
	print("arena base likely %z%s\n", (vlong)ap.arenabase,
		u32(p)!=ArenaHeadMagic ? " (but no arena head there)" : "");

	ap.tabsize = ap.arenabase - ap.tabbase;
}

/*
 * Check the arena partition blocks and then the arenas listed in range.
 */
void
checkarenas(char *range)
{
	char *s, *t;
	int i, lo, hi, narena;
	uchar dbuf[HeadSize];
	uchar *p;

	guessgeometry();

	partend -= partend%ap.blocksize;

	memset(dbuf, 0, sizeof dbuf);
	packarenapart(&ap, dbuf);
	p = pagein(PartBlank, Block);
	if(memcmp(p, dbuf, HeadSize) != 0){
		print("on-disk arena part superblock incorrect\n");
		showdiffs(dbuf, p, HeadSize, partinfo);
	}
	memmove(p, dbuf, HeadSize);

	narena = (partend-ap.arenabase + arenasize-1)/arenasize;
	if(range == nil){
		for(i=0; i<narena; i++)
			checkarena(ap.arenabase+(vlong)i*arenasize, i);
	}else if(strcmp(range, "none") == 0){
		/* nothing */
	}else{
		/* parse, e.g., -4,8-9,10- */
		for(s=range; *s; s=t){
			t = strchr(s, ',');
			if(t)
				*t++ = 0;
			else
				t = s+strlen(s);
			if(*s == '-')
				lo = 0;
			else
				lo = strtol(s, &s, 0);
			hi = lo;
			if(*s == '-'){
				s++;
				if(*s == 0)
					hi = narena-1;
				else
					hi = strtol(s, &s, 0);
			}
			if(*s != 0){
				print("bad arena range: %s\n", s);
				continue;
			}
			for(i=lo; i<=hi; i++)
				checkarena(ap.arenabase+(vlong)i*arenasize, i);
		}
	}
}

/*
 * Is there a clump here at p?
 */
static int
isclump(uchar *p, Clump *cl, u32int *pmagic)
{
	int n;
	u32int magic;
	uchar score[VtScoreSize], *bp;
	Unwhack uw;
	uchar ubuf[70*1024];

	bp = p;
	magic = u32(p);
	if(magic == 0)
		return 0;
	p += U32Size;

	cl->info.type = vtfromdisktype(*p);
	if(cl->info.type == 0xFF)
		return 0;
	p++;
	cl->info.size = u16(p);
	p += U16Size;
	cl->info.uncsize = u16(p);
	if(cl->info.size > cl->info.uncsize)
		return 0;
	p += U16Size;
	scorecp(cl->info.score, p);
	p += VtScoreSize;
	cl->encoding = *p;
	p++;
	cl->creator = u32(p);
	p += U32Size;
	cl->time = u32(p);
	p += U32Size;

	switch(cl->encoding){
	case ClumpENone:
		if(cl->info.size != cl->info.uncsize)
			return 0;
		scoremem(score, p, cl->info.size);
		if(scorecmp(score, cl->info.score) != 0)
			return 0;
		break;
	case ClumpECompress:
		if(cl->info.size >= cl->info.uncsize)
			return 0;
		unwhackinit(&uw);
		n = unwhack(&uw, ubuf, cl->info.uncsize, p, cl->info.size);
		if(n != cl->info.uncsize)
			return 0;
		scoremem(score, ubuf, cl->info.uncsize);
		if(scorecmp(score, cl->info.score) != 0)
			return 0;
		break;
	default:
		return 0;
	}
	p += cl->info.size;

	/* it all worked out in the end */
	*pmagic = magic;
	return p - bp;
}

/*
 * All ClumpInfos seen in this arena.
 * Kept in binary tree so we can look up by score.
 */
typedef struct Cit Cit;
struct Cit
{
	int left;
	int right;
	vlong corrupt;
	ClumpInfo ci;
};
Cit *cibuf;
int ciroot;
int ncibuf, mcibuf;

void
resetcibuf(void)
{
	ncibuf = 0;
	ciroot = -1;
}

int*
ltreewalk(int *p, uchar *score)
{
	int i;

	for(;;){
		if(*p == -1)
			return p;
		i = scorecmp(cibuf[*p].ci.score, score);
		if(i == 0)
			return p;
		if(i < 0)
			p = &cibuf[*p].right;
		else
			p = &cibuf[*p].left;
	}
}

void
addcibuf(ClumpInfo *ci, vlong corrupt)
{
	Cit *cit;

	if(ncibuf == mcibuf){
		mcibuf += 131072;
		cibuf = vtrealloc(cibuf, mcibuf*sizeof cibuf[0]);
	}
	cit = &cibuf[ncibuf];
	cit->ci = *ci;
	cit->left = -1;
	cit->right = -1;
	cit->corrupt = corrupt;
	if(!corrupt)
		*ltreewalk(&ciroot, ci->score) = ncibuf;
	ncibuf++;
}

void
addcicorrupt(vlong len)
{
	static ClumpInfo zci;

	addcibuf(&zci, len);
}

int
haveclump(uchar *score)
{
	int i;
	int p;

	p = ciroot;
	for(;;){
		if(p == -1)
			return 0;
		i = scorecmp(cibuf[p].ci.score, score);
		if(i == 0)
			return 1;
		if(i < 0)
			p = cibuf[p].right;
		else
			p = cibuf[p].left;
	}
}

int
matchci(ClumpInfo *ci, uchar *p)
{
	if(ci->type != vtfromdisktype(p[0]))
		return 0;
	if(ci->size != u16(p+1))
		return 0;
	if(ci->uncsize != u16(p+3))
		return 0;
	if(scorecmp(ci->score, p+5) != 0)
		return 0;
	return 1;
}

int
sealedarena(uchar *p, int blocksize)
{
	int v, n;

	v = u32(p+4);
	switch(v){
	default:
		return 0;
	case ArenaVersion4:
		n = ArenaSize4;
		break;
	case ArenaVersion5:
		n = ArenaSize5;
		break;
	}
	if(p[n-1] != 1){
		print("arena tail says not sealed\n");
		return 0;
	}
	if(memcmp(p+n, zero, blocksize-VtScoreSize-n) != 0){
		print("arena tail followed by non-zero data\n");
		return 0;
	}
	if(memcmp(p+blocksize-VtScoreSize, zero, VtScoreSize) == 0){
		print("arena score zero\n");
		return 0;
	}
	return 1;
}

int
okayname(char *name, int n)
{
	char buf[20];

	if(nameok(name) < 0)
		return 0;
	sprint(buf, "%d", n);
	if(n == 0)
		buf[0] = 0;
	if(strlen(name) < strlen(buf)
	|| strcmp(name+strlen(name)-strlen(buf), buf) != 0)
		return 0;
	return 1;
}

int
clumpinfocmp(ClumpInfo *a, ClumpInfo *b)
{
	if(a->type != b->type)
		return a->type - b->type;
	if(a->size != b->size)
		return a->size - b->size;
	if(a->uncsize != b->uncsize)
		return a->uncsize - b->uncsize;
	return scorecmp(a->score, b->score);
}

ClumpInfo*
loadci(vlong offset, Arena *arena, int nci)
{
	int i, j, per;
	uchar *p, *sp;
	ClumpInfo *bci, *ci;

	per = arena->blocksize/ClumpInfoSize;
	bci = vtmalloc(nci*sizeof bci[0]);
	ci = bci;
	offset += arena->size - arena->blocksize;
	p = sp = nil;
	for(i=0; i<nci; i+=per){
		if(p == sp){
			sp = pagein(offset-4*M, 4*M);
			p = sp+4*M;
		}
		p -= arena->blocksize;
		offset -= arena->blocksize;
		for(j=0; j<per && i+j<nci; j++)
			unpackclumpinfo(ci++, p+j*ClumpInfoSize);
	}
	return bci;
}

vlong
writeci(vlong offset, Arena *arena, ClumpInfo *ci, int nci)
{
	int i, j, per;
	uchar *p, *sp;

	per = arena->blocksize/ClumpInfoSize;
	offset += arena->size - arena->blocksize;
	p = sp = nil;
	for(i=0; i<nci; i+=per){
		if(p == sp){
			sp = pagein(offset-4*M, 4*M);
			p = sp+4*M;
		}
		p -= arena->blocksize;
		offset -= arena->blocksize;
		memset(p, 0, arena->blocksize);
		for(j=0; j<per && i+j<nci; j++)
			packclumpinfo(ci++, p+j*ClumpInfoSize);
	}
	pageout();
	return offset;
}

void
loadarenabasics(vlong offset0, int anum, ArenaHead *head, Arena *arena)
{
	char dname[ANameSize];
	static char lastbase[ANameSize];
	uchar *p;
	Arena oarena;
	ArenaHead ohead;

	/*
	 * Fmtarenas makes all arenas the same size
	 * except the last, which may be smaller.
	 * It uses the same block size for arenas as for
	 * the arena partition blocks.
	 */
	arena->size = arenasize;
	if(offset0+arena->size > partend)
		arena->size = partend - offset0;
	head->size = arena->size;

	arena->blocksize = ap.blocksize;
	head->blocksize = arena->blocksize;

	/*
	 * Look for clump magic and name in head/tail blocks.
	 * All the other info we will reconstruct just in case.
	 */
	p = pagein(offset0, arena->blocksize);
	memset(&ohead, 0, sizeof ohead);
	if(unpackarenahead(&ohead, p) >= 0){
		head->version = ohead.version;
		head->clumpmagic = ohead.clumpmagic;
		if(okayname(ohead.name, anum))
			strcpy(head->name, ohead.name);
	}

	p = pagein(offset0+arena->size-arena->blocksize,
		arena->blocksize);
	memset(&oarena, 0, sizeof oarena);
	if(unpackarena(&oarena, p) >= 0){
		arena->version = oarena.version;
		arena->clumpmagic = oarena.clumpmagic;
		if(okayname(oarena.name, anum))
			strcpy(arena->name, oarena.name);
		arena->diskstats.clumps = oarena.diskstats.clumps;
print("old arena: sealed=%d\n", oarena.diskstats.sealed);
		arena->diskstats.sealed = oarena.diskstats.sealed;
	}

	/* Head trumps arena. */
	if(head->version){
		arena->version = head->version;
		arena->clumpmagic = head->clumpmagic;
	}
	if(arena->version == 0)
		arena->version = ArenaVersion5;
	if(basename){
		if(anum == -1)
			snprint(arena->name, ANameSize, "%s", basename);
		else
			snprint(arena->name, ANameSize, "%s%d", basename, anum);
	}else if(lastbase[0])
		snprint(arena->name, ANameSize, "%s%d", lastbase, anum);
	else if(head->name[0])
		strcpy(arena->name, head->name);
	else if(arena->name[0] == 0)
		sysfatal("cannot determine base name for arena; use -n");
	strcpy(lastbase, arena->name);
	sprint(dname, "%d", anum);
	lastbase[strlen(lastbase)-strlen(dname)] = 0;

	/* Was working in arena, now copy to head. */
	head->version = arena->version;
	memmove(head->name, arena->name, sizeof head->name);
	head->blocksize = arena->blocksize;
	head->size = arena->size;
}

void
shahead(Shabuf *sb, vlong offset0, ArenaHead *head)
{
	uchar headbuf[MaxDiskBlock];

	sb->offset = offset0;
	memset(headbuf, 0, sizeof headbuf);
	packarenahead(head, headbuf);
	sbupdate(sb, headbuf, offset0, head->blocksize);
}

u32int
newclumpmagic(int version)
{
	u32int m;

	if(version == ArenaVersion4)
		return _ClumpMagic;
	do{
		m = fastrand();
	}while(m==0 || m == _ClumpMagic);
	return m;
}

/*
 * Poke around in the arena to find the clump data
 * and compute the relevant statistics.
 */
void
guessarena(vlong offset0, int anum, ArenaHead *head, Arena *arena,
	uchar *oldscore, uchar *score)
{
	uchar dbuf[MaxDiskBlock];
	int needtozero, clumps, nb1, nb2, minclumps;
	int inbad, n, ncib, printed, sealing, smart;
	u32int magic;
	uchar *sp, *ep, *p;
	vlong boffset, eoffset, lastclumpend, leaked;
	vlong offset, toffset, totalcorrupt, v;
	Clump cl;
	ClumpInfo *bci, *ci, *eci, *xci;
	Cit *bcit, *cit, *ecit;
	Shabuf oldsha, newsha;

	/*
	 * We expect to find an arena, with data, between offset
	 * and offset+arenasize.  With any luck, the data starts at
	 * offset+ap.blocksize.  The blocks have variable size and
	 * aren't padded at all, which doesn't give us any alignment
	 * constraints.  The blocks are compressed or high entropy,
	 * but the headers are pretty low entropy (except the score):
	 *
	 *	type[1] (range 0 thru 9, 13)
	 *	size[2]
	 *	uncsize[2] (<= size)
	 *
	 * so we can look for these.  We check the scores as we go,
	 * so we can't make any wrong turns.  If we find ourselves
	 * in a dead end, scan forward looking for a new start.
	 */

	resetcibuf();
	memset(head, 0, sizeof *head);
	memset(arena, 0, sizeof *arena);
	memset(oldscore, 0, VtScoreSize);
	memset(score, 0, VtScoreSize);
	memset(&oldsha, 0, sizeof oldsha);
	memset(&newsha, 0, sizeof newsha);
	newsha.rollback = 1;

	if(0){
		sbdebug(&oldsha, "old.sha");
		sbdebug(&newsha, "new.sha");
	}

	loadarenabasics(offset0, anum, head, arena);

	/* start the clump hunt */

	clumps = 0;
	totalcorrupt = 0;
	sealing = 1;
	boffset = offset0 + arena->blocksize;
	offset = boffset;
	eoffset = offset0+arena->size - arena->blocksize;
	toffset = eoffset;
	sp = pagein(offset0, 4*M);

	if(arena->diskstats.sealed){
		oldsha.offset = offset0;
		sbupdate(&oldsha, sp, offset0, 4*M);
	}
	ep = sp+4*M;
	p = sp + (boffset - offset0);
	ncib = arena->blocksize / ClumpInfoSize;	/* ci per block in index */
	lastclumpend = offset;
	nbad = 0;
	inbad = 0;
	needtozero = 0;
	minclumps = 0;
	while(offset < eoffset){
		/*
		 * Shift buffer if we're running out of room.
		 */
		if(p+70*K >= ep){
			/*
			 * Start the post SHA1 buffer.   By now we should know the
			 * clumpmagic and arena version, so we can create a
			 * correct head block to get things going.
			 */
			if(sealing && fix && newsha.offset == 0){
				newsha.offset = offset0;
				if(arena->clumpmagic == 0){
					if(arena->version == 0)
						arena->version = ArenaVersion5;
					arena->clumpmagic = newclumpmagic(arena->version);
				}
				head->clumpmagic = arena->clumpmagic;
				shahead(&newsha, offset0, head);
			}
			n = 4*M-256*K;
			if(sealing && fix){
				sbdiskhash(&newsha, bufoffset);
				sbupdate(&newsha, buf, bufoffset, 4*M-256*K);
			}
			pagein(bufoffset+n, 4*M);
			p -= n;
			if(arena->diskstats.sealed)
				sbupdate(&oldsha, buf, bufoffset, 4*M);
		}

		/*
		 * Check for a clump at p, which is at offset in the disk.
		 * Duplicate clumps happen in corrupted disks
		 * (the same pattern gets written many times in a row)
		 * and should never happen during regular use.
		 */
		magic = 0;
		if((n = isclump(p, &cl, &magic)) > 0){
			/*
			 * If we were in the middle of some corrupted data,
			 * flush a warning about it and then add any clump
			 * info blocks as necessary.
			 */
			if(inbad){
				inbad = 0;
				v = offset-lastclumpend;
				if(needtozero){
					zerorange(lastclumpend, v);
					sbrollback(&newsha, lastclumpend);
					print("corrupt clump data - %#llux+%#llux (%,llud bytes)\n",
						lastclumpend, v, v);
				}
				addcicorrupt(v);
				totalcorrupt += v;
				nb1 = (minclumps+ncib-1)/ncib;
				minclumps += (v+ClumpSize+VtMaxLumpSize-1)/(ClumpSize+VtMaxLumpSize);
				nb2 = (minclumps+ncib-1)/ncib;
				eoffset -= (nb2-nb1)*arena->blocksize;
			}

			if(haveclump(cl.info.score))
				print("warning: duplicate clump %d %V at %#llux+%#d\n", cl.info.type, cl.info.score, offset, n);

			/*
			 * If clumps use different magic numbers, we don't care.
			 * We'll just use the first one we find and make the others
			 * follow suit.
			 */
			if(arena->clumpmagic == 0){
				print("clump type %d size %d score %V magic %x\n",
					cl.info.type, cl.info.size, cl.info.score, magic);
				arena->clumpmagic = magic;
				if(magic == _ClumpMagic)
					arena->version = ArenaVersion4;
				else
					arena->version = ArenaVersion5;
			}
			if(magic != arena->clumpmagic)
				p32(p, arena->clumpmagic);
			if(clumps == 0)
				arena->ctime = cl.time;

			/*
			 * Record the clump, update arena stats,
			 * grow clump info blocks if needed.
			 */
			if(verbose > 1)
				print("\tclump %d: %d %V at %#llux+%#ux (%d)\n",
					clumps, cl.info.type, cl.info.score, offset, n, n);
			addcibuf(&cl.info, 0);
			if(minclumps%ncib == 0)
				eoffset -= arena->blocksize;
			minclumps++;
			clumps++;
			if(cl.encoding != ClumpENone)
				arena->diskstats.cclumps++;
			arena->diskstats.uncsize += cl.info.uncsize;
			arena->wtime = cl.time;

			/*
			 * Move to next clump.
			 */
			offset += n;
			p += n;
			lastclumpend = offset;
		}else{
			/*
			 * Overwrite malformed clump data with zeros later.
			 * For now, just record whether it needs to be overwritten.
			 * Bad regions must be of size at least ClumpSize.
			 * Postponing the overwriting keeps us from writing past
			 * the end of the arena data (which might be directory data)
			 * with zeros.
			 */
			if(!inbad){
				inbad = 1;
				needtozero = 0;
				if(memcmp(p, zero, ClumpSize) != 0)
					needtozero = 1;
				p += ClumpSize;
				offset += ClumpSize;
				nbad++;
			}else{
				if(*p != 0)
					needtozero = 1;
				p++;
				offset++;
			}
		}
	}
	pageout();

	if(verbose)
		print("readable clumps: %d; min. directory entries: %d\n",
			clumps, minclumps);
	arena->diskstats.used = lastclumpend - boffset;
	leaked = eoffset - lastclumpend;
	if(verbose)
		print("used from %#llux to %#llux = %,lld (%,lld unused)\n",
			boffset, lastclumpend, arena->diskstats.used, leaked);

	/*
	 * Finish the SHA1 of the old data.
	 */
	if(arena->diskstats.sealed){
		sbdiskhash(&oldsha, toffset);
		readdisk(dbuf, toffset, arena->blocksize);
		scorecp(dbuf+arena->blocksize-VtScoreSize, zero);
		sbupdate(&oldsha, dbuf, toffset, arena->blocksize);
		sbscore(&oldsha, oldscore);
	}

	/*
	 * If we still don't know the clump magic, the arena
	 * must be empty.  It still needs a value, so make
	 * something up.
	 */
	if(arena->version == 0)
		arena->version = ArenaVersion5;
	if(arena->clumpmagic == 0){
		if(arena->version == ArenaVersion4)
			arena->clumpmagic = _ClumpMagic;
		else{
			do
				arena->clumpmagic = fastrand();
			while(arena->clumpmagic==_ClumpMagic
				||arena->clumpmagic==0);
		}
		head->clumpmagic = arena->clumpmagic;
	}

	/*
	 * Guess at number of clumpinfo blocks to load.
	 * If we guess high, it's no big deal.  If we guess low,
	 * we'll be forced into rewriting the whole directory.
	 * Still not such a big deal.
	 */
	if(clumps == 0 || arena->diskstats.used == totalcorrupt)
		goto Nocib;
	if(clumps < arena->diskstats.clumps)
		clumps = arena->diskstats.clumps;
	if(clumps < ncibuf)
		clumps = ncibuf;
	clumps += totalcorrupt/
		((arena->diskstats.used - totalcorrupt)/clumps);
	clumps += totalcorrupt/2000;
	if(clumps < minclumps)
		clumps = minclumps;
	clumps += ncib-1;
	clumps -= clumps%ncib;

	/*
	 * Can't write into the actual data.
	 */
	v = offset0 + arena->size - arena->blocksize;
	v -= (clumps+ncib-1)/ncib * arena->blocksize;
	if(v < lastclumpend){
		v = offset0 + arena->size - arena->blocksize;
		clumps = (v-lastclumpend)/arena->blocksize * ncib;
	}

	if(clumps < minclumps)
		print("cannot happen?\n");

	/*
	 * Check clumpinfo blocks against directory we created.
	 * The tricky part is handling the corrupt sections of arena.
	 * If possible, we remark just the affected directory entries
	 * rather than slide everything down.
	 *
	 * Allocate clumps+1 blocks and check that we don't need
	 * the last one at the end.
	 */
	bci = loadci(offset0, arena, clumps+1);
	eci = bci+clumps+1;
	bcit = cibuf;
	ecit = cibuf+ncibuf;

	smart = 0;	/* Somehow the smart code doesn't do corrupt clumps right. */
Again:
	nbad = 0;
	ci = bci;
	for(cit=bcit; cit<ecit && ci<eci; cit++){
		if(cit->corrupt){
			vlong n, m;
			if(smart){
				/*
				 * If we can, just mark existing entries as corrupt.
				 */
				n = cit->corrupt;
				for(xci=ci; n>0 && xci<eci; xci++)
					n -= ClumpSize+xci->size;
				if(n > 0 || xci >= eci)
					goto Dumb;
				printed = 0;
				for(; ci<xci; ci++){
					if(verbose && ci->type != VtCorruptType){
						if(!printed){
							print("marking directory %d-%d as corrupt\n",
								(int)(ci-bci), (int)(xci-bci));
							printed = 1;
						}
						print("\ttype=%d size=%d uncsize=%d score=%V\n",
							ci->type, ci->size, ci->uncsize, ci->score);
					}
					ci->type = VtCorruptType;
				}
			}else{
			Dumb:
				print("\trewriting clump directory\n");
				/*
				 * Otherwise, blaze a new trail.
				 */
				n = cit->corrupt;
				while(n > 0 && ci < eci){
					if(n < ClumpSize)
						sysfatal("bad math in clump corrupt");
					if(n <= VtMaxLumpSize+ClumpSize)
						m = n;
					else{
						m = VtMaxLumpSize+ClumpSize;
						if(n-m < ClumpSize)
							m -= ClumpSize;
					}
					ci->type = VtCorruptType;
					ci->size = m-ClumpSize;
					ci->uncsize = m-ClumpSize;
					memset(ci->score, 0, VtScoreSize);
					ci++;
					n -= m;
				}
			}
			continue;
		}
		if(clumpinfocmp(&cit->ci, ci) != 0){
			if(verbose && (smart || verbose>1)){
				print("clumpinfo %d\n", (int)(ci-bci));
				print("\twant: %d %d %d %V\n",
					cit->ci.type, cit->ci.size,
					cit->ci.uncsize, cit->ci.score);
				print("\thave: %d %d %d %V\n",
					ci->type, ci->size,
					ci->uncsize, ci->score);
			}
			*ci = cit->ci;
			nbad++;
		}
		ci++;
	}
	if(ci >= eci || cit < ecit){
		print("ran out of space editing existing directory; rewriting\n");
		print("# eci %ld ci %ld ecit %ld cit %ld\n", eci-bci, ci-bci, ecit-bcit, cit-bcit);
		assert(smart);	/* can't happen second time thru */
		smart = 0;
		goto Again;
	}

	assert(ci <= eci);
	arena->diskstats.clumps = ci-bci;
	eoffset = writeci(offset0, arena, bci, ci-bci);
	if(sealing && fix)
		sbrollback(&newsha, v);
print("eoffset=%lld lastclumpend=%lld diff=%lld unseal=%d\n", eoffset, lastclumpend, eoffset-lastclumpend, unseal);
	if(lastclumpend > eoffset)
		print("arena directory overwrote blocks!  cannot happen!\n");
	free(bci);
	if(smart && nbad)
		print("arena directory has %d bad or missing entries\n", nbad);
Nocib:
	if(eoffset - lastclumpend > 64*1024 && (!arena->diskstats.sealed || unseal)){
		if(arena->diskstats.sealed)
			print("unsealing arena\n");
		sealing = 0;
		memset(oldscore, 0, VtScoreSize);
	}

	/*
	 * Finish the SHA1 of the new data - only meaningful
	 * if we've been writing to disk (`fix').
	 */
	arena->diskstats.sealed = sealing;
	arena->memstats = arena->diskstats;
	if(sealing && fix){
		uchar tbuf[MaxDiskBlock];

		sbdiskhash(&newsha, toffset);
		memset(tbuf, 0, sizeof tbuf);
		packarena(arena, tbuf);
		sbupdate(&newsha, tbuf, toffset, arena->blocksize);
		sbscore(&newsha, score);
	}
}

void
dumparena(vlong offset, int anum, Arena *arena)
{
	char buf[1000];
	vlong o, e;
	int fd, n;

	snprint(buf, sizeof buf, "%s.%d", dumpbase, anum);
	if((fd = create(buf, OWRITE, 0666)) < 0){
		fprint(2, "create %s: %r\n", buf);
		return;
	}
	e = offset+arena->size;
	for(o=offset; o<e; o+=n){
		n = 4*M;
		if(o+n > e)
			n = e-o;
		if(pwrite(fd, pagein(o, n), n, o-offset) != n){
			fprint(2, "write %s at %#llux: %r\n", buf, o-offset);
			return;
		}
	}
}

void
checkarena(vlong offset, int anum)
{
	uchar dbuf[MaxDiskBlock];
	uchar *p, oldscore[VtScoreSize], score[VtScoreSize];
	Arena arena, oarena;
	ArenaHead head;
	Info *fmt, *fmta;
	int sz;

	print("# arena %d: offset %#llux\n", anum, offset);

	if(offset >= partend){
		print("arena offset out of bounds\n");
		return;
	}

	guessarena(offset, anum, &head, &arena, oldscore, score);

	if(verbose){
		print("#\tversion=%d name=%s blocksize=%d size=%z",
			head.version, head.name, head.blocksize, head.size);
		if(head.clumpmagic)
			print(" clumpmagic=%#.8ux", head.clumpmagic);
		print("\n#\tclumps=%d cclumps=%d used=%,lld uncsize=%,lld\n",
			arena.diskstats.clumps, arena.diskstats.cclumps,
			arena.diskstats.used, arena.diskstats.uncsize);
		print("#\tctime=%t\n", arena.ctime);
		print("#\twtime=%t\n", arena.wtime);
		if(arena.diskstats.sealed)
			print("#\tsealed score=%V\n", score);
	}

	if(dumpbase){
		dumparena(offset, anum, &arena);
		return;
	}

	memset(dbuf, 0, sizeof dbuf);
	packarenahead(&head, dbuf);
	p = pagein(offset, arena.blocksize);
	if(memcmp(dbuf, p, arena.blocksize) != 0){
		print("on-disk arena header incorrect\n");
		showdiffs(dbuf, p, arena.blocksize,
			arena.version==ArenaVersion4 ? headinfo4 : headinfo5);
	}
	memmove(p, dbuf, arena.blocksize);

	memset(dbuf, 0, sizeof dbuf);
	packarena(&arena, dbuf);
	if(arena.diskstats.sealed)
		scorecp(dbuf+arena.blocksize-VtScoreSize, score);
	p = pagein(offset+arena.size-arena.blocksize, arena.blocksize);
	memset(&oarena, 0, sizeof oarena);
	unpackarena(&oarena, p);
	if(arena.version == ArenaVersion4){
		sz = ArenaSize4;
		fmt = tailinfo4;
		fmta = tailinfo4a;
	}else{
		sz = ArenaSize5;
		fmt = tailinfo5;
		fmta = tailinfo5a;
	}
	if(p[sz] == 1){
		fmt = fmta;
		if(oarena.diskstats.sealed){
			/*
			 * some arenas were sealed with the extension
			 * before we adopted the convention that if it didn't
			 * add new information it gets dropped.
			 */
			_packarena(&arena, dbuf, 1);
		}
	}
	if(memcmp(dbuf, p, arena.blocksize-VtScoreSize) != 0){
		print("on-disk arena tail incorrect\n");
		showdiffs(dbuf, p, arena.blocksize-VtScoreSize, fmt);
	}
	if(arena.diskstats.sealed){
		if(oarena.diskstats.sealed)
		if(scorecmp(p+arena.blocksize-VtScoreSize, oldscore) != 0){
			print("on-disk arena seal score incorrect\n");
			print("\tcorrect=%V\n", oldscore);
			print("\t   disk=%V\n", p+arena.blocksize-VtScoreSize);
		}
		if(fix && scorecmp(p+arena.blocksize-VtScoreSize, score) != 0){
			print("%ssealing arena%s: %V\n",
				oarena.diskstats.sealed ? "re" : "",
				scorecmp(oldscore, score) == 0 ?
					"" : " after changes", score);
		}
	}
	memmove(p, dbuf, arena.blocksize);

	pageout();
}

AMapN*
buildamap(void)
{
	uchar *p;
	vlong o;
	ArenaHead h;
	AMapN *an;
	AMap *m;

	an = vtmallocz(sizeof *an);
	for(o=ap.arenabase; o<partend; o+=arenasize){
		p = pagein(o, Block);
		if(unpackarenahead(&h, p) >= 0){
			an->map = vtrealloc(an->map, (an->n+1)*sizeof an->map[0]);
			m = &an->map[an->n++];
			m->start = o;
			m->stop = o+h.size;
			strcpy(m->name, h.name);
		}
	}
	return an;
}

void
checkmap(void)
{
	char *s;
	uchar *p;
	int i, len;
	AMapN *an;
	Fmt fmt;

	an = buildamap();
	fmtstrinit(&fmt);
	fmtprint(&fmt, "%ud\n", an->n);
	for(i=0; i<an->n; i++)
		fmtprint(&fmt, "%s\t%lld\t%lld\n",
			an->map[i].name, an->map[i].start, an->map[i].stop);
	s = fmtstrflush(&fmt);
	len = strlen(s);
	if(len > ap.tabsize){
		print("arena partition map too long: need %z bytes have %z\n",
			(vlong)len, (vlong)ap.tabsize);
		len = ap.tabsize;
	}

	if(ap.tabsize >= 4*M){	/* can't happen - max arenas is 2000 */
		print("arena partition map *way* too long\n");
		return;
	}

	p = pagein(ap.tabbase, ap.tabsize);
	if(memcmp(p, s, len) != 0){
		print("arena partition map incorrect; rewriting.\n");
		memmove(p, s, len);
	}
	pageout();
}

int mainstacksize = 512*1024;

void
threadmain(int argc, char **argv)
{
	int mode;

	mode = OREAD;
	readonly = 1;
	ARGBEGIN{
	case 'U':
		unseal = 1;
		break;
	case 'a':
		arenasize = unittoull(EARGF(usage()));
		break;
	case 'b':
		ap.blocksize = unittoull(EARGF(usage()));
		break;
	case 'f':
		fix = 1;
		mode = ORDWR;
		readonly = 0;
		break;
	case 'n':
		basename = EARGF(usage());
		break;
	case 'v':
		verbose++;
		break;
	case 'x':
		dumpbase = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	file = argv[0];

	ventifmtinstall();
	fmtinstall('z', zfmt);
	fmtinstall('t', tfmt);
	quotefmtinstall();

	part = initpart(file, mode|ODIRECT);
	if(part == nil)
		sysfatal("can't open %s: %r", file);
	partend = part->size;

	if(isonearena()){
		checkarena(0, -1);
		threadexitsall(nil);
	}
	checkarenas(argc > 1 ? argv[1] : nil);
	checkmap();
	threadexitsall(nil);
}
