#include <u.h>
#include <sys/stat.h>
#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

int mainstacksize = 128*1024;

typedef struct Sink Sink;
typedef struct MetaSink MetaSink;
typedef struct DirSink DirSink;

struct Sink {
	VtConn *z;
	VtEntry dir;
	uchar *buf;
	uchar *pbuf[VtPointerDepth+1];
};

struct DirSink {
	Sink *sink;
	MetaSink *msink;
	ulong nentry;
	uchar *buf;
	uchar *p;	/* current pointer */
	uchar *ep;	/* end pointer */
};

struct MetaSink {
	Sink *sink;
	uchar *buf;
	int maxindex;
	int nindex;
	uchar *rp;	/* start of current record */
	uchar *p;	/* current pointer */
	uchar *ep;	/* end pointer */
};

static void usage(void);
static int strpcmp(const void*, const void*);
static void warn(char *fmt, ...);
static void cleanup(void);
static u64int unittoull(char *s);
static void vac(VtConn *z, char *argv[]);
static void vacfile(DirSink *dsink, char *lname, char *sname, VacFile*);
static void vacstdin(DirSink *dsink, char *name, VacFile *vf);
static void vacdata(DirSink *dsink, int fd, char *lname, VacFile*, Dir*);
static void vacdir(DirSink *dsink, int fd, char *lname, char *sname, VacFile*);
static int vacmerge(DirSink *dsink, char *lname, char *sname);

Sink *sinkalloc(VtConn *z, int psize, int dsize);
void sinkwrite(Sink *k, uchar *data, int n);
void sinkwritescore(Sink *k, uchar *score, int n);
void sinkclose(Sink *k);
void sinkfree(Sink *k);

DirSink *dirsinkalloc(VtConn *z, int psize, int dsize);
void dirsinkwrite(DirSink *k, VtEntry*);
void dirsinkwritesink(DirSink *k, Sink*);
int dirsinkwritefile(DirSink *k, VacFile *vf);
void dirsinkclose(DirSink *k);
void dirsinkfree(DirSink *k);

MetaSink *metasinkalloc(VtConn *z, int psize, int dsize);
void metasinkputc(MetaSink *k, int c);
void metasinkputstring(MetaSink *k, char *s);
void metasinkputuint32(MetaSink *k, ulong x);
void metasinkputuint64(MetaSink *k, uvlong x);
void metasinkwrite(MetaSink *k, uchar *data, int n);
void metasinkwritedir(MetaSink *ms, VacDir *vd);
void metasinkeor(MetaSink *k);
void metasinkclose(MetaSink *k);
void metasinkfree(MetaSink *k);
void plan9tovacdir(VacDir*, Dir*, ulong entry, uvlong qid);

enum {
	Version = 8,
	BlockSize = 8*1024,
	MaxExclude = 1000,
};

struct {
	ulong	file;
	ulong	sfile;
	ulong	data;
	ulong	sdata;
	ulong	skip;
	ulong	meta;
} stats;

int bsize = BlockSize;
int maxbsize;
char *oname, *dfile;
int verbose;
uvlong fileid = 1;
int qdiff;
char *exclude[MaxExclude];
int nexclude;
int nowrite;
int merge;
char *isi;

static void
usage(void)
{
	fprint(2, "usage: %s [-amqsv] [-h host] [-d vacfile] [-b blocksize] [-i name] [-e exclude] [-f vacfile] file ... \n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	VtConn *z;
	char *p;
	char *host = nil;
	int statsflag = 0;

	atexit(cleanup);

	ARGBEGIN{
	default:
		usage();
	case 'b':
		p = ARGF();
		if(p == 0)
			usage();
		bsize = unittoull(p);
		if(bsize == ~0)
			usage();
		break;
	case 'd':
		dfile = ARGF();
		if(dfile == nil)
			usage();
		break;
	case 'e':
		if(nexclude >= MaxExclude)
			sysfatal("too many exclusions\n");
		exclude[nexclude] = ARGF();
		if(exclude[nexclude] == nil)
			usage();
		nexclude++;
		break;
	case 'f':
		oname = ARGF();
		if(oname == 0)
			usage();
		break;
	case 'h':
		host = ARGF();
		if(host == nil)
			usage();
		break;
	case 'i':
		isi = ARGF();
		if(isi == nil)
			usage();
		break;
	case 'n':
		nowrite++;
		break;
	case 'm':
		merge++;
		break;
	case 'q':
		qdiff++;
		break;
	case 's':
		statsflag++;
		break;
	case 'v':
		verbose++;
		break;
	}ARGEND;

	if(argc == 0)
		usage();

	if(bsize < 512)
		bsize = 512;
	if(bsize > VtMaxLumpSize)
		bsize = VtMaxLumpSize;
	maxbsize = bsize;

	fmtinstall('V', vtscorefmt);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	qsort(exclude, nexclude, sizeof(char*), strpcmp);

	vac(z, argv);

	if(vtsync(z) < 0)
		fprint(2, "warning: could not ask server to flush pending writes: %r\n");

	if(statsflag)
		fprint(2, "files %ld:%ld data %ld:%ld:%ld meta %ld\n", stats.file, stats.sfile,
			stats.data, stats.skip, stats.sdata, stats.meta);
//packetStats();
	vthangup(z);

	threadexitsall(0);
}

static int
strpcmp(const void *p0, const void *p1)
{
	return strcmp(*(char**)p0, *(char**)p1);
}

int
vacwrite(VtConn *z, uchar score[VtScoreSize], int type, uchar *buf, int n)
{
	assert(n > 0);
	if(nowrite){
		sha1(buf, n, score, nil);
		return 0;
	}
	return vtwrite(z, score, type, buf, n);
}

static char*
lastelem(char *oname)
{
	char *p;

	if(oname == nil)
		abort();
	if((p = strrchr(oname, '/')) == nil)
		return oname;
	return p+1;
}

static void
vac(VtConn *z, char *argv[])
{
	DirSink *dsink, *ds;
	MetaSink *ms;
	VtRoot root;
	uchar score[VtScoreSize], buf[VtRootSize];
	char cwd[2048];
	int cd;
	char *cp2, *cp;
	VacFs *fs;
	VacFile *vff;
	int fd;
	Dir *dir;
	VacDir vd;

	if(getwd(cwd, sizeof(cwd)) == 0)
		sysfatal("can't find current directory: %r\n");

	dsink = dirsinkalloc(z, bsize, bsize);

	fs = nil;
	if(dfile != nil) {
		fs = vacfsopen(z, dfile, VtOREAD, 1000);
		if(fs == nil)
			fprint(2, "could not open diff: %s: %r\n", dfile);
	}
		

	if(oname != nil) {
		fd = create(oname, OWRITE, 0666);
		if(fd < 0)
			sysfatal("could not create file: %s: %r", oname);
	} else 
		fd = 1;

	dir = dirfstat(fd);
	if(dir == nil)
		sysfatal("dirfstat failed: %r");
	if(oname)
		dir->name = lastelem(oname);
	else
		dir->name = "stdin";

	for(; *argv; argv++) {
		cp2 = *argv;
		cd = 0;
		for (cp = *argv; *cp; cp++)
			if (*cp == '/')
				cp2 = cp;
		if (cp2 != *argv) {
			*cp2 = '\0';
			chdir(*argv);
			*cp2 = '/';
			cp2++;
			cd = 1;
		}
		vff = nil;
		if(fs)
			vff = vacfileopen(fs, cp2);
		vacfile(dsink, argv[0], cp2, vff);
		if(vff)
			vacfiledecref(vff);
		if(cd && chdir(cwd) < 0)
			sysfatal("can't cd back to %s: %r\n", cwd);
	}
	
	if(isi) {
		vff = nil;
		if(fs)
			vff = vacfileopen(fs, isi);
		vacstdin(dsink, isi, vff);
		if(vff)
			vacfiledecref(vff);
	}

	dirsinkclose(dsink);

	/* build meta information for the root */
	ms = metasinkalloc(z, bsize, bsize);
	/* fake into a directory */
	dir->mode |= (dir->mode&0444)>>2;
	dir->qid.type |= QTDIR;
	dir->mode |= DMDIR;
	plan9tovacdir(&vd, dir, 0, fileid++);
	if(strcmp(vd.elem, "/") == 0){
		vtfree(vd.elem);
		vd.elem = vtstrdup("root");
	}
	metasinkwritedir(ms, &vd);
	vdcleanup(&vd);
	metasinkclose(ms);
	
	ds = dirsinkalloc(z, bsize, bsize);
	dirsinkwritesink(ds, dsink->sink);
	dirsinkwritesink(ds, dsink->msink->sink);
	dirsinkwritesink(ds, ms->sink);
	dirsinkclose(ds);

	memset(&root, 0, sizeof(root));		
	strncpy(root.name, dir->name, sizeof(root.name));
	root.name[sizeof(root.name)-1] = 0;
	free(dir);
	sprint(root.type, "vac");
	memmove(root.score, ds->sink->dir.score, VtScoreSize);
	root.blocksize = maxbsize;
	if(fs != nil)
		vacfsgetscore(fs, root.prev);

	metasinkfree(ms);
	dirsinkfree(ds);
	dirsinkfree(dsink);
	if(fs != nil)
		vacfsclose(fs);
	
	vtrootpack(&root, buf);
	if(vacwrite(z, score, VtRootType, buf, VtRootSize) < 0)
		sysfatal("vacWrite failed: %r");

	fprint(fd, "vac:%V\n", score);

	/* avoid remove at cleanup */
	oname = nil;
}

static int
isexcluded(char *name)
{
	int bot, top, i, x;

	bot = 0;	
	top = nexclude;
	while(bot < top) {
		i = (bot+top)>>1;
		x = strcmp(exclude[i], name);
		if(x == 0)
			return 1;
		if(x < 0)
			bot = i + 1;
		else /* x > 0 */
			top = i;
	}
	return 0;
}

static void
vacfile(DirSink *dsink, char *lname, char *sname, VacFile *vf)
{
	int fd;
	Dir *dir;
	VacDir vd;
	ulong entry;

	if(isexcluded(lname)) {
		warn("excluding: %s", lname);
		return;
	}

	if(merge && vacmerge(dsink, lname, sname) >= 0)
		return;

	if((dir = dirstat(sname)) == nil){
		warn("could not stat file %s: %r", lname);
		return;
	}
	if(dir->mode&(DMSYMLINK|DMDEVICE|DMNAMEDPIPE|DMSOCKET)){
		free(dir);
		return;
	}
	free(dir);
	
	fd = open(sname, OREAD);
	if(fd < 0) {
		warn("could not open file: %s: %r", lname);
		return;
	}

	if(verbose)
		fprint(2, "%s\n", lname);

	dir = dirfstat(fd);
	if(dir == nil) {
		warn("can't stat %s: %r", lname);
		close(fd);
		return;
	}
	dir->name = lastelem(sname);

	entry = dsink->nentry;

	if(dir->mode & DMDIR) 
		vacdir(dsink, fd, lname, sname, vf);
	else
		vacdata(dsink, fd, lname, vf, dir);

	plan9tovacdir(&vd, dir, entry, fileid++);
	metasinkwritedir(dsink->msink, &vd);
	vdcleanup(&vd);

	free(dir);
	close(fd);
}

static void
vacstdin(DirSink *dsink, char *name, VacFile *vf)
{
	Dir *dir;
	VacDir vd;
	ulong entry;

	if(verbose)
		fprint(2, "%s\n", "<stdio>");

	dir = dirfstat(0);
	if(dir == nil) {
		warn("can't stat <stdio>: %r");
		return;
	}
	dir->name = "stdin";

	entry = dsink->nentry;

	vacdata(dsink, 0, "<stdin>", vf, dir);

	plan9tovacdir(&vd, dir, entry, fileid++);
	vd.elem = vtstrdup(name);
	metasinkwritedir(dsink->msink, &vd);
	vdcleanup(&vd);

	free(dir);
}

static int
sha1check(u8int *score, uchar *buf, int n)
{
	char score2[VtScoreSize];

	sha1(buf, n, score, nil);
	if(memcmp(score, score2, VtScoreSize) == 0)
		return 0;
	return -1;
}

static ulong
vacdataskip(Sink *sink, VacFile *vf, int fd, ulong blocks, uchar *buf, char *lname)
{
	int n;
	ulong i;
	uchar score[VtScoreSize];

	/* skip blocks for append only files */
	if(seek(fd, (blocks-1)*bsize, 0) != (blocks-1)*bsize) {
		warn("error seeking: %s", lname);
		goto Err;
	}
	n = readn(fd, buf, bsize);
	if(n < bsize) {
		warn("error checking append only file: %s", lname);
		goto Err;
	}
	if(vacfileblockscore(vf, blocks-1, score)<0 || sha1check(score, buf, n)<0) {
		warn("last block of append file did not match: %s", lname);
		goto Err;
	}

	for(i=0; i<blocks; i++) {
		if(vacfileblockscore(vf, i, score) < 0) {
			warn("could not get score: %s: %lud", lname, i);
			seek(fd, i*bsize, 0);
			return i;
		}
		stats.skip++;
		sinkwritescore(sink, score, bsize);
	}

	return i;
Err:
	seek(fd, 0, 0);
	return 0;
}

static void
vacdata(DirSink *dsink, int fd, char *lname, VacFile *vf, Dir *dir)
{
	uchar *buf;
	Sink *sink;
	int n;
	uchar score[VtScoreSize];
	ulong block, same;
	VacDir vd;
	ulong vfblocks;

	vfblocks = 0;
	if(vf != nil && qdiff) {
		vacfilegetdir(vf, &vd);
		if(vd.mtime == dir->mtime)
		if(vd.size == dir->length)
		if(!vd.plan9 || /* vd.p9path == dir->qid.path && */ vd.p9version == dir->qid.vers)
		if(dirsinkwritefile(dsink, vf)) {
			stats.sfile++;
			vdcleanup(&vd);
			return;
		}

		/* look for an append only file */
		if((dir->mode&DMAPPEND) != 0)
		if(vd.size < dir->length)
		if(vd.plan9)
		if(vd.p9path == dir->qid.path)
			vfblocks = vd.size/bsize;

		vdcleanup(&vd);
	}
	stats.file++;

	buf = vtmalloc(bsize);
	sink = sinkalloc(dsink->sink->z, bsize, bsize);
	block = 0;
	same = stats.sdata+stats.skip;

	if(vfblocks > 1)
		block += vacdataskip(sink, vf, fd, vfblocks, buf, lname);

if(0) fprint(2, "vacData: %s: %ld\n", lname, block);
	for(;;) {
		n = readn(fd, buf, bsize);
		if(0 && n < 0)
			warn("file truncated due to read error: %s: %r", lname);
		if(n <= 0)
			break;
		if(vf != nil && vacfileblockscore(vf, block, score) && sha1check(score, buf, n)>=0) {
			stats.sdata++;
			sinkwritescore(sink, score, n);
		} else
			sinkwrite(sink, buf, n);
		block++;
	}
	same = stats.sdata+stats.skip - same;

	if(same && (dir->mode&DMAPPEND) != 0)
		if(0)fprint(2, "%s: total %lud same %lud:%lud diff %lud\n",
			lname, block, same, vfblocks, block-same);

	sinkclose(sink);
	dirsinkwritesink(dsink, sink);
	sinkfree(sink);
	free(buf);
}


static void
vacdir(DirSink *dsink, int fd, char *lname, char *sname, VacFile *vf)
{
	Dir *dirs;
	char *ln, *sn;
	int i, nd;
	DirSink *ds;
	VacFile *vvf;
	char *name;

	ds = dirsinkalloc(dsink->sink->z, bsize, bsize);
	while((nd = dirread(fd, &dirs)) > 0){
		for(i = 0; i < nd; i++){
			name = dirs[i].name;
			/* check for bad file names */
			if(name[0] == 0 || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;
			ln = vtmalloc(strlen(lname) + strlen(name) + 2);
			sn = vtmalloc(strlen(sname) + strlen(name) + 2);
			sprint(ln, "%s/%s", lname, name);
			sprint(sn, "%s/%s", sname, name);
			if(vf != nil)
				vvf = vacfilewalk(vf, name);
			else
				vvf = nil;
			vacfile(ds, ln, sn, vvf);
			if(vvf != nil)
				vacfiledecref(vvf);
			vtfree(ln);
			vtfree(sn);
		}
		free(dirs);
	}
	dirsinkclose(ds);
	dirsinkwritesink(dsink, ds->sink);
	dirsinkwritesink(dsink, ds->msink->sink);
	dirsinkfree(ds);
}

static int
vacmergefile(DirSink *dsink, VacFile *vf, VacDir *dir, uvlong offset, uvlong *max)
{
	uchar buf[VtEntrySize];
	VtEntry dd, md;
	int e;

	if(vacfileread(vf, buf, VtEntrySize, (uvlong)dir->entry*VtEntrySize) != VtEntrySize) {
		warn("could not read venti dir entry: %s\n", dir->elem);
		return -1;
	}
	vtentryunpack(&dd, buf, 0);

	if(dir->mode & ModeDir)	{
		e = dir->mentry;
		if(e == 0)
			e = dir->entry + 1;
		
		if(vacfileread(vf, buf, VtEntrySize, e*VtEntrySize) != VtEntrySize) {
			warn("could not read venti dir entry: %s\n", dir->elem);
			return 0;
		}
		vtentryunpack(&md, buf, 0);
	}

	/* max might incorrect in some old dumps */
	if(dir->qid >= *max) {
		warn("qid out of range: %s", dir->elem);
		*max = dir->qid;
	}

	dir->qid += offset;
	dir->entry = dsink->nentry;

	if(dir->qidspace) {
		dir->qidoffset += offset;
	} else {
		dir->qidspace = 1;
		dir->qidoffset = offset;
		dir->qidmax = *max;
	}

	dirsinkwrite(dsink, &dd);
	if(dir->mode & ModeDir)	
		dirsinkwrite(dsink, &md);
	metasinkwritedir(dsink->msink, dir);
	
	return 0;
}

static int
vacmerge(DirSink *dsink, char *lname, char *sname)
{
	char *p;
	VacFs *fs;
	VacFile *vf;
	VacDirEnum *d;
	VacDir dir;
	uvlong max;

	p = strrchr(sname, '.');
	if(p == 0 || strcmp(p, ".vac"))
		return 0;

	d = nil;
	fs = vacfsopen(dsink->sink->z, sname, VtOREAD, 100);
	if(fs == nil)
		return -1;

	vf = vacfileopen(fs, "/");
	if(vf == nil)
		goto Done;
	max = vacfilegetid(vf);
	d = vdeopen(vf);
	if(d == nil)
		goto Done;

	if(verbose)
		fprint(2, "merging: %s\n", lname);

	if(maxbsize < fs->bsize)
		maxbsize = fs->bsize;

	for(;;) {
		if(vderead(d, &dir) < 1)
			break;
		vacmergefile(dsink, vf, &dir, fileid, &max);
		vdcleanup(&dir);	
	}
	fileid += max;

Done:
	if(d != nil)
		vdeclose(d);
	if(vf != nil)
		vacfiledecref(vf);
	vacfsclose(fs);
	return 0;
}

Sink *
sinkalloc(VtConn *z, int psize, int dsize)
{
	Sink *k;
	int i;

	if(psize < 512 || psize > VtMaxLumpSize)
		sysfatal("sinkalloc: bad psize");
	if(dsize < 512 || dsize > VtMaxLumpSize)
		sysfatal("sinkalloc: bad psize");

	psize = VtScoreSize*(psize/VtScoreSize);

	k = vtmallocz(sizeof(Sink));
	k->z = z;
	k->dir.flags = VtEntryActive;
	k->dir.psize = psize;
	k->dir.dsize = dsize;
	k->buf = vtmallocz(VtPointerDepth*k->dir.psize + VtScoreSize);
	for(i=0; i<=VtPointerDepth; i++)
		k->pbuf[i] = k->buf + i*k->dir.psize;
	return k;
}

void
sinkwritescore(Sink *k, uchar score[VtScoreSize], int n)
{
	int i;
	uchar *p;
	VtEntry *d;

	memmove(k->pbuf[0], score, VtScoreSize);

	d = &k->dir;

	for(i=0; i<VtPointerDepth; i++) {
		k->pbuf[i] += VtScoreSize;
		if(k->pbuf[i] < k->buf + d->psize*(i+1))
			break;
		if(i == VtPointerDepth-1)
			sysfatal("file too big");
		p = k->buf+i*d->psize;
		stats.meta++;
		if(vacwrite(k->z, k->pbuf[i+1], VtDataType+1+i, p, d->psize) < 0)
			sysfatal("vacwrite failed: %r");
		k->pbuf[i] = p;
	}

	/* round size up to multiple of dsize */
	d->size = d->dsize * ((d->size + d->dsize-1)/d->dsize);
	
	d->size += n;
}

void
sinkwrite(Sink *k, uchar *p, int n)
{
	int type;
	uchar score[VtScoreSize];

	if(n > k->dir.dsize)
		sysfatal("sinkWrite: size too big");

	if((k->dir.type&~VtTypeDepthMask) == VtDirType){
		type = VtDirType;
		stats.meta++;
	} else {
		type = VtDataType;
		stats.data++;
	}
	if(vacwrite(k->z, score, type, p, n) < 0)
		sysfatal("vacWrite failed: %r");

	sinkwritescore(k, score, n);
}

static int
sizetodepth(uvlong s, int psize, int dsize)
{
	int np;
	int d;
	
	/* determine pointer depth */
	np = psize/VtScoreSize;
	s = (s + dsize - 1)/dsize;
	for(d = 0; s > 1; d++)
		s = (s + np - 1)/np;
	return d;
}

void
sinkclose(Sink *k)
{
	int i, n, base;
	uchar *p;
	VtEntry *kd;

	kd = &k->dir;

	/* empty */
	if(kd->size == 0) {
		memmove(kd->score, vtzeroscore, VtScoreSize);
		return;
	}

	for(n=VtPointerDepth-1; n>0; n--)
		if(k->pbuf[n] > k->buf + kd->psize*n)
			break;

	base = kd->type&~VtTypeDepthMask;
	kd->type = base + sizetodepth(kd->size, kd->psize, kd->dsize);

	/* skip full part of tree */
	for(i=0; i<n && k->pbuf[i] == k->buf + kd->psize*i; i++)
		;

	/* is the tree completely full */
	if(i == n && k->pbuf[n] == k->buf + kd->psize*n + VtScoreSize) {
		memmove(kd->score, k->pbuf[n] - VtScoreSize, VtScoreSize);
		return;
	}
	n++;

	/* clean up the edge */
	for(; i<n; i++) {
		p = k->buf+i*kd->psize;
		stats.meta++;
		if(vacwrite(k->z, k->pbuf[i+1], base+1+i, p, k->pbuf[i]-p) < 0)
			sysfatal("vacWrite failed: %r");
		k->pbuf[i+1] += VtScoreSize;
	}
	memmove(kd->score, k->pbuf[i] - VtScoreSize, VtScoreSize);
}

void
sinkfree(Sink *k)
{
	vtfree(k->buf);
	vtfree(k);
}

DirSink *
dirsinkalloc(VtConn *z, int psize, int dsize)
{
	DirSink *k;
	int ds;

	ds = VtEntrySize*(dsize/VtEntrySize);

	k = vtmallocz(sizeof(DirSink));
	k->sink = sinkalloc(z, psize, ds);
	k->sink->dir.type = VtDirType;
	k->msink = metasinkalloc(z, psize, dsize);
	k->buf = vtmalloc(ds);
	k->p = k->buf;
	k->ep = k->buf + ds;
	return k;
}

void
dirsinkwrite(DirSink *k, VtEntry *dir)
{
	if(k->p + VtEntrySize > k->ep) {
		sinkwrite(k->sink, k->buf, k->p - k->buf);
		k->p = k->buf;
	}
	vtentrypack(dir, k->p, 0);
	k->nentry++;
	k->p += VtEntrySize;
}

void
dirsinkwritesink(DirSink *k, Sink *sink)
{
	dirsinkwrite(k, &sink->dir);
}

int
dirsinkwritefile(DirSink *k, VacFile *vf)
{
	VtEntry dir;

	if(vacfilegetvtentry(vf, &dir) < 0)
		return -1;
	dirsinkwrite(k, &dir);
	return 0;
}

void
dirsinkclose(DirSink *k)
{
	metasinkclose(k->msink);
	if(k->p != k->buf)
		sinkwrite(k->sink, k->buf, k->p - k->buf);
	sinkclose(k->sink);
}

void
dirsinkfree(DirSink *k)
{
	sinkfree(k->sink);
	metasinkfree(k->msink);
	vtfree(k->buf);
	vtfree(k);
}

MetaSink*
metasinkalloc(VtConn *z, int psize, int dsize)
{
	MetaSink *k;

	k = vtmallocz(sizeof(MetaSink));
	k->sink = sinkalloc(z, psize, dsize);
	k->buf = vtmalloc(dsize);
	k->maxindex = dsize/100;	/* 100 byte entries seems reasonable */
	if(k->maxindex < 1)
		k->maxindex = 1;
	k->rp = k->p = k->buf + MetaHeaderSize + k->maxindex*MetaIndexSize;
	k->ep = k->buf + dsize;
	return k;
}

/* hack to get base to compare routine - not reentrant */
uchar *blockbase;

int
dircmp(const void *p0, const void *p1)
{
	uchar *q0, *q1;
	int n0, n1, r;

	/* name is first element of entry */
	q0 = (uchar*)p0;
	q0 = blockbase + (q0[0]<<8) + q0[1];
	n0 = (q0[6]<<8) + q0[7];
	q0 += 8;

	q1 = (uchar*)p1;
	q1 = blockbase + (q1[0]<<8) + q1[1];
	n1 = (q1[6]<<8) + q1[7];
	q1 += 8;

	if(n0 == n1)
		return memcmp(q0, q1, n0);
	else if (n0 < n1) {
		r = memcmp(q0, q1, n0);
		return (r==0)?1:r;
	} else  {
		r = memcmp(q0, q1, n1);
		return (r==0)?-1:r;
	}
}

void
metasinkflush(MetaSink *k)
{
	uchar *p;
	int n;
	MetaBlock mb;

	if(k->nindex == 0)
		return;
	assert(k->nindex <= k->maxindex);

	p = k->buf;
	n = k->rp - p;

	mb.size = n;
	mb.free = 0;
	mb.nindex = k->nindex;
	mb.maxindex = k->maxindex;
	mb.buf = p;
	mbpack(&mb);
	
	p += MetaHeaderSize;

	/* XXX this is not reentrant! */
	blockbase = k->buf;
	qsort(p, k->nindex, MetaIndexSize, dircmp);
	p += k->nindex*MetaIndexSize;
	
	memset(p, 0, (k->maxindex-k->nindex)*MetaIndexSize);
	p += (k->maxindex-k->nindex)*MetaIndexSize;

	sinkwrite(k->sink, k->buf, n);

	/* move down partial entry */
	n = k->p - k->rp;
	memmove(p, k->rp, n);
	k->rp = p;
	k->p = p + n;
	k->nindex = 0;
}

void
metasinkputc(MetaSink *k, int c)
{
	if(k->p+1 > k->ep)
		metasinkflush(k);
	if(k->p+1 > k->ep)
		sysfatal("directory entry too large");
	k->p[0] = c;
	k->p++;
}

void
metasinkputstring(MetaSink *k, char *s)
{
	int n = strlen(s);
	metasinkputc(k, n>>8);
	metasinkputc(k, n);
	metasinkwrite(k, (uchar*)s, n);
}

void
metasinkputuint32(MetaSink *k, ulong x)
{
	metasinkputc(k, x>>24);
	metasinkputc(k, x>>16);
	metasinkputc(k, x>>8);
	metasinkputc(k, x);
}

void
metasinkputuint64(MetaSink *k, uvlong x)
{
	metasinkputuint32(k, x>>32);
	metasinkputuint32(k, x);
}

void
metasinkwrite(MetaSink *k, uchar *data, int n)
{
	if(k->p + n > k->ep)
		metasinkflush(k);
	if(k->p + n > k->ep)
		sysfatal("directory entry too large");
	
	memmove(k->p, data, n);
	k->p += n;
}

void
metasinkwritedir(MetaSink *ms, VacDir *dir)
{
	metasinkputuint32(ms, DirMagic);
	metasinkputc(ms, Version>>8);
	metasinkputc(ms, Version);		
	metasinkputstring(ms, dir->elem);
	metasinkputuint32(ms, dir->entry);
	metasinkputuint64(ms, dir->qid);
	metasinkputstring(ms, dir->uid);
	metasinkputstring(ms, dir->gid);
	metasinkputstring(ms, dir->mid);
	metasinkputuint32(ms, dir->mtime);
	metasinkputuint32(ms, dir->mcount);
	metasinkputuint32(ms, dir->ctime);
	metasinkputuint32(ms, dir->atime);
	metasinkputuint32(ms, dir->mode);

	if(dir->plan9) {
		metasinkputc(ms, DirPlan9Entry);	/* plan9 extra info */
		metasinkputc(ms, 0);			/* plan9 extra size */
		metasinkputc(ms, 12);			/* plan9 extra size */
		metasinkputuint64(ms, dir->p9path);
		metasinkputuint32(ms, dir->p9version);
	}

	if(dir->qidspace != 0) {
		metasinkputc(ms, DirQidSpaceEntry);
		metasinkputc(ms, 0);
		metasinkputc(ms, 16);
		metasinkputuint64(ms, dir->qidoffset);
		metasinkputuint64(ms, dir->qidmax);
	}

	if(dir->gen != 0) {
		metasinkputc(ms, DirGenEntry);
		metasinkputc(ms, 0);
		metasinkputc(ms, 4);
		metasinkputuint32(ms, dir->gen);
	}

	metasinkeor(ms);
}


void
plan9tovacdir(VacDir *vd, Dir *dir, ulong entry, uvlong qid)
{
	memset(vd, 0, sizeof(VacDir));

	vd->elem = vtstrdup(dir->name);
	vd->entry = entry;
	vd->qid = qid;
	vd->uid = vtstrdup(dir->uid);
	vd->gid = vtstrdup(dir->gid);
	vd->mid = vtstrdup(dir->muid);
	vd->mtime = dir->mtime;
	vd->mcount = 0;
	vd->ctime = dir->mtime;		/* ctime: not available on plan 9 */
	vd->atime = dir->atime;

	vd->mode = dir->mode & 0777;
	if(dir->mode & DMDIR)
		vd->mode |= ModeDir;
	if(dir->mode & DMAPPEND)
		vd->mode |= ModeAppend;
	if(dir->mode & DMEXCL)
		vd->mode |= ModeExclusive;

	vd->plan9 = 1;
	vd->p9path = dir->qid.path;
	vd->p9version = dir->qid.vers;
}


void
metasinkeor(MetaSink *k)
{
	uchar *p;
	int o, n;

	p = k->buf + MetaHeaderSize;
	p += k->nindex * MetaIndexSize;
	o = k->rp-k->buf; 	/* offset from start of block */
	n = k->p-k->rp;		/* size of entry */
	p[0] = o >> 8;
	p[1] = o;
	p[2] = n >> 8;
	p[3] = n;
	k->rp = k->p;
	k->nindex++;
	if(k->nindex == k->maxindex)
		metasinkflush(k);
}

void
metasinkclose(MetaSink *k)
{
	metasinkflush(k);
	sinkclose(k->sink);
}

void
metasinkfree(MetaSink *k)
{
	sinkfree(k->sink);
	vtfree(k->buf);
	vtfree(k);
}

static void
warn(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	fprint(2, "%s: ", argv0);
	vfprint(2, fmt, arg);
	fprint(2, "\n");
	va_end(arg);
}

static void
cleanup(void)
{
	if(oname != nil)
		remove(oname);
}

#define TWID64	((u64int)~(u64int)0)

static u64int
unittoull(char *s)
{
	char *es;
	u64int n;

	if(s == nil)
		return TWID64;
	n = strtoul(s, &es, 0);
	if(*es == 'k' || *es == 'K'){
		n *= 1024;
		es++;
	}else if(*es == 'm' || *es == 'M'){
		n *= 1024*1024;
		es++;
	}else if(*es == 'g' || *es == 'G'){
		n *= 1024*1024*1024;
		es++;
	}
	if(*es != '\0')
		return TWID64;
	return n;
}
