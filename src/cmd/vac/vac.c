#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

// TODO: qids

void
usage(void)
{
	fprint(2, "vac [-imqsv] [-a archive.vac] [-b bsize] [-d old.vac] [-f new.vac] [-e exclude]... [-h host] file...\n");
	threadexitsall("usage");
}

enum
{
	BlockSize = 8*1024,
	CacheSize = 128<<20,
};

struct
{
	int nfile;
	int ndir;
	vlong data;
	vlong skipdata;
	int skipfiles;
} stats;

int qdiff;
int merge;
int verbose;
char *host;
VtConn *z;
VacFs *fs;
char *archivefile;
char *vacfile;

int vacmerge(VacFile*, char*);
void vac(VacFile*, VacFile*, char*, Dir*);
void vacstdin(VacFile*, char*);
VacFile *recentarchive(VacFs*, char*);

static u64int unittoull(char*);
static void warn(char *fmt, ...);
static void removevacfile(void);

#ifdef PLAN9PORT
/*
 * We're between a rock and a hard place here.
 * The pw library (getpwnam, etc.) reads the
 * password and group files into an on-stack buffer,
 * so if you have some huge groups, you overflow
 * the stack.  Because of this, the thread library turns
 * it off by default, so that dirstat returns "14571" instead of "rsc".
 * But for vac we want names.  So cautiously turn the pwlibrary
 * back on (see threadmain) and make the main thread stack huge.
 */
extern int _p9usepwlibrary;
int mainstacksize = 4*1024*1024;

#endif
void
threadmain(int argc, char **argv)
{
	int i, j, fd, n, printstats;
	Dir *d;
	char *s;
	uvlong u;
	VacFile *f, *fdiff;
	VacFs *fsdiff;
	int blocksize;
	int outfd;
	char *stdinname;
	char *diffvac;
	uvlong qid;

#ifdef PLAN9PORT
	/* see comment above */
	_p9usepwlibrary = 1;
#endif

	fmtinstall('F', vtfcallfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('V', vtscorefmt);

	blocksize = BlockSize;
	stdinname = nil;
	printstats = 0;
	fsdiff = nil;
	diffvac = nil;

	ARGBEGIN{
	case 'V':
		chattyventi++;
		break;
	case 'a':
		archivefile = EARGF(usage());
		break;
	case 'b':
		u = unittoull(EARGF(usage()));
		if(u < 512)
			u = 512;
		blocksize = u;
		break;
	case 'd':
		diffvac = EARGF(usage());
		break;
	case 'e':
		excludepattern(EARGF(usage()));
		break;
	case 'f':
		vacfile = EARGF(usage());
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 'i':
		stdinname = EARGF(usage());
		break;
	case 'm':
		merge++;
		break;
	case 'q':
		qdiff++;
		break;
	case 's':
		printstats++;
		break;
	case 'v':
		verbose++;
		break;
	case 'x':
		loadexcludefile(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if(argc == 0 && !stdinname)
		usage();

	if(archivefile && (vacfile || diffvac)){
		fprint(2, "cannot use -a with -f, -d\n");
		usage();
	}

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	// Setup:
	//	fs is the output vac file system
	//	f is directory in output vac to write new files
	//	fdiff is corresponding directory in existing vac
	if(archivefile){
		VacFile *fp;
		char yyyy[5];
		char mmdd[10];
		char oldpath[40];
		Tm tm;

		fdiff = nil;
		if((outfd = open(archivefile, ORDWR)) < 0){
			if(access(archivefile, 0) >= 0)
				sysfatal("open %s: %r", archivefile);
			if((outfd = create(archivefile, OWRITE, 0666)) < 0)
				sysfatal("create %s: %r", archivefile);
			atexit(removevacfile);	// because it is new
			if((fs = vacfscreate(z, blocksize, CacheSize)) == nil)
				sysfatal("vacfscreate: %r");
		}else{
			if((fs = vacfsopen(z, archivefile, VtORDWR, CacheSize)) == nil)
				sysfatal("vacfsopen %s: %r", archivefile);
			if((fdiff = recentarchive(fs, oldpath)) != nil){
				if(verbose)
					fprint(2, "diff %s\n", oldpath);
			}else
				if(verbose)
					fprint(2, "no recent archive to diff against\n");
		}

		// Create yyyy/mmdd.
		tm = *localtime(time(0));
		snprint(yyyy, sizeof yyyy, "%04d", tm.year+1900);
		fp = vacfsgetroot(fs);
		if((f = vacfilewalk(fp, yyyy)) == nil
		&& (f = vacfilecreate(fp, yyyy, ModeDir|0555)) == nil)
			sysfatal("vacfscreate %s: %r", yyyy);
		vacfiledecref(fp);
		fp = f;

		snprint(mmdd, sizeof mmdd, "%02d%02d", tm.mon+1, tm.mday);
		n = 0;
		while((f = vacfilewalk(fp, mmdd)) != nil){
			vacfiledecref(f);
			n++;
			snprint(mmdd+4, sizeof mmdd-4, ".%d", n);
		}
		f = vacfilecreate(fp, mmdd, ModeDir|0555);
		if(f == nil)
			sysfatal("vacfscreate %s/%s: %r", yyyy, mmdd);
		vacfiledecref(fp);

		if(verbose)
			fprint(2, "archive %s/%s\n", yyyy, mmdd);
	}else{
		if(vacfile == nil)
			outfd = 1;
		else if((outfd = create(vacfile, OWRITE, 0666)) < 0)
			sysfatal("create %s: %r", vacfile);
		atexit(removevacfile);
		if((fs = vacfscreate(z, blocksize, CacheSize)) == nil)
			sysfatal("vacfscreate: %r");
		f = vacfsgetroot(fs);

		fdiff = nil;
		if(diffvac){
			if((fsdiff = vacfsopen(z, diffvac, VtOREAD, CacheSize)) == nil)
				warn("vacfsopen %s: %r", diffvac);
			else
				fdiff = vacfsgetroot(fsdiff);
		}
	}

	if(stdinname)
		vacstdin(f, stdinname);
	for(i=0; i<argc; i++){
		// We can't use / and . and .. and ../.. as valid archive
		// names, so expand to the list of files in the directory.
		if(argv[i][0] == 0){
			warn("empty string given as command-line argument");
			continue;
		}
		cleanname(argv[i]);
		if(strcmp(argv[i], "/") == 0
		|| strcmp(argv[i], ".") == 0
		|| strcmp(argv[i], "..") == 0
		|| (strlen(argv[i]) > 3 && strcmp(argv[i]+strlen(argv[i])-3, "/..") == 0)){
			if((fd = open(argv[i], OREAD)) < 0){
				warn("open %s: %r", argv[i]);
				continue;
			}
			while((n = dirread(fd, &d)) > 0){
				for(j=0; j<n; j++){
					s = vtmalloc(strlen(argv[i])+1+strlen(d[j].name)+1);
					strcpy(s, argv[i]);
					strcat(s, "/");
					strcat(s, d[j].name);
					cleanname(s);
					vac(f, fdiff, s, &d[j]);
				}
				free(d);
			}
			close(fd);
			continue;
		}
		if((d = dirstat(argv[i])) == nil){
			warn("stat %s: %r", argv[i]);
			continue;
		}
		vac(f, fdiff, argv[i], d);
		free(d);
	}
	if(fdiff)
		vacfiledecref(fdiff);

	/*
	 * Record the maximum qid so that vacs can be merged
	 * without introducing overlapping qids.  Older versions
	 * of vac arranged that the root would have the largest
	 * qid in the file system, but we can't do that anymore
	 * (the root gets created first!).
	 */
	if(_vacfsnextqid(fs, &qid) >= 0)
		vacfilesetqidspace(f, 0, qid);
	vacfiledecref(f);

	/*
	 * Copy fsdiff's root block score into fs's slot for that,
	 * so that vacfssync will copy it into root.prev for us.
	 * Just nice documentation, no effect.
	 */
	if(fsdiff)
		memmove(fs->score, fsdiff->score, VtScoreSize);
	if(vacfssync(fs) < 0)
		fprint(2, "vacfssync: %r\n");

	fprint(outfd, "vac:%V\n", fs->score);
	atexitdont(removevacfile);
	vacfsclose(fs);
	vthangup(z);

	if(printstats){
		fprint(2,
			"%d files, %d files skipped, %d directories\n"
			"%lld data bytes written, %lld data bytes skipped\n",
			stats.nfile, stats.skipfiles, stats.ndir, stats.data, stats.skipdata);
		dup(2, 1);
		packetstats();
	}
	threadexitsall(0);
}

VacFile*
recentarchive(VacFs *fs, char *path)
{
	VacFile *fp, *f;
	VacDirEnum *de;
	VacDir vd;
	char buf[10];
	int year, mmdd, nn, n, n1;
	char *p;

	fp = vacfsgetroot(fs);
	de = vdeopen(fp);
	year = 0;
	if(de){
		for(; vderead(de, &vd) > 0; vdcleanup(&vd)){
			if(strlen(vd.elem) != 4)
				continue;
			if((n = strtol(vd.elem, &p, 10)) < 1900 || *p != 0)
				continue;
			if(year < n)
				year = n;
		}
	}
	vdeclose(de);
	if(year == 0){
		vacfiledecref(fp);
		return nil;
	}
	snprint(buf, sizeof buf, "%04d", year);
	if((f = vacfilewalk(fp, buf)) == nil){
		fprint(2, "warning: dirread %s but cannot walk", buf);
		vacfiledecref(fp);
		return nil;
	}
	fp = f;

	de = vdeopen(fp);
	mmdd = 0;
	nn = 0;
	if(de){
		for(; vderead(de, &vd) > 0; vdcleanup(&vd)){
			if(strlen(vd.elem) < 4)
				continue;
			if((n = strtol(vd.elem, &p, 10)) < 100 || n > 1231 || p != vd.elem+4)
				continue;
			if(*p == '.'){
				if(p[1] == '0' || (n1 = strtol(p+1, &p, 10)) == 0 || *p != 0)
					continue;
			}else{
				if(*p != 0)
					continue;
				n1 = 0;
			}
			if(n < mmdd || (n == mmdd && n1 < nn))
				continue;
			mmdd = n;
			nn = n1;
		}
	}
	vdeclose(de);
	if(mmdd == 0){
		vacfiledecref(fp);
		return nil;
	}
	if(nn == 0)
		snprint(buf, sizeof buf, "%04d", mmdd);
	else
		snprint(buf, sizeof buf, "%04d.%d", mmdd, nn);
	if((f = vacfilewalk(fp, buf)) == nil){
		fprint(2, "warning: dirread %s but cannot walk", buf);
		vacfiledecref(fp);
		return nil;
	}
	vacfiledecref(fp);

	sprint(path, "%04d/%s", year, buf);
	return f;
}

static void
removevacfile(void)
{
	if(vacfile)
		remove(vacfile);
}

void
plan9tovacdir(VacDir *vd, Dir *dir)
{
	memset(vd, 0, sizeof *vd);

	vd->elem = dir->name;
	vd->uid = dir->uid;
	vd->gid = dir->gid;
	vd->mid = dir->muid;
	if(vd->mid == nil)
		vd->mid = "";
	vd->mtime = dir->mtime;
	vd->mcount = 0;
	vd->ctime = dir->mtime;		/* ctime: not available on plan 9 */
	vd->atime = dir->atime;
	vd->size = dir->length;

	vd->mode = dir->mode & 0777;
	if(dir->mode & DMDIR)
		vd->mode |= ModeDir;
	if(dir->mode & DMAPPEND)
		vd->mode |= ModeAppend;
	if(dir->mode & DMEXCL)
		vd->mode |= ModeExclusive;
#ifdef PLAN9PORT
	if(dir->mode & DMDEVICE)
		vd->mode |= ModeDevice;
	if(dir->mode & DMNAMEDPIPE)
		vd->mode |= ModeNamedPipe;
	if(dir->mode & DMSYMLINK)
		vd->mode |= ModeLink;
#endif

	vd->plan9 = 1;
	vd->p9path = dir->qid.path;
	vd->p9version = dir->qid.vers;
}

#ifdef PLAN9PORT
enum {
	Special =
		DMSOCKET |
		DMSYMLINK |
		DMNAMEDPIPE |
		DMDEVICE
};
#endif

/*
 * Archive the file named name, which has stat info d,
 * into the vac directory fp (p = parent).
 *
 * If we're doing a vac -d against another archive, the
 * equivalent directory to fp in that archive is diffp.
 */
void
vac(VacFile *fp, VacFile *diffp, char *name, Dir *d)
{
	char *elem, *s;
	static char *buf;
	int fd, i, n, bsize;
	vlong off;
	Dir *dk;	// kids
	VacDir vd, vddiff;
	VacFile *f, *fdiff;
	VtEntry e;

	if(!includefile(name)){
		warn("excluding %s%s", name, (d->mode&DMDIR) ? "/" : "");
		return;
	}

	if(d->mode&DMDIR)
		stats.ndir++;
	else
		stats.nfile++;

	if(merge && vacmerge(fp, name) >= 0)
		return;

	if(verbose)
		fprint(2, "%s%s\n", name, (d->mode&DMDIR) ? "/" : "");

#ifdef PLAN9PORT
	if(d->mode&Special)
		fd = -1;
	else
#endif
	if((fd = open(name, OREAD)) < 0){
		warn("open %s: %r", name);
		return;
	}

	elem = strrchr(name, '/');
	if(elem)
		elem++;
	else
		elem = name;

	plan9tovacdir(&vd, d);
	if((f = vacfilecreate(fp, elem, vd.mode)) == nil){
		warn("vacfilecreate %s: %r", name);
		return;
	}
	if(diffp)
		fdiff = vacfilewalk(diffp, elem);
	else
		fdiff = nil;

	if(vacfilesetdir(f, &vd) < 0)
		warn("vacfilesetdir %s: %r", name);

	bsize = fs->bsize;
	if(buf == nil)
		buf = vtmallocz(bsize);

#ifdef PLAN9PORT
	if(d->mode&(DMSOCKET|DMNAMEDPIPE)){
		/* don't write anything */
	}
	else if(d->mode&DMSYMLINK){
		n = readlink(name, buf, sizeof buf);
		if(n > 0 && vacfilewrite(f, buf, n, 0) < 0){
			warn("venti write %s: %r", name);
			goto Out;
		}
		stats.data += n;
	}else if(d->mode&DMDEVICE){
		snprint(buf, sizeof buf, "%c %d %d",
			(char)((d->qid.path >> 16) & 0xFF),
			(int)(d->qid.path & 0xFF),
			(int)((d->qid.path >> 8) & 0xFF));
		if(vacfilewrite(f, buf, strlen(buf), 0) < 0){
			warn("venti write %s: %r", name);
			goto Out;
		}
		stats.data += strlen(buf);
	}else
#endif
	if(d->mode&DMDIR){
		while((n = dirread(fd, &dk)) > 0){
			for(i=0; i<n; i++){
				s = vtmalloc(strlen(name)+1+strlen(dk[i].name)+1);
				strcpy(s, name);
				strcat(s, "/");
				strcat(s, dk[i].name);
				vac(f, fdiff, s, &dk[i]);
				free(s);
			}
			free(dk);
		}
	}else{
		off = 0;
		if(fdiff){
			/*
			 * Copy fdiff's contents into f by moving the score.
			 * We'll diff and update below.
			 */
			if(vacfilegetentries(fdiff, &e, nil) >= 0)
			if(vacfilesetentries(f, &e, nil) >= 0){
				bsize = e.dsize;

				/*
				 * Or if -q is set, and the metadata looks the same,
				 * don't even bother reading the file.
				 */
				if(qdiff && vacfilegetdir(fdiff, &vddiff) >= 0){
					if(vddiff.mtime == vd.mtime)
					if(vddiff.size == vd.size)
					if(!vddiff.plan9 || (/* vddiff.p9path == vd.p9path && */ vddiff.p9version == vd.p9version)){
						stats.skipfiles++;
						stats.nfile--;
						vdcleanup(&vddiff);
						goto Out;
					}

					/*
					 * Skip over presumably-unchanged prefix
					 * of an append-only file.
					 */
					if(vd.mode&ModeAppend)
					if(vddiff.size < vd.size)
					if(vddiff.plan9 && vd.plan9)
					if(vddiff.p9path == vd.p9path){
						off = vd.size/bsize*bsize;
						if(seek(fd, off, 0) >= 0)
							stats.skipdata += off;
						else{
							seek(fd, 0, 0);	// paranoia
							off = 0;
						}
					}

					vdcleanup(&vddiff);
					// XXX different verbose chatty prints for kaminsky?
				}
			}
		}
		if(qdiff && verbose)
			fprint(2, "+%s\n", name);
		while((n = readn(fd, buf, bsize)) > 0){
			if(fdiff && sha1matches(f, off/bsize, (uchar*)buf, n)){
				off += n;
				stats.skipdata += n;
				continue;
			}
			if(vacfilewrite(f, buf, n, off) < 0){
				warn("venti write %s: %r", name);
				goto Out;
			}
			stats.data += n;
			off += n;
		}
		/*
		 * Since we started with fdiff's contents,
		 * set the size in case fdiff was bigger.
		 */
		if(fdiff && vacfilesetsize(f, off) < 0)
			warn("vtfilesetsize %s: %r", name);
	}

Out:
	vacfileflush(f, 1);
	vacfiledecref(f);
	if(fdiff)
		vacfiledecref(fdiff);
	close(fd);
}

void
vacstdin(VacFile *fp, char *name)
{
	vlong off;
	VacFile *f;
	static char buf[8192];
	int n;

	if((f = vacfilecreate(fp, name, 0666)) == nil){
		warn("vacfilecreate %s: %r", name);
		return;
	}

	off = 0;
	while((n = read(0, buf, sizeof buf)) > 0){
		if(vacfilewrite(f, buf, n, off) < 0){
			warn("venti write %s: %r", name);
			vacfiledecref(f);
			return;
		}
		off += n;
	}
	vacfileflush(f, 1);
	vacfiledecref(f);
}

/*
 * fp is the directory we're writing.
 * mp is the directory whose contents we're merging in.
 * d is the directory entry of the file from mp that we want to add to fp.
 * vacfile is the name of the .vac file, for error messages.
 * offset is the qid that qid==0 in mp should correspond to.
 * max is the maximum qid we expect to see (not really needed).
 */
int
vacmergefile(VacFile *fp, VacFile *mp, VacDir *d, char *vacfile,
	vlong offset, vlong max)
{
	VtEntry ed, em;
	VacFile *mf;
	VacFile *f;

	mf = vacfilewalk(mp, d->elem);
	if(mf == nil){
		warn("could not walk %s in %s", d->elem, vacfile);
		return -1;
	}
	if(vacfilegetentries(mf, &ed, &em) < 0){
		warn("could not get entries for %s in %s", d->elem, vacfile);
		vacfiledecref(mf);
		return -1;
	}

	if((f = vacfilecreate(fp, d->elem, d->mode)) == nil){
		warn("vacfilecreate %s: %r", d->elem);
		vacfiledecref(mf);
		return -1;
	}
	if(d->qidspace){
		d->qidoffset += offset;
		d->qidmax += offset;
	}else{
		d->qidspace = 1;
		d->qidoffset = offset;
		d->qidmax = max;
	}
	if(vacfilesetdir(f, d) < 0
	|| vacfilesetentries(f, &ed, &em) < 0
	|| vacfilesetqidspace(f, d->qidoffset, d->qidmax) < 0){
		warn("vacmergefile %s: %r", d->elem);
		vacfiledecref(mf);
		vacfiledecref(f);
		return -1;
	}

	vacfiledecref(mf);
	vacfiledecref(f);
	return 0;
}

int
vacmerge(VacFile *fp, char *name)
{
	VacFs *mfs;
	VacDir vd;
	VacDirEnum *de;
	VacFile *mp;
	uvlong maxqid, offset;

	if(strlen(name) < 4 || strcmp(name+strlen(name)-4, ".vac") != 0)
		return -1;
	if((mfs = vacfsopen(z, name, VtOREAD, CacheSize)) == nil)
		return -1;
	if(verbose)
		fprint(2, "merging %s\n", name);

	mp = vacfsgetroot(mfs);
	de = vdeopen(mp);
	if(de){
		offset = 0;
		if(vacfsgetmaxqid(mfs, &maxqid) >= 0){
			_vacfsnextqid(fs, &offset);
			vacfsjumpqid(fs, maxqid+1);
		}
		while(vderead(de, &vd) > 0){
			if(vd.qid > maxqid){
				warn("vacmerge %s: maxqid=%lld but %s has %lld",
					name, maxqid, vd.elem, vd.qid);
				vacfsjumpqid(fs, vd.qid - maxqid);
				maxqid = vd.qid;
			}
			vacmergefile(fp, mp, &vd, name,
				offset, maxqid);
			vdcleanup(&vd);
		}
		vdeclose(de);
	}
	vacfiledecref(mp);
	vacfsclose(mfs);
	return 0;
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

static void
warn(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	fprint(2, "vac: ");
	vfprint(2, fmt, arg);
	fprint(2, "\n");
	va_end(arg);
}
