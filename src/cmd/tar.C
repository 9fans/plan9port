/*
 * tar - `tape archiver', actually usable on any medium.
 *	POSIX "ustar" compliant when extracting, and by default when creating.
 *	this tar attempts to read and write multiple Tblock-byte blocks
 *	at once to and from the filesystem, and does not copy blocks
 *	around internally.
 */

#include <u.h>
#include <libc.h>
#include <fcall.h>		/* for %M */
#include <libString.h>

#define creat tar_creat

/*
 * modified versions of those in libc.h; scans only the first arg for
 * keyletters and options.
 */
#define	TARGBEGIN {\
	__fixargv0(); if(!argv0) argv0=*argv; \
	argv++, argc--; \
	if (argv[0]) {\
		char *_args, *_argt;\
		Rune _argc;\
		_args = &argv[0][0];\
		_argc = 0;\
		while(*_args && (_args += chartorune(&_argc, _args)))\
			switch(_argc)
#define	TARGEND	SET(_argt); USED(_argt); USED(_argc); USED(_args); argc--, argv++; } \
	USED(argv);USED(argc); }

#define ROUNDUP(a, b)	(((a) + (b) - 1)/(b))
#define BYTES2TBLKS(bytes) ROUNDUP(bytes, Tblock)

typedef vlong Off;
typedef char *(*Refill)(int ar, char *bufs);

enum { Stdin, Stdout, Stderr };
enum { None, Toc, Xtract, Replace };
enum {
	Tblock = 512,
	Nblock = 40,		/* maximum blocksize */
	Dblock = 20,		/* default blocksize */
	Namsiz = 100,
	Maxpfx = 155,		/* from POSIX */
	Maxname = Namsiz + 1 + Maxpfx,
	DEBUG = 0,
};

/* POSIX link flags */
enum {
	LF_PLAIN1 =	'\0',
	LF_PLAIN2 =	'0',
	LF_LINK =	'1',
	LF_SYMLINK1 =	'2',
	LF_SYMLINK2 =	's',
	LF_CHR =	'3',
	LF_BLK =	'4',
	LF_DIR =	'5',
	LF_FIFO =	'6',
	LF_CONTIG =	'7',
	/* 'A' - 'Z' are reserved for custom implementations */
};

#define islink(lf)	(isreallink(lf) || issymlink(lf))
#define isreallink(lf)	((lf) == LF_LINK)
#define issymlink(lf)	((lf) == LF_SYMLINK1 || (lf) == LF_SYMLINK2)

typedef union {
	char	name[Namsiz];
	char	mode[8];
	char	uid[8];
	char	gid[8];
	char	size[12];
	char	mtime[12];
	char	chksum[8];
	char	linkflag;
	char	linkname[Namsiz];

	/* rest are defined by POSIX's ustar format; see p1003.2b */
	char	magic[6];	/* "ustar" */
	char	version[2];
	char	uname[32];
	char	gname[32];
	char	devmajor[8];
	char	devminor[8];
	char	prefix[Maxpfx]; /* if non-null, path= prefix "/" name */
} Hdr;

int debug;
static int verb;
static int posix = 1;
static int creat;
static int aruid;
static int argid;
static int relative;
static int settime;
static int verbose;

static int nblock = Dblock;
static char *usefile;
static char origdir[Maxname*2];
static Hdr *tpblk, *endblk;
static Hdr *curblk;

static void
usage(void)
{
	fprint(2, "usage: %s {txrc}[pPvRTugf] [archive] file1 file2...\n",
		argv0);
	exits("usage");
}

/*
 * block-buffer management
 */

static void
initblks(void)
{
	free(tpblk);
	tpblk = malloc(Tblock * nblock);
	assert(tpblk != nil);
	endblk = tpblk + nblock;
}

static char *
refill(int ar, char *bufs)
{
	int i, n;
	unsigned bytes = Tblock * nblock;
	static int done, first = 1;

	if (done)
		return nil;

	/* try to size non-pipe input at first read */
	if (first && usefile) {
		first = 0;
		n = read(ar, bufs, bytes);
		if (n <= 0)
			sysfatal("error reading archive: %r");
		i = n;
		if (i % Tblock != 0) {
			fprint(2, "%s: archive block size (%d) error\n",
				argv0, i);
			exits("blocksize");
		}
		i /= Tblock;
		if (i != nblock) {
			nblock = i;
			fprint(2, "%s: blocking = %d\n", argv0, nblock);
			endblk = (Hdr *)bufs + nblock;
			bytes = n;
		}
	} else
		n = readn(ar, bufs, bytes);
	if (n == 0)
		sysfatal("unexpected EOF reading archive");
	else if (n < 0)
		sysfatal("error reading archive: %r");
	else if (n%Tblock != 0)
		sysfatal("partial block read from archive");
	if (n != bytes) {
		done = 1;
		memset(bufs + n, 0, bytes - n);
	}
	return bufs;
}

static Hdr *
getblk(int ar, Refill rfp)
{
	if (curblk == nil || curblk >= endblk) {  /* input block exhausted? */
		if (rfp != nil && (*rfp)(ar, (char *)tpblk) == nil)
			return nil;
		curblk = tpblk;
	}
	return curblk++;
}

static Hdr *
getblkrd(int ar)
{
	return getblk(ar, refill);
}

static Hdr *
getblke(int ar)
{
	return getblk(ar, nil);
}

static Hdr *
getblkz(int ar)
{
	Hdr *hp = getblke(ar);

	if (hp != nil)
		memset(hp, 0, Tblock);
	return hp;
}

/*
 * how many block buffers are available, starting at the address
 * just returned by getblk*?
 */
static int
gothowmany(int max)
{
	int n = endblk - (curblk - 1);

	return n > max? max: n;
}

/*
 * indicate that one is done with the last block obtained from getblke
 * and it is now available to be written into the archive.
 */
static void
putlastblk(int ar)
{
	unsigned bytes = Tblock * nblock;

	/* if writing end-of-archive, aid compression (good hygiene too) */
	if (curblk < endblk)
		memset(curblk, 0, (char *)endblk - (char *)curblk);
	if (write(ar, tpblk, bytes) != bytes)
		sysfatal("error writing archive: %r");
}

static void
putblk(int ar)
{
	if (curblk >= endblk)
		putlastblk(ar);
}

static void
putbackblk(int ar)
{
	curblk--;
	USED(ar);
}

static void
putreadblks(int ar, int blks)
{
	curblk += blks - 1;
	USED(ar);
}

static void
putblkmany(int ar, int blks)
{
	curblk += blks - 1;
	putblk(ar);
}

/*
 * common routines
 */

/* modifies hp->chksum */
long
chksum(Hdr *hp)
{
	int n = Tblock;
	long i = 0;
	uchar *cp = (uchar*)hp;

	memset(hp->chksum, ' ', sizeof hp->chksum);
	while (n-- > 0)
		i += *cp++;
	return i;
}

static int
isustar(Hdr *hp)
{
	return strcmp(hp->magic, "ustar") == 0;
}

/*
 * s is at most n bytes long, but need not be NUL-terminated.
 * if shorter than n bytes, all bytes after the first NUL must also
 * be NUL.
 */
static int
strnlen(char *s, int n)
{
	return s[n - 1] != '\0'? n: strlen(s);
}

/* set fullname from header */
static char *
name(Hdr *hp)
{
	int pfxlen, namlen;
	static char fullname[Maxname + 1];

	namlen = strnlen(hp->name, sizeof hp->name);
	if (hp->prefix[0] == '\0' || !isustar(hp)) {	/* old-style name? */
		memmove(fullname, hp->name, namlen);
		fullname[namlen] = '\0';
		return fullname;
	}

	/* name is in two pieces */
	pfxlen = strnlen(hp->prefix, sizeof hp->prefix);
	memmove(fullname, hp->prefix, pfxlen);
	fullname[pfxlen] = '/';
	memmove(fullname + pfxlen + 1, hp->name, namlen);
	fullname[pfxlen + 1 + namlen] = '\0';
	return fullname;
}

static int
isdir(Hdr *hp)
{
	/* the mode test is ugly but sometimes necessary */
	return hp->linkflag == LF_DIR ||
		strrchr(name(hp), '\0')[-1] == '/' ||
		(strtoul(hp->mode, nil, 8)&0170000) == 040000;
}

static int
eotar(Hdr *hp)
{
	return name(hp)[0] == '\0';
}

static Hdr *
readhdr(int ar)
{
	long hdrcksum;
	Hdr *hp;

	hp = getblkrd(ar);
	if (hp == nil)
		sysfatal("unexpected EOF instead of archive header");
	if (eotar(hp))			/* end-of-archive block? */
		return nil;
	hdrcksum = strtoul(hp->chksum, nil, 8);
	if (chksum(hp) != hdrcksum)
		sysfatal("bad archive header checksum: name %.64s...",
			hp->name);
	return hp;
}

/*
 * tar r[c]
 */

/*
 * if name is longer than Namsiz bytes, try to split it at a slash and fit the
 * pieces into hp->prefix and hp->name.
 */
static int
putfullname(Hdr *hp, char *name)
{
	int namlen, pfxlen;
	char *sl, *osl;
	String *slname = nil;

	if (isdir(hp)) {
		slname = s_new();
		s_append(slname, name);
		s_append(slname, "/");		/* posix requires this */
		name = s_to_c(slname);
	}

	namlen = strlen(name);
	if (namlen <= Namsiz) {
		strncpy(hp->name, name, Namsiz);
		hp->prefix[0] = '\0';		/* ustar paranoia */
		return 0;
	}

	if (!posix || namlen > Maxname) {
		fprint(2, "%s: name too long for tar header: %s\n",
			argv0, name);
		return -1;
	}
	/*
	 * try various splits until one results in pieces that fit into the
	 * appropriate fields of the header.  look for slashes from right
	 * to left, in the hopes of putting the largest part of the name into
	 * hp->prefix, which is larger than hp->name.
	 */
	sl = strrchr(name, '/');
	while (sl != nil) {
		pfxlen = sl - name;
		if (pfxlen <= sizeof hp->prefix && namlen-1 - pfxlen <= Namsiz)
			break;
		osl = sl;
		*osl = '\0';
		sl = strrchr(name, '/');
		*osl = '/';
	}
	if (sl == nil) {
		fprint(2, "%s: name can't be split to fit tar header: %s\n",
			argv0, name);
		return -1;
	}
	*sl = '\0';
	strncpy(hp->prefix, name, sizeof hp->prefix);
	*sl++ = '/';
	strncpy(hp->name, sl, sizeof hp->name);
	if (slname)
		s_free(slname);
	return 0;
}

static int
mkhdr(Hdr *hp, Dir *dir, char *file)
{
	/*
	 * these fields run together, so we format them in order and don't use
	 * snprint.
	 */
	sprint(hp->mode, "%6lo ", dir->mode & 0777);
	sprint(hp->uid, "%6o ", aruid);
	sprint(hp->gid, "%6o ", argid);
	/*
	 * files > 2⁳⁳ bytes can't be described
	 * (unless we resort to xustar or exustar formats).
	 */
	if (dir->length >= (Off)1<<33) {
		fprint(2, "%s: %s: too large for tar header format\n",
			argv0, file);
		return -1;
	}
	sprint(hp->size, "%11lluo ", dir->length);
	sprint(hp->mtime, "%11luo ", dir->mtime);
	hp->linkflag = (dir->mode&DMDIR? LF_DIR: LF_PLAIN1);
	putfullname(hp, file);
	if (posix) {
		strncpy(hp->magic, "ustar", sizeof hp->magic);
		strncpy(hp->version, "00", sizeof hp->version);
		strncpy(hp->uname, dir->uid, sizeof hp->uname);
		strncpy(hp->gname, dir->gid, sizeof hp->gname);
	}
	sprint(hp->chksum, "%6luo", chksum(hp));
	return 0;
}

static void addtoar(int ar, char *file, char *shortf);

static void
addtreetoar(int ar, char *file, char *shortf, int fd)
{
	int n;
	Dir *dent, *dirents;
	String *name = s_new();

	n = dirreadall(fd, &dirents);
	close(fd);
	if (n == 0)
		return;

	if (chdir(shortf) < 0)
		sysfatal("chdir %s: %r", file);
	if (DEBUG)
		fprint(2, "chdir %s\t# %s\n", shortf, file);

	for (dent = dirents; dent < dirents + n; dent++) {
		s_reset(name);
		s_append(name, file);
		s_append(name, "/");
		s_append(name, dent->name);
		addtoar(ar, s_to_c(name), dent->name);
	}
	s_free(name);
	free(dirents);

	if (chdir("..") < 0)
		sysfatal("chdir %s: %r", file);
	if (DEBUG)
		fprint(2, "chdir ..\n");
}

static void
addtoar(int ar, char *file, char *shortf)
{
	int n, fd, isdir, r;
	long bytes;
	ulong blksleft, blksread;
	Hdr *hbp;
	Dir *dir;

	fd = open(shortf, OREAD);
	if (fd < 0) {
		fprint(2, "%s: can't open %s: %r\n", argv0, file);
		return;
	}
	dir = dirfstat(fd);
	if (dir == nil)
		sysfatal("can't fstat %s: %r", file);

	hbp = getblkz(ar);
	isdir = !!(dir->qid.type&QTDIR);
	r = mkhdr(hbp, dir, file);
	if (r < 0) {
		putbackblk(ar);
		free(dir);
		close(fd);
		return;
	}
	putblk(ar);

	blksleft = BYTES2TBLKS(dir->length);
	free(dir);

	if (isdir)
		addtreetoar(ar, file, shortf, fd);
	else {
		for (; blksleft > 0; blksleft -= blksread) {
			hbp = getblke(ar);
			blksread = gothowmany(blksleft);
			bytes = blksread * Tblock;
			n = read(fd, hbp, bytes);
			if (n < 0)
				sysfatal("error reading %s: %r", file);
			/*
			 * ignore EOF.  zero any partial block to aid
			 * compression and emergency recovery of data.
			 */
			if (n < Tblock)
				memset((char*)hbp + n, 0, bytes - n);
			putblkmany(ar, blksread);
		}
		close(fd);
		if (verbose)
			fprint(2, "%s: a %s\n", argv0, file);
	}
}

static void
replace(char **argv)
{
	int i, ar;
	ulong blksleft, blksread;
	Off bytes;
	Hdr *hp;

	if (usefile && creat)
		ar = create(usefile, ORDWR, 0666);
	else if (usefile)
		ar = open(usefile, ORDWR);
	else
		ar = Stdout;
	if (ar < 0)
		sysfatal("can't open archive %s: %r", usefile);

	if (usefile && !creat) {
		/* skip quickly to the end */
		while ((hp = readhdr(ar)) != nil) {
			bytes = (isdir(hp)? 0: strtoull(hp->size, nil, 8));
			for (blksleft = BYTES2TBLKS(bytes);
			     blksleft > 0 && getblkrd(ar) != nil;
			     blksleft -= blksread) {
				blksread = gothowmany(blksleft);
				putreadblks(ar, blksread);
			}
		}
		/*
		 * we have just read the end-of-archive Tblock.
		 * now seek back over the (big) archive block containing it,
		 * and back up curblk ptr over end-of-archive Tblock in memory.
		 */
		if (seek(ar, -Tblock*nblock, 1) < 0)
			sysfatal("can't seek back over end-of-archive: %r");
		curblk--;
	}

	for (i = 0; argv[i] != nil; i++)
		addtoar(ar, argv[i], argv[i]);

	/* write end-of-archive marker */
	getblkz(ar);
	putblk(ar);
	getblkz(ar);
	putlastblk(ar);

	if (ar > Stderr)
		close(ar);
}

/*
 * tar [xt]
 */

/* is pfx a file-name prefix of name? */
static int
prefix(char *name, char *pfx)
{
	int pfxlen = strlen(pfx);
	char clpfx[Maxname+1];

	if (pfxlen > Maxname)
		return 0;
	strcpy(clpfx, pfx);
	cleanname(clpfx);
	return strncmp(pfx, name, pfxlen) == 0 &&
		(name[pfxlen] == '\0' || name[pfxlen] == '/');
}

static int
match(char *name, char **argv)
{
	int i;
	char clname[Maxname+1];

	if (argv[0] == nil)
		return 1;
	strcpy(clname, name);
	cleanname(clname);
	for (i = 0; argv[i] != nil; i++)
		if (prefix(clname, argv[i]))
			return 1;
	return 0;
}

static int
makedir(char *s)
{
	int f;

	if (access(s, AEXIST) == 0)
		return -1;
	f = create(s, OREAD, DMDIR | 0777);
	if (f >= 0)
		close(f);
	return f;
}

static void
mkpdirs(char *s)
{
	int done = 0;
	char *p = s;

	while (!done && (p = strchr(p + 1, '/')) != nil) {
		*p = '\0';
		done = (access(s, AEXIST) < 0 && makedir(s) < 0);
		*p = '/';
	}
}

/* copy a file from the archive into the filesystem */
static void
extract1(int ar, Hdr *hp, char *fname)
{
	int wrbytes, fd = -1, dir = 0, okcreate = 1;
	long mtime = strtol(hp->mtime, nil, 8);
	ulong mode = strtoul(hp->mode, nil, 8) & 0777;
	Off bytes = strtoll(hp->size, nil, 8);
	ulong blksread, blksleft = BYTES2TBLKS(bytes);
	Hdr *hbp;

	if (isdir(hp)) {
		mode |= DMDIR|0700;
		blksleft = 0;
		dir = 1;
	}
	switch (hp->linkflag) {
	case LF_LINK:
	case LF_SYMLINK1:
	case LF_SYMLINK2:
	case LF_FIFO:
		blksleft = okcreate = 0;
		break;
	}
	if (relative && fname[0] == '/')
		fname++;
	if (verb == Xtract) {
		cleanname(fname);
		switch (hp->linkflag) {
		case LF_LINK:
		case LF_SYMLINK1:
		case LF_SYMLINK2:
			fprint(2, "%s: can't make (sym)link %s\n",
				argv0, fname);
			break;
		case LF_FIFO:
			fprint(2, "%s: can't make fifo %s\n", argv0, fname);
			break;
		}
		if (okcreate)
			fd = create(fname, (dir? OREAD: OWRITE), mode);
		if (fd < 0) {
			mkpdirs(fname);
			fd = create(fname, (dir? OREAD: OWRITE), mode);
			if (fd < 0 && (!dir || access(fname, AEXIST) < 0))
				fprint(2, "%s: can't create %s: %r\n",
					argv0, fname);
		}
		if (fd >= 0 && verbose)
			fprint(2, "%s: x %s\n", argv0, fname);
	} else if (verbose) {
		char *cp = ctime(mtime);

		print("%M %8lld %-12.12s %-4.4s %s\n",
			mode, bytes, cp+4, cp+24, fname);
	} else
		print("%s\n", fname);

	for (; blksleft > 0; blksleft -= blksread) {
		hbp = getblkrd(ar);
		if (hbp == nil)
			sysfatal("unexpected EOF on archive extracting %s",
				fname);
		blksread = gothowmany(blksleft);
		wrbytes = (bytes >= Tblock*blksread? Tblock*blksread: bytes);
		if (fd >= 0 && write(fd, (char*)hbp, wrbytes) != wrbytes)
			sysfatal("write error on %s: %r", fname);
		putreadblks(ar, blksread);
		bytes -= wrbytes;
	}
	if (fd >= 0) {
		/*
		 * directories should be wstated after we're done
		 * creating files in them.
		 */
		if (settime) {
			Dir nd;

			nulldir(&nd);
			nd.mtime = mtime;
			if (isustar(hp))
				nd.gid = hp->gname;
			dirfwstat(fd, &nd);
		}
		close(fd);
	}
}

static void
skip(int ar, Hdr *hp, char *fname)
{
	Off bytes;
	ulong blksleft, blksread;
	Hdr *hbp;

	if (isdir(hp))
		return;
	bytes = strtoull(hp->size, nil, 8);
	blksleft = BYTES2TBLKS(bytes);
	for (; blksleft > 0; blksleft -= blksread) {
		hbp = getblkrd(ar);
		if (hbp == nil)
			sysfatal("unexpected EOF on archive extracting %s",
				fname);
		blksread = gothowmany(blksleft);
		putreadblks(ar, blksread);
	}
}

static void
extract(char **argv)
{
	int ar;
	char *longname;
	Hdr *hp;

	if (usefile)
		ar = open(usefile, OREAD);
	else
		ar = Stdin;
	if (ar < 0)
		sysfatal("can't open archive %s: %r", usefile);
	while ((hp = readhdr(ar)) != nil) {
		longname = name(hp);
		if (match(longname, argv))
			extract1(ar, hp, longname);
		else
			skip(ar, hp, longname);
	}
	if (ar > Stderr)
		close(ar);
}

void
main(int argc, char *argv[])
{
	int errflg = 0;
	char *ret = nil;

	quotefmtinstall();
	fmtinstall('M', dirmodefmt);

	TARGBEGIN {
	case 'c':
		creat++;
		verb = Replace;
		break;
	case 'f':
		usefile = EARGF(usage());
		break;
	case 'g':
		argid = strtoul(EARGF(usage()), 0, 0);
		break;
	case 'p':
		posix++;
		break;
	case 'P':
		posix = 0;
		break;
	case 'r':
		verb = Replace;
		break;
	case 'R':
		relative++;
		break;
	case 't':
		verb = Toc;
		break;
	case 'T':
		settime++;
		break;
	case 'u':
		aruid = strtoul(EARGF(usage()), 0, 0);
		break;
	case 'v':
		verbose++;
		break;
	case 'x':
		verb = Xtract;
		break;
	case '-':
		break;
	default:
		errflg++;
		break;
	} TARGEND

	if (argc < 0 || errflg)
		usage();

	initblks();
	switch (verb) {
	case Toc:
	case Xtract:
		extract(argv);
		break;
	case Replace:
		if (getwd(origdir, sizeof origdir) == nil)
			strcpy(origdir, "/tmp");
		replace(argv);
		chdir(origdir);		/* for profiling */
		break;
	default:
		usage();
		break;
	}
	exits(ret);
}
