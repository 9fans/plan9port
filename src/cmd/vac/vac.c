#include <u.h>
#include <libc.h>
#include <venti.h>

int bsize;
char *host;
VtConn *z;

void
usage(void)
{
	fprint(2, "usage: vac [-b blocksize] [-h host] file\n");
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	default:
		usage();
	case 'b':
		bsize = unittoull(EARGF(usage()));
		break;
	case 'h':
		host = EARGF(usage());
		break;
	}ARGEND

	if(bsize < 512)
		bsize = 512;
	if(bsize > VtMaxLumpSize)
		bsize = VtMaxLumpSize;
	maxbsize = bsize;

	vtAttach();

	fmtinstall('V', vtScoreFmt);
	fmtinstall('R', vtErrFmt);

	z = vtDial(host, 0);
	if(z == nil)
		vtFatal("could not connect to server: %R");

	if(!vtConnect(z, 0))
		vtFatal("vtConnect: %R");

	qsort(exclude, nexclude, sizeof(char*), strpCmp);

	vac(z, argv);
	if(!vtSync(z))
		fprint(2, "warning: could not ask server to flush pending writes: %R\n");

	if(statsFlag)
		fprint(2, "files %ld:%ld data %ld:%ld:%ld meta %ld\n", stats.file, stats.sfile,
			stats.data, stats.skip, stats.sdata, stats.meta);
//packetStats();
	vtClose(z);
	vtDetach();

	exits(0);
}

static int
vac(VtSession *z, char *argv[])
{
	DirSink *dsink, *ds;
	MetaSink *ms;
	VtRoot root;
	uchar score[VtScoreSize], buf[VtRootSize];
	char cwd[2048];
	int cd, i;
	char *cp2, *cp;
	VacFS *fs;
	VacFile *vff;
	int fd;
	Dir *dir;
	VacDir vd;

	if(getwd(cwd, sizeof(cwd)) == 0)
		sysfatal("can't find current directory: %r\n");

	dsink = dirSinkAlloc(z, bsize, bsize);

	fs = nil;
	if(dfile != nil) {
		fs = vfsOpen(z, dfile, 1, 10000);
		if(fs == nil)
			fprint(2, "could not open diff: %s: %s\n", dfile, vtGetError());
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
			vff = vfOpen(fs, cp2);
		vacFile(dsink, argv[0], cp2, vff);
		if(vff)
			vfDecRef(vff);
		if(cd && chdir(cwd) < 0)
			sysfatal("can't cd back to %s: %r\n", cwd);
	}
	
	if(isi) {
		vff = nil;
		if(fs)
			vff = vfOpen(fs, isi);
		vacStdin(dsink, isi, vff);
		if(vff)
			vfDecRef(vff);
	}

	dirSinkClose(dsink);

	/* build meta information for the root */
	ms = metaSinkAlloc(z, bsize, bsize);
	/* fake into a directory */
	dir->mode |= (dir->mode&0444)>>2;
	dir->qid.type |= QTDIR;
	dir->mode |= DMDIR;
	plan9ToVacDir(&vd, dir, 0, fileid++);
	if(strcmp(vd.elem, "/") == 0){
		vtMemFree(vd.elem);
		vd.elem = vtStrDup("root");
	}
	metaSinkWriteDir(ms, &vd);
	vdCleanup(&vd);
	metaSinkClose(ms);
	
	ds = dirSinkAlloc(z, bsize, bsize);
	dirSinkWriteSink(ds, dsink->sink);
	dirSinkWriteSink(ds, dsink->msink->sink);
	dirSinkWriteSink(ds, ms->sink);
	dirSinkClose(ds);

	memset(&root, 0, sizeof(root));		
	root.version = VtRootVersion;
	strncpy(root.name, dir->name, sizeof(root.name));
	root.name[sizeof(root.name)-1] = 0;
	free(dir);
	sprint(root.type, "vac");
	memmove(root.score, ds->sink->dir.score, VtScoreSize);
	root.blockSize = maxbsize;
	if(fs != nil)
		vfsGetScore(fs, root.prev);

	metaSinkFree(ms);
	dirSinkFree(ds);
	dirSinkFree(dsink);
	if(fs != nil)
		vfsClose(fs);
	
	vtRootPack(&root, buf);
	if(!vacWrite(z, score, VtRootType, buf, VtRootSize))
		vtFatal("vacWrite failed: %s", vtGetError());

	fprint(fd, "vac:");
	for(i=0; i<VtScoreSize; i++)
		fprint(fd, "%.2x", score[i]);
	fprint(fd, "\n");
	
	/* avoid remove at cleanup */
	oname = nil;
	return 1;
}

static int
isExcluded(char *name)
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
vacFile(DirSink *dsink, char *lname, char *sname, VacFile *vf)
{
	int fd;
	Dir *dir;
	VacDir vd;
	ulong entry;

	if(isExcluded(lname)) {
		warn("excluding: %s", lname);
		return;
	}

	if(merge && vacMerge(dsink, lname, sname))
		return;

	fd = open(sname, OREAD);
	if(fd < 0) {
		warn("could not open file: %s: %s", lname, vtOSError());
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

	entry = dsink->nentry;

	if(dir->mode & DMDIR) 
		vacDir(dsink, fd, lname, sname, vf);
	else
		vacData(dsink, fd, lname, vf, dir);

	plan9ToVacDir(&vd, dir, entry, fileid++);
	metaSinkWriteDir(dsink->msink, &vd);
	vdCleanup(&vd);

	free(dir);
	close(fd);
}

static void
vacStdin(DirSink *dsink, char *name, VacFile *vf)
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

	entry = dsink->nentry;

	vacData(dsink, 0, "<stdin>", vf, dir);

	plan9ToVacDir(&vd, dir, entry, fileid++);
	vd.elem = vtStrDup(name);
	metaSinkWriteDir(dsink->msink, &vd);
	vdCleanup(&vd);

	free(dir);
}

static ulong
vacDataSkip(Sink *sink, VacFile *vf, int fd, ulong blocks, uchar *buf, char *lname)
{
	int n;
	ulong i;
	uchar score[VtScoreSize];

	/* skip blocks for append only files */
	if(seek(fd, (blocks-1)*bsize, 0) != (blocks-1)*bsize) {
		warn("error seeking: %s", lname);
		goto Err;
	}
	n = readBlock(fd, buf, bsize);
	if(n < bsize) {
		warn("error checking append only file: %s", lname);
		goto Err;
	}
	if(!vfGetBlockScore(vf, blocks-1, score) || !vtSha1Check(score, buf, n)) {
		warn("last block of append file did not match: %s", lname);
		goto Err;
	}

	for(i=0; i<blocks; i++) {
		if(!vfGetBlockScore(vf, i, score)) {
			warn("could not get score: %s: %lud", lname, i);
			seek(fd, i*bsize, 0);
			return i;
		}
		stats.skip++;
		sinkWriteScore(sink, score, bsize);
	}

	return i;
Err:
	seek(fd, 0, 0);
	return 0;
}

static void
vacData(DirSink *dsink, int fd, char *lname, VacFile *vf, Dir *dir)
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
		vfGetDir(vf, &vd);
		if(vd.mtime == dir->mtime)
		if(vd.size == dir->length)
		if(!vd.plan9 || /* vd.p9path == dir->qid.path && */ vd.p9version == dir->qid.vers)
		if(dirSinkWriteFile(dsink, vf)) {
			stats.sfile++;
			vdCleanup(&vd);
			return;
		}

		/* look for an append only file */
		if((dir->mode&DMAPPEND) != 0)
		if(vd.size < dir->length)
		if(vd.plan9)
		if(vd.p9path == dir->qid.path)
			vfblocks = vd.size/bsize;

		vdCleanup(&vd);
	}
	stats.file++;

	buf = vtMemAlloc(bsize);
	sink = sinkAlloc(dsink->sink->z, bsize, bsize);
	block = 0;
	same = stats.sdata+stats.skip;

	if(vfblocks > 1)
		block += vacDataSkip(sink, vf, fd, vfblocks, buf, lname);

if(0) fprint(2, "vacData: %s: %ld\n", lname, block);
	for(;;) {
		n = readBlock(fd, buf, bsize);
		if(0 && n < 0)
			warn("file truncated due to read error: %s: %s", lname, vtOSError());
		if(n <= 0)
			break;
		if(vf != nil && vfGetBlockScore(vf, block, score) && vtSha1Check(score, buf, n)) {
			stats.sdata++;
			sinkWriteScore(sink, score, n);
		} else
			sinkWrite(sink, buf, n);
		block++;
	}
	same = stats.sdata+stats.skip - same;

	if(same && (dir->mode&DMAPPEND) != 0)
		if(0)fprint(2, "%s: total %lud same %lud:%lud diff %lud\n",
			lname, block, same, vfblocks, block-same);

	sinkClose(sink);
	dirSinkWriteSink(dsink, sink);
	sinkFree(sink);
	free(buf);
}


static void
vacDir(DirSink *dsink, int fd, char *lname, char *sname, VacFile *vf)
{
	Dir *dirs;
	char *ln, *sn;
	int i, nd;
	DirSink *ds;
	VacFile *vvf;
	char *name;

	ds = dirSinkAlloc(dsink->sink->z, bsize, bsize);
	while((nd = dirread(fd, &dirs)) > 0){
		for(i = 0; i < nd; i++){
			name = dirs[i].name;
			/* check for bad file names */
			if(name[0] == 0 || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;
			ln = vtMemAlloc(strlen(lname) + strlen(name) + 2);
			sn = vtMemAlloc(strlen(sname) + strlen(name) + 2);
			sprint(ln, "%s/%s", lname, name);
			sprint(sn, "%s/%s", sname, name);
			if(vf != nil)
				vvf = vfWalk(vf, name);
			else
				vvf = nil;
			vacFile(ds, ln, sn, vvf);
			if(vvf != nil)
				vfDecRef(vvf);
			vtMemFree(ln);
			vtMemFree(sn);
		}
		free(dirs);
	}
	dirSinkClose(ds);
	dirSinkWriteSink(dsink, ds->sink);
	dirSinkWriteSink(dsink, ds->msink->sink);
	dirSinkFree(ds);
}

static int
vacMergeFile(DirSink *dsink, VacFile *vf, VacDir *dir, uvlong offset, uvlong *max)
{
	uchar buf[VtEntrySize];
	VtEntry dd, md;
	int e;

	if(vfRead(vf, buf, VtEntrySize, (uvlong)dir->entry*VtEntrySize) != VtEntrySize) {
		warn("could not read venti dir entry: %s\n", dir->elem);
		return 0;
	}
	vtEntryUnpack(&dd, buf, 0);

	if(dir->mode & ModeDir)	{
		e = dir->mentry;
		if(e == 0)
			e = dir->entry + 1;
		
		if(vfRead(vf, buf, VtEntrySize, e*VtEntrySize) != VtEntrySize) {
			warn("could not read venti dir entry: %s\n", dir->elem);
			return 0;
		}
		vtEntryUnpack(&md, buf, 0);
	}

	/* max might incorrect in some old dumps */
	if(dir->qid >= *max) {
		warn("qid out of range: %s", dir->elem);
		*max = dir->qid;
	}

	dir->qid += offset;
	dir->entry = dsink->nentry;

	if(dir->qidSpace) {
		dir->qidOffset += offset;
	} else {
		dir->qidSpace = 1;
		dir->qidOffset = offset;
		dir->qidMax = *max;
	}

	dirSinkWrite(dsink, &dd);
	if(dir->mode & ModeDir)	
		dirSinkWrite(dsink, &md);
	metaSinkWriteDir(dsink->msink, dir);
	
	return 1;
}

static int
vacMerge(DirSink *dsink, char *lname, char *sname)
{
	char *p;
	VacFS *fs;
	VacFile *vf;
	VacDirEnum *d;
	VacDir dir;
	uvlong max;

	p = strrchr(sname, '.');
	if(p == 0 || strcmp(p, ".vac"))
		return 0;

	d = nil;
	fs = vfsOpen(dsink->sink->z, sname, 1, 100);
	if(fs == nil)
		return 0;

	vf = vfOpen(fs, "/");
	if(vf == nil)
		goto Done;
	max = vfGetId(vf);
	d = vdeOpen(fs, "/");
	if(d == nil)
		goto Done;

	if(verbose)
		fprint(2, "merging: %s\n", lname);

	if(maxbsize < vfsGetBlockSize(fs))
		maxbsize = vfsGetBlockSize(fs);

	for(;;) {
		if(vdeRead(d, &dir, 1) < 1)
			break;
		vacMergeFile(dsink, vf, &dir, fileid, &max);
		vdCleanup(&dir);	
	}
	fileid += max;

Done:
	if(d != nil)
		vdeFree(d);
	if(vf != nil)
		vfDecRef(vf);
	vfsClose(fs);
	return 1;
}

Sink *
sinkAlloc(VtSession *z, int psize, int dsize)
{
	Sink *k;
	int i;

	if(psize < 512 || psize > VtMaxLumpSize)
		vtFatal("sinkAlloc: bad psize");
	if(dsize < 512 || dsize > VtMaxLumpSize)
		vtFatal("sinkAlloc: bad psize");

	psize = VtScoreSize*(psize/VtScoreSize);

	k = vtMemAllocZ(sizeof(Sink));
	k->z = z;
	k->dir.flags = VtEntryActive;
	k->dir.psize = psize;
	k->dir.dsize = dsize;
	k->buf = vtMemAllocZ(VtPointerDepth*k->dir.psize + VtScoreSize);
	for(i=0; i<=VtPointerDepth; i++)
		k->pbuf[i] = k->buf + i*k->dir.psize;
	return k;
}

void
sinkWriteScore(Sink *k, uchar score[VtScoreSize], int n)
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
			vtFatal("file too big");
		p = k->buf+i*d->psize;
		stats.meta++;
		if(!vacWrite(k->z, k->pbuf[i+1], VtPointerType0+i, p, d->psize))
			vtFatal("vacWrite failed: %s", vtGetError());
		k->pbuf[i] = p;
	}

	/* round size up to multiple of dsize */
	d->size = d->dsize * ((d->size + d->dsize-1)/d->dsize);
	
	d->size += n;
}

void
sinkWrite(Sink *k, uchar *p, int n)
{
	int type;
	uchar score[VtScoreSize];

	if(n > k->dir.dsize)
		vtFatal("sinkWrite: size too big");

	if(k->dir.flags & VtEntryDir) {
		type = VtDirType;
		stats.meta++;
	} else {
		type = VtDataType;
		stats.data++;
	}
	if(!vacWrite(k->z, score, type, p, n))
		vtFatal("vacWrite failed: %s", vtGetError());

	sinkWriteScore(k, score, n);
}

static int
sizeToDepth(uvlong s, int psize, int dsize)
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
sinkClose(Sink *k)
{
	int i, n;
	uchar *p;
	VtEntry *kd;

	kd = &k->dir;

	/* empty */
	if(kd->size == 0) {
		memmove(kd->score, vtZeroScore, VtScoreSize);
		return;
	}

	for(n=VtPointerDepth-1; n>0; n--)
		if(k->pbuf[n] > k->buf + kd->psize*n)
			break;

	kd->depth = sizeToDepth(kd->size, kd->psize, kd->dsize);

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
		if(!vacWrite(k->z, k->pbuf[i+1], VtPointerType0+i, p, k->pbuf[i]-p))
			vtFatal("vacWrite failed: %s", vtGetError());
		k->pbuf[i+1] += VtScoreSize;
	}
	memmove(kd->score, k->pbuf[i] - VtScoreSize, VtScoreSize);
}

void
sinkFree(Sink *k)
{
	vtMemFree(k->buf);
	vtMemFree(k);
}

DirSink *
dirSinkAlloc(VtSession *z, int psize, int dsize)
{
	DirSink *k;
	int ds;

	ds = VtEntrySize*(dsize/VtEntrySize);

	k = vtMemAllocZ(sizeof(DirSink));
	k->sink = sinkAlloc(z, psize, ds);
	k->sink->dir.flags |= VtEntryDir;
	k->msink = metaSinkAlloc(z, psize, dsize);
	k->buf = vtMemAlloc(ds);
	k->p = k->buf;
	k->ep = k->buf + ds;
	return k;
}

void
dirSinkWrite(DirSink *k, VtEntry *dir)
{
	if(k->p + VtEntrySize > k->ep) {
		sinkWrite(k->sink, k->buf, k->p - k->buf);
		k->p = k->buf;
	}
	vtEntryPack(dir, k->p, 0);
	k->nentry++;
	k->p += VtEntrySize;
}

void
dirSinkWriteSink(DirSink *k, Sink *sink)
{
	dirSinkWrite(k, &sink->dir);
}

int
dirSinkWriteFile(DirSink *k, VacFile *vf)
{
	VtEntry dir;

	if(!vfGetVtEntry(vf, &dir))
		return 0;
	dirSinkWrite(k, &dir);
	return 1;
}

void
dirSinkClose(DirSink *k)
{
	metaSinkClose(k->msink);
	if(k->p != k->buf)
		sinkWrite(k->sink, k->buf, k->p - k->buf);
	sinkClose(k->sink);
}

void
dirSinkFree(DirSink *k)
{
	sinkFree(k->sink);
	metaSinkFree(k->msink);
	vtMemFree(k->buf);
	vtMemFree(k);
}

MetaSink *
metaSinkAlloc(VtSession *z, int psize, int dsize)
{
	MetaSink *k;

	k = vtMemAllocZ(sizeof(MetaSink));
	k->sink = sinkAlloc(z, psize, dsize);
	k->buf = vtMemAlloc(dsize);
	k->maxindex = dsize/100;	/* 100 byte entries seems reasonable */
	if(k->maxindex < 1)
		k->maxindex = 1;
	k->rp = k->p = k->buf + MetaHeaderSize + k->maxindex*MetaIndexSize;
	k->ep = k->buf + dsize;
	return k;
}

/* hack to get base to compare routine - not reentrant */
uchar *blockBase;

int
dirCmp(void *p0, void *p1)
{
	uchar *q0, *q1;
	int n0, n1, r;

	/* name is first element of entry */
	q0 = p0;
	q0 = blockBase + (q0[0]<<8) + q0[1];
	n0 = (q0[6]<<8) + q0[7];
	q0 += 8;

	q1 = p1;
	q1 = blockBase + (q1[0]<<8) + q1[1];
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
metaSinkFlush(MetaSink *k)
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
	mbPack(&mb);
	
	p += MetaHeaderSize;

	/* XXX this is not reentrant! */
	blockBase = k->buf;
	qsort(p, k->nindex, MetaIndexSize, dirCmp);
	p += k->nindex*MetaIndexSize;
	
	memset(p, 0, (k->maxindex-k->nindex)*MetaIndexSize);
	p += (k->maxindex-k->nindex)*MetaIndexSize;

	sinkWrite(k->sink, k->buf, n);

	/* move down partial entry */
	n = k->p - k->rp;
	memmove(p, k->rp, n);
	k->rp = p;
	k->p = p + n;
	k->nindex = 0;
}

void
metaSinkPutc(MetaSink *k, int c)
{
	if(k->p+1 > k->ep)
		metaSinkFlush(k);
	if(k->p+1 > k->ep)
		vtFatal("directory entry too large");
	k->p[0] = c;
	k->p++;
}

void
metaSinkPutString(MetaSink *k, char *s)
{
	int n = strlen(s);
	metaSinkPutc(k, n>>8);
	metaSinkPutc(k, n);
	metaSinkWrite(k, (uchar*)s, n);
}

void
metaSinkPutUint32(MetaSink *k, ulong x)
{
	metaSinkPutc(k, x>>24);
	metaSinkPutc(k, x>>16);
	metaSinkPutc(k, x>>8);
	metaSinkPutc(k, x);
}

void
metaSinkPutUint64(MetaSink *k, uvlong x)
{
	metaSinkPutUint32(k, x>>32);
	metaSinkPutUint32(k, x);
}

void
metaSinkWrite(MetaSink *k, uchar *data, int n)
{
	if(k->p + n > k->ep)
		metaSinkFlush(k);
	if(k->p + n > k->ep)
		vtFatal("directory entry too large");
	
	memmove(k->p, data, n);
	k->p += n;
}

void
metaSinkWriteDir(MetaSink *ms, VacDir *dir)
{
	metaSinkPutUint32(ms, DirMagic);
	metaSinkPutc(ms, Version>>8);
	metaSinkPutc(ms, Version);		
	metaSinkPutString(ms, dir->elem);
	metaSinkPutUint32(ms, dir->entry);
	metaSinkPutUint64(ms, dir->qid);
	metaSinkPutString(ms, dir->uid);
	metaSinkPutString(ms, dir->gid);
	metaSinkPutString(ms, dir->mid);
	metaSinkPutUint32(ms, dir->mtime);
	metaSinkPutUint32(ms, dir->mcount);
	metaSinkPutUint32(ms, dir->ctime);
	metaSinkPutUint32(ms, dir->atime);
	metaSinkPutUint32(ms, dir->mode);

	if(dir->plan9) {
		metaSinkPutc(ms, DirPlan9Entry);	/* plan9 extra info */
		metaSinkPutc(ms, 0);			/* plan9 extra size */
		metaSinkPutc(ms, 12);			/* plan9 extra size */
		metaSinkPutUint64(ms, dir->p9path);
		metaSinkPutUint32(ms, dir->p9version);
	}

	if(dir->qidSpace != 0) {
		metaSinkPutc(ms, DirQidSpaceEntry);
		metaSinkPutc(ms, 0);
		metaSinkPutc(ms, 16);
		metaSinkPutUint64(ms, dir->qidOffset);
		metaSinkPutUint64(ms, dir->qidMax);
	}

	if(dir->gen != 0) {
		metaSinkPutc(ms, DirGenEntry);
		metaSinkPutc(ms, 0);
		metaSinkPutc(ms, 4);
		metaSinkPutUint32(ms, dir->gen);
	}

	metaSinkEOR(ms);
}


void
plan9ToVacDir(VacDir *vd, Dir *dir, ulong entry, uvlong qid)
{
	memset(vd, 0, sizeof(VacDir));

	vd->elem = vtStrDup(dir->name);
	vd->entry = entry;
	vd->qid = qid;
	vd->uid = vtStrDup(dir->uid);
	vd->gid = vtStrDup(dir->gid);
	vd->mid = vtStrDup(dir->muid);
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
metaSinkEOR(MetaSink *k)
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
		metaSinkFlush(k);
}

void
metaSinkClose(MetaSink *k)
{
	metaSinkFlush(k);
	sinkClose(k->sink);
}

void
metaSinkFree(MetaSink *k)
{
	sinkFree(k->sink);
	vtMemFree(k->buf);
	vtMemFree(k);
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
