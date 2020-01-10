/*
 * Initialize a fossil file system from an ISO9660 image already in the
 * file system.  This is a fairly bizarre thing to do, but it lets us generate
 * installation CDs that double as valid Plan 9 disk partitions.
 * People having trouble booting the CD can just copy it into a disk
 * partition and you've got a working Plan 9 system.
 *
 * I've tried hard to keep all the associated cruft in this file.
 * If you deleted this file and cut out the three calls into it from flfmt.c,
 * no traces would remain.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "flfmt9660.h"
#include <bio.h>
#include <ctype.h>

static Biobuf *b;

enum{
	Tag = 0x96609660,
	Blocksize = 2048,
};

#pragma varargck type "s" uchar*
#pragma varargck type "L" uchar*
#pragma varargck type "B" uchar*
#pragma varargck type "N" uchar*
#pragma varargck type "C" uchar*
#pragma varargck type "D" uchar*

typedef struct Voldesc Voldesc;
struct Voldesc {
	uchar	magic[8];	/* 0x01, "CD001", 0x01, 0x00 */
	uchar	systemid[32];	/* system identifier */
	uchar	volumeid[32];	/* volume identifier */
	uchar	unused[8];	/* character set in secondary desc */
	uchar	volsize[8];	/* volume size */
	uchar	charset[32];
	uchar	volsetsize[4];	/* volume set size = 1 */
	uchar	volseqnum[4];	/* volume sequence number = 1 */
	uchar	blocksize[4];	/* logical block size */
	uchar	pathsize[8];	/* path table size */
	uchar	lpathloc[4];	/* Lpath */
	uchar	olpathloc[4];	/* optional Lpath */
	uchar	mpathloc[4];	/* Mpath */
	uchar	ompathloc[4];	/* optional Mpath */
	uchar	rootdir[34];	/* root directory */
	uchar	volsetid[128];	/* volume set identifier */
	uchar	publisher[128];
	uchar	prepid[128];	/* data preparer identifier */
	uchar	applid[128];	/* application identifier */
	uchar	notice[37];	/* copyright notice file */
	uchar	abstract[37];	/* abstract file */
	uchar	biblio[37];	/* bibliographic file */
	uchar	cdate[17];	/* creation date */
	uchar	mdate[17];	/* modification date */
	uchar	xdate[17];	/* expiration date */
	uchar	edate[17];	/* effective date */
	uchar	fsvers;		/* file system version = 1 */
};

typedef struct Cdir Cdir;
struct Cdir {
	uchar	len;
	uchar	xlen;
	uchar	dloc[8];
	uchar	dlen[8];
	uchar	date[7];
	uchar	flags;
	uchar	unitsize;
	uchar	gapsize;
	uchar	volseqnum[4];
	uchar	namelen;
	uchar	name[1];	/* chumminess */
};
#pragma varargck type "D" Cdir*

static int
Dfmt(Fmt *fmt)
{
	char buf[128];
	Cdir *c;

	c = va_arg(fmt->args, Cdir*);
	if(c->namelen == 1 && c->name[0] == '\0' || c->name[0] == '\001') {
		snprint(buf, sizeof buf, ".%s dloc %.4N dlen %.4N",
			c->name[0] ? "." : "", c->dloc, c->dlen);
	} else {
		snprint(buf, sizeof buf, "%.*C dloc %.4N dlen %.4N", c->namelen, c->name,
			c->dloc, c->dlen);
	}
	fmtstrcpy(fmt, buf);
	return 0;
}

static ulong
big(void *a, int n)
{
	uchar *p;
	ulong v;
	int i;

	p = a;
	v = 0;
	for(i=0; i<n; i++)
		v = (v<<8) | *p++;
	return v;
}

static ulong
little(void *a, int n)
{
	uchar *p;
	ulong v;
	int i;

	p = a;
	v = 0;
	for(i=0; i<n; i++)
		v |= (*p++<<(i*8));
	return v;
}

/* numbers in big or little endian. */
static int
BLfmt(Fmt *fmt)
{
	ulong v;
	uchar *p;
	char buf[20];

	p = va_arg(fmt->args, uchar*);

	if(!(fmt->flags&FmtPrec)) {
		fmtstrcpy(fmt, "*BL*");
		return 0;
	}

	if(fmt->r == 'B')
		v = big(p, fmt->prec);
	else
		v = little(p, fmt->prec);

	sprint(buf, "0x%.*lux", fmt->prec*2, v);
	fmt->flags &= ~FmtPrec;
	fmtstrcpy(fmt, buf);
	return 0;
}

/* numbers in both little and big endian */
static int
Nfmt(Fmt *fmt)
{
	char buf[100];
	uchar *p;

	p = va_arg(fmt->args, uchar*);

	sprint(buf, "%.*L %.*B", fmt->prec, p, fmt->prec, p+fmt->prec);
	fmt->flags &= ~FmtPrec;
	fmtstrcpy(fmt, buf);
	return 0;
}

static int
asciiTfmt(Fmt *fmt)
{
	char *p, buf[256];
	int i;

	p = va_arg(fmt->args, char*);
	for(i=0; i<fmt->prec; i++)
		buf[i] = *p++;
	buf[i] = '\0';
	for(p=buf+strlen(buf); p>buf && p[-1]==' '; p--)
		;
	p[0] = '\0';
	fmt->flags &= ~FmtPrec;
	fmtstrcpy(fmt, buf);
	return 0;
}

static void
ascii(void)
{
	fmtinstall('C', asciiTfmt);
}

static void
getsect(uchar *buf, int n)
{
	if(Bseek(b, n*2048, 0) != n*2048 || Bread(b, buf, 2048) != 2048)
{
abort();
		sysfatal("reading block at %,d: %r", n*2048);
}
}

static Header *h;
static int fd;
static char *file9660;
static int off9660;
static ulong startoff;
static ulong endoff;
static ulong fsoff;
static uchar root[2048];
static Voldesc *v;
static ulong iso9660start(Cdir*);
static void iso9660copydir(Fs*, File*, Cdir*);
static void iso9660copyfile(Fs*, File*, Cdir*);

void
iso9660init(int xfd, Header *xh, char *xfile9660, int xoff9660)
{
	uchar sect[2048], sect2[2048];

	fmtinstall('L', BLfmt);
	fmtinstall('B', BLfmt);
	fmtinstall('N', Nfmt);
	fmtinstall('D', Dfmt);

	fd = xfd;
	h = xh;
	file9660 = xfile9660;
	off9660 = xoff9660;

	if((b = Bopen(file9660, OREAD)) == nil)
		sysfatal("Bopen %s: %r", file9660);

	getsect(root, 16);
	ascii();

	v = (Voldesc*)root;
	if(memcmp(v->magic, "\001CD001\001\000", 8) != 0)
		sysfatal("%s not a cd image", file9660);

	startoff = iso9660start((Cdir*)v->rootdir)*Blocksize;
	endoff = little(v->volsize, 4);	/* already in bytes */

	fsoff = off9660 + h->data*h->blockSize;
	if(fsoff > startoff)
		sysfatal("fossil data starts after cd data");
	if(off9660 + (vlong)h->end*h->blockSize < endoff)
		sysfatal("fossil data ends before cd data");
	if(fsoff%h->blockSize)
		sysfatal("cd offset not a multiple of fossil block size");

	/* Read "same" block via CD image and via Fossil image */
	getsect(sect, startoff/Blocksize);
	if(seek(fd, startoff-off9660, 0) < 0)
		sysfatal("cannot seek to first data sector on cd via fossil");
fprint(2, "look for %lud at %lud\n", startoff, startoff-off9660);
	if(readn(fd, sect2, Blocksize) != Blocksize)
		sysfatal("cannot read first data sector on cd via fossil");
	if(memcmp(sect, sect2, Blocksize) != 0)
		sysfatal("iso9660 offset is a lie");
}

void
iso9660labels(Disk *disk, uchar *buf, void (*write)(int, u32int))
{
	ulong sb, eb, bn, lb, llb;
	Label l;
	int lpb;
	uchar sect[Blocksize];

	if(!diskReadRaw(disk, PartData, (startoff-fsoff)/h->blockSize, buf))
		sysfatal("disk read failed: %r");
	getsect(sect, startoff/Blocksize);
	if(memcmp(buf, sect, Blocksize) != 0)
		sysfatal("fsoff is wrong");

	sb = (startoff-fsoff)/h->blockSize;
	eb = (endoff-fsoff+h->blockSize-1)/h->blockSize;

	lpb = h->blockSize/LabelSize;

	/* for each reserved block, mark label */
	llb = ~0;
	l.type = BtData;
	l.state = BsAlloc;
	l.tag = Tag;
	l.epoch = 1;
	l.epochClose = ~(u32int)0;
	for(bn=sb; bn<eb; bn++){
		lb = bn/lpb;
		if(lb != llb){
			if(llb != ~0)
				(*write)(PartLabel, llb);
			memset(buf, 0, h->blockSize);
		}
		llb = lb;
		labelPack(&l, buf, bn%lpb);
	}
	if(llb != ~0)
		(*write)(PartLabel, llb);
}

void
iso9660copy(Fs *fs)
{
	File *root;

	root = fileOpen(fs, "/active");
	iso9660copydir(fs, root, (Cdir*)v->rootdir);
	fileDecRef(root);
	runlock(&fs->elk);
	if(!fsSnapshot(fs, nil, nil, 0))
		sysfatal("snapshot failed: %r");
	rlock(&fs->elk);
}

/*
 * The first block used is the first data block of the leftmost file in the tree.
 * (Just an artifact of how mk9660 works.)
 */
static ulong
iso9660start(Cdir *c)
{
	uchar sect[Blocksize];

	while(c->flags&2){
		getsect(sect, little(c->dloc, 4));
		c = (Cdir*)sect;
		c = (Cdir*)((uchar*)c+c->len);	/* skip dot */
		c = (Cdir*)((uchar*)c+c->len);	/* skip dotdot */
		/* oops: might happen if leftmost directory is empty or leftmost file is zero length! */
		if(little(c->dloc, 4) == 0)
			sysfatal("error parsing cd image or unfortunate cd image");
	}
	return little(c->dloc, 4);
}

static void
iso9660copydir(Fs *fs, File *dir, Cdir *cd)
{
	ulong off, end, len;
	uchar sect[Blocksize], *esect, *p;
	Cdir *c;

	len = little(cd->dlen, 4);
	off = little(cd->dloc, 4)*Blocksize;
	end = off+len;
	esect = sect+Blocksize;

	for(; off<end; off+=Blocksize){
		getsect(sect, off/Blocksize);
		p = sect;
		while(p < esect){
			c = (Cdir*)p;
			if(c->len <= 0)
				break;
			if(c->namelen!=1 || c->name[0]>1)
				iso9660copyfile(fs, dir, c);
			p += c->len;
		}
	}
}

static char*
getname(uchar **pp)
{
	uchar *p;
	int l;

	p = *pp;
	l = *p;
	*pp = p+1+l;
	if(l == 0)
		return "";
	memmove(p, p+1, l);
	p[l] = 0;
	return (char*)p;
}

static char*
getcname(Cdir *c)
{
	uchar *up;
	char *p, *q;

	up = &c->namelen;
	p = getname(&up);
	for(q=p; *q; q++)
		*q = tolower(*q);
	return p;
}

static char
dmsize[12] =
{
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

static ulong
getcdate(uchar *p)	/* yMdhmsz */
{
	Tm tm;
	int y, M, d, h, m, s, tz;

	y=p[0]; M=p[1]; d=p[2];
	h=p[3]; m=p[4]; s=p[5]; tz=p[6];
	USED(tz);
	if (y < 70)
		return 0;
	if (M < 1 || M > 12)
		return 0;
	if (d < 1 || d > dmsize[M-1])
		return 0;
	if (h > 23)
		return 0;
	if (m > 59)
		return 0;
	if (s > 59)
		return 0;

	memset(&tm, 0, sizeof tm);
	tm.sec = s;
	tm.min = m;
	tm.hour = h;
	tm.mday = d;
	tm.mon = M-1;
	tm.year = 1900+y;
	tm.zone[0] = 0;
	return tm2sec(&tm);
}

static int ind;

static void
iso9660copyfile(Fs *fs, File *dir, Cdir *c)
{
	Dir d;
	DirEntry de;
	int sysl;
	uchar score[VtScoreSize];
	ulong off, foff, len, mode;
	uchar *p;
	File *f;

	ind++;
	memset(&d, 0, sizeof d);
	p = c->name + c->namelen;
	if(((uintptr)p) & 1)
		p++;
	sysl = (uchar*)c + c->len - p;
	if(sysl <= 0)
		sysfatal("missing plan9 directory entry on %d/%d/%.*s", c->namelen, c->name[0], c->namelen, c->name);
	d.name = getname(&p);
	d.uid = getname(&p);
	d.gid = getname(&p);
	if((uintptr)p & 1)
		p++;
	d.mode = little(p, 4);
	if(d.name[0] == 0)
		d.name = getcname(c);
	d.mtime = getcdate(c->date);
	d.atime = d.mtime;

if(d.mode&DMDIR)	print("%*scopy %s %s %s %luo\n", ind*2, "", d.name, d.uid, d.gid, d.mode);

	mode = d.mode&0777;
	if(d.mode&DMDIR)
		mode |= ModeDir;
	if((f = fileCreate(dir, d.name, mode, d.uid)) == nil)
		sysfatal("could not create file '%s': %r", d.name);
	if(d.mode&DMDIR)
		iso9660copydir(fs, f, c);
	else{
		len = little(c->dlen, 4);
		off = little(c->dloc, 4)*Blocksize;
		for(foff=0; foff<len; foff+=h->blockSize){
			localToGlobal((off+foff-fsoff)/h->blockSize, score);
			if(!fileMapBlock(f, foff/h->blockSize, score, Tag))
				sysfatal("fileMapBlock: %r");
		}
		if(!fileSetSize(f, len))
			sysfatal("fileSetSize: %r");
	}
	if(!fileGetDir(f, &de))
		sysfatal("fileGetDir: %r");
	de.uid = d.uid;
	de.gid = d.gid;
	de.mtime = d.mtime;
	de.atime = d.atime;
	de.mode = d.mode&0777;
	if(!fileSetDir(f, &de, "sys"))
		sysfatal("fileSetDir: %r");
	fileDecRef(f);
	ind--;
}
