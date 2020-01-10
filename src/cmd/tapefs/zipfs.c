#include <u.h>
#include <libc.h>
#include <bio.h>
#include <flate.h>
#include <auth.h>
#include <fcall.h>
#include <ctype.h>
#include "tapefs.h"
#include "zip.h"

#define FORCE_LOWER	1	/* force filenames to lower case */
#define MUNGE_CR	1	/* replace '\r\n' with ' \n' */
#define High64 (1LL<<63)

/*
 * File system for zip archives (read-only)
 */

enum {
	IS_MSDOS = 0,	/* creator OS (interpretation of external flags) */
	IS_RDONLY = 1,	/* file was readonly (external flags) */
	IS_TEXT = 1	/* file was text  (internal flags) */
};

typedef struct Block Block;
struct Block{
	uchar *pos;
	uchar *limit;
};

static Biobuf *bin;
static u32int *crctab;
static ulong crc;

static int findCDir(Biobuf *);
static int header(Biobuf *, ZipHead *);
static int cheader(Biobuf *, ZipHead *);
/* static void trailer(Biobuf *, ZipHead *); */
static char *getname(Biobuf *, int);
static int blwrite(void *, void *, int);
static ulong get4(Biobuf *);
static int get2(Biobuf *);
static int get1(Biobuf *);
static long msdos2time(int, int);

void
populate(char *name)
{
	char *p;
	Fileinf f;
	ZipHead zh;
	int ok, entries;

	crctab = mkcrctab(ZCrcPoly);
	ok = inflateinit();
	if(ok != FlateOk)
		sysfatal("inflateinit failed: %s", flateerr(ok));

	bin = Bopen(name, OREAD);
	if (bin == nil)
		error("Can't open argument file");

	entries = findCDir(bin);
	if(entries < 0)
		sysfatal("empty file");

	while(entries-- > 0){
		memset(&zh, 0, sizeof(zh));
		if(!cheader(bin, &zh))
			break;
		f.addr = zh.off;
		if(zh.iattr & IS_TEXT)
			f.addr |= High64;
		f.mode = (zh.madevers == IS_MSDOS && zh.eattr & IS_RDONLY)? 0444: 0644;
		if (zh.meth == 0 && zh.uncsize == 0){
			p = strchr(zh.file, '\0');
			if(p > zh.file && p[-1] == '/')
				f.mode |= (DMDIR | 0111);
		}
		f.uid = 0;
		f.gid = 0;
		f.size = zh.uncsize;
		f.mdate = msdos2time(zh.modtime, zh.moddate);
		f.name = zh.file + ((zh.file[0] == '/')? 1: 0);
		poppath(f, 1);
		free(zh.file);
	}
	return ;
}

void
dotrunc(Ram *r)
{
	USED(r);
}

void
docreate(Ram *r)
{
	USED(r);
}

char *
doread(Ram *r, vlong off, long cnt)
{
	int i, err;
	Block bs;
	ZipHead zh;
	static Qid oqid;
	static char buf[Maxbuf];
	static uchar *cache = nil;

	if (cnt > Maxbuf)
		sysfatal("file too big (>%d)", Maxbuf);

	if (Bseek(bin, r->addr & 0x7FFFFFFFFFFFFFFFLL, 0) < 0)
		sysfatal("seek failed");

	memset(&zh, 0, sizeof(zh));
	if (!header(bin, &zh))
		sysfatal("cannot get local header");

	switch(zh.meth){
	case 0:
		if (Bseek(bin, off, 1) < 0)
			sysfatal("seek failed");
		if (Bread(bin, buf, cnt) != cnt)
			sysfatal("read failed");
		break;
	case 8:
		if (r->qid.path != oqid.path){
			oqid = r->qid;
			if (cache)
				free(cache);
			cache = emalloc(r->ndata);

			bs.pos = cache;
			bs.limit = cache+r->ndata;
			if ((err = inflate(&bs, blwrite, bin, (int(*)(void*))Bgetc)) != FlateOk)
				sysfatal("inflate failed - %s", flateerr(err));

			if (blockcrc(crctab, crc, cache, r->ndata) != zh.crc)
				fprint(2, "%s - crc failed", r->name);

			if ((r->addr & High64) && MUNGE_CR){
				for (i = 0; i < r->ndata -1; i++)
					if (cache[i] == '\r' && cache[i +1] == '\n')
						cache[i] = ' ';
			}
		}
		memcpy(buf, cache+off, cnt);
		break;
	default:
		sysfatal("%d - unsupported compression method", zh.meth);
		break;
	}

	return buf;
}

void
popdir(Ram *r)
{
	USED(r);
}

void
dowrite(Ram *r, char *buf, long off, long cnt)
{
	USED(r); USED(buf); USED(off); USED(cnt);
}

int
dopermw(Ram *r)
{
	USED(r);
	return 0;
}

/*************************************************/

static int
findCDir(Biobuf *bin)
{
	vlong ecoff;
	long off;
	int entries, zclen;

	ecoff = Bseek(bin, -ZECHeadSize, 2);
	if(ecoff < 0)
		sysfatal("can't seek to header");

	if(get4(bin) != ZECHeader)
		sysfatal("bad magic number on directory");

	get2(bin);
	get2(bin);
	get2(bin);
	entries = get2(bin);
	get4(bin);
	off = get4(bin);
	zclen = get2(bin);
	while(zclen-- > 0)
		get1(bin);

	if(Bseek(bin, off, 0) != off)
		sysfatal("can't seek to contents");

	return entries;
}


static int
header(Biobuf *bin, ZipHead *zh)
{
	ulong v;
	int flen, xlen;

	v = get4(bin);
	if(v != ZHeader){
		if(v == ZCHeader)
			return 0;
		sysfatal("bad magic on local header");
	}
	zh->extvers = get1(bin);
	zh->extos = get1(bin);
	zh->flags = get2(bin);
	zh->meth = get2(bin);
	zh->modtime = get2(bin);
	zh->moddate = get2(bin);
	zh->crc = get4(bin);
	zh->csize = get4(bin);
	zh->uncsize = get4(bin);
	flen = get2(bin);
	xlen = get2(bin);

	zh->file = getname(bin, flen);

	while(xlen-- > 0)
		get1(bin);
	return 1;
}

static int
cheader(Biobuf *bin, ZipHead *zh)
{
	ulong v;
	int flen, xlen, fclen;

	v = get4(bin);
	if(v != ZCHeader){
		if(v == ZECHeader)
			return 0;
		sysfatal("bad magic number in file");
	}
	zh->madevers = get1(bin);
	zh->madeos = get1(bin);
	zh->extvers = get1(bin);
	zh->extos = get1(bin);
	zh->flags = get2(bin);
	zh->meth = get2(bin);
	zh->modtime = get2(bin);
	zh->moddate = get2(bin);
	zh->crc = get4(bin);
	zh->csize = get4(bin);
	zh->uncsize = get4(bin);
	flen = get2(bin);
	xlen = get2(bin);
	fclen = get2(bin);
	get2(bin);		/* disk number start */
	zh->iattr = get2(bin);	/* 1 == is-text-file */
	zh->eattr = get4(bin);	/* 1 == readonly-file */
	zh->off = get4(bin);

	zh->file = getname(bin, flen);

	while(xlen-- > 0)
		get1(bin);

	while(fclen-- > 0)
		get1(bin);

	return 1;
}

static int
blwrite(void *vb, void *buf, int n)
{
	Block *b = vb;
	if(n > b->limit - b->pos)
		n = b->limit - b->pos;
	memmove(b->pos, buf, n);
	b->pos += n;
	return n;
}

/*
static void
trailer(Biobuf *bin, ZipHead *zh)
{
	if(zh->flags & ZTrailInfo){
		zh->crc = get4(bin);
		zh->csize = get4(bin);
		zh->uncsize = get4(bin);
	}
}
*/

static char*
getname(Biobuf *bin, int len)
{
	char *s;
	int i, c;

	s = emalloc(len + 1);
	for(i = 0; i < len; i++){
		c = get1(bin);
		if(FORCE_LOWER)
			c = tolower(c);
		s[i] = c;
	}
	s[i] = '\0';
	return s;
}


static ulong
get4(Biobuf *b)
{
	ulong v;
	int i, c;

	v = 0;
	for(i = 0; i < 4; i++){
		c = Bgetc(b);
		if(c < 0)
			sysfatal("unexpected eof");
		v |= c << (i * 8);
	}
	return v;
}

static int
get2(Biobuf *b)
{
	int i, c, v;

	v = 0;
	for(i = 0; i < 2; i++){
		c = Bgetc(b);
		if(c < 0)
			sysfatal("unexpected eof");
		v |= c << (i * 8);
	}
	return v;
}

static int
get1(Biobuf *b)
{
	int c;

	c = Bgetc(b);
	if(c < 0)
		sysfatal("unexpected eof");
	return c;
}

static long
msdos2time(int time, int date)
{
	Tm tm;

	tm.hour = time >> 11;
	tm.min = (time >> 5) & 63;
	tm.sec = (time & 31) << 1;
	tm.year = 80 + (date >> 9);
	tm.mon = ((date >> 5) & 15) - 1;
	tm.mday = date & 31;
	tm.zone[0] = '\0';
	tm.yday = 0;

	return tm2sec(&tm);
}
