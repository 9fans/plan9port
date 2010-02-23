#include <u.h>
#include <libc.h>
#include <bio.h>
#include <flate.h>
#include "zip.h"

enum
{
	BufSize	= 4096
};

static	int	cheader(Biobuf *bin, ZipHead *zh);
static	int	copyout(int ofd, Biobuf *bin, long len);
static	int	crcwrite(void *ofd, void *buf, int n);
static	int	findCDir(Biobuf *bin, char *file);
static	int	get1(Biobuf *b);
static	int	get2(Biobuf *b);
static	u32int	get4(Biobuf *b);
static	char	*getname(Biobuf *b, int len);
static	int	header(Biobuf *bin, ZipHead *zh);
static	long	msdos2time(int time, int date);
static	int	sunzip(Biobuf *bin);
static	int	sunztable(Biobuf *bin);
static	void	trailer(Biobuf *bin, ZipHead *zh);
static	int	unzip(Biobuf *bin, char *file);
static	int	unzipEntry(Biobuf *bin, ZipHead *czh);
static	int	unztable(Biobuf *bin, char *file);
static	int	wantFile(char *file);

static	void	*emalloc(u32int);
static	void	error(char*, ...);
/* #pragma	varargck	argpos	error	1 */

static	Biobuf	bin;
static	u32int	crc;
static	u32int	*crctab;
static	int	debug;
static	char	*delfile;
static	int	lower;
static	int	nwant;
static	u32int	rlen;
static	int	settimes;
static	int	stdout;
static	int	verbose;
static	char	**want;
static	int	wbad;
static	u32int	wlen;
static	jmp_buf	zjmp;

static void
usage(void)
{
	fprint(2, "usage: unzip [-tsv] [-f zipfile] [file ...]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	char *zfile;
	int fd, ok, table, stream;

	table = 0;
	stream = 0;
	zfile = nil;
	ARGBEGIN{
	case 'D':
		debug++;
		break;
	case 'c':
		stdout++;
		break;
	case 'i':
		lower++;
		break;
	case 'f':
		zfile = ARGF();
		if(zfile == nil)
			usage();
		break;
	case 's':
		stream++;
		break;
	case 't':
		table++;
		break;
	case 'T':
		settimes++;
		break;
	case 'v':
		verbose++;
		break;
	default:
		usage();
		break;
	}ARGEND

	nwant = argc;
	want = argv;

	crctab = mkcrctab(ZCrcPoly);
	ok = inflateinit();
	if(ok != FlateOk)
		sysfatal("inflateinit failed: %s\n", flateerr(ok));

	if(zfile == nil){
		Binit(&bin, 0, OREAD);
		zfile = "<stdin>";
	}else{
		fd = open(zfile, OREAD);
		if(fd < 0)
			sysfatal("can't open %s: %r", zfile);
		Binit(&bin, fd, OREAD);
	}

	if(table){
		if(stream)
			ok = sunztable(&bin);
		else
			ok = unztable(&bin, zfile);
	}else{
		if(stream)
			ok = sunzip(&bin);
		else
			ok = unzip(&bin, zfile);
	}

	exits(ok ? nil: "errors");
}

/*
 * print the table of contents from the "central directory structure"
 */
static int
unztable(Biobuf *bin, char *file)
{
	ZipHead zh;
	int volatile entries;

	entries = findCDir(bin, file);
	if(entries < 0)
		return 0;

	if(verbose > 1)
		print("%d items in the archive\n", entries);
	while(entries-- > 0){
		if(setjmp(zjmp)){
			free(zh.file);
			return 0;
		}

		memset(&zh, 0, sizeof(zh));
		if(!cheader(bin, &zh))
			return 1;

		if(wantFile(zh.file)){
			if(verbose)
				print("%-32s %10lud %s", zh.file, zh.uncsize, ctime(msdos2time(zh.modtime, zh.moddate)));
			else
				print("%s\n", zh.file);

			if(verbose > 1){
				print("\tmade by os %d vers %d.%d\n", zh.madeos, zh.madevers/10, zh.madevers % 10);
				print("\textract by os %d vers %d.%d\n", zh.extos, zh.extvers/10, zh.extvers % 10);
				print("\tflags %x\n", zh.flags);
				print("\tmethod %d\n", zh.meth);
				print("\tmod time %d\n", zh.modtime);
				print("\tmod date %d\n", zh.moddate);
				print("\tcrc %lux\n", zh.crc);
				print("\tcompressed size %lud\n", zh.csize);
				print("\tuncompressed size %lud\n", zh.uncsize);
				print("\tinternal attributes %ux\n", zh.iattr);
				print("\texternal attributes %lux\n", zh.eattr);
				print("\tstarts at %ld\n", zh.off);
			}
		}

		free(zh.file);
		zh.file = nil;
	}

	return 1;
}

/*
 * print the "local file header" table of contents
 */
static int
sunztable(Biobuf *bin)
{
	ZipHead zh;
	vlong off;
	u32int hcrc, hcsize, huncsize;
	int ok, err;

	ok = 1;
	for(;;){
		if(setjmp(zjmp)){
			free(zh.file);
			return 0;
		}

		memset(&zh, 0, sizeof(zh));
		if(!header(bin, &zh))
			return ok;

		hcrc = zh.crc;
		hcsize = zh.csize;
		huncsize = zh.uncsize;

		wlen = 0;
		rlen = 0;
		crc = 0;
		wbad = 0;

		if(zh.meth == 0){
			if(!copyout(-1, bin, zh.csize))
				error("reading data for %s failed: %r", zh.file);
		}else if(zh.meth == 8){
			off = Boffset(bin);
			err = inflate((void*)-1, crcwrite, bin, (int(*)(void*))Bgetc);
			if(err != FlateOk)
				error("inflate %s failed: %s", zh.file, flateerr(err));
			rlen = Boffset(bin) - off;
		}else
			error("can't handle compression method %d for %s", zh.meth, zh.file);

		trailer(bin, &zh);

		if(wantFile(zh.file)){
			if(verbose)
				print("%-32s %10lud %s", zh.file, zh.uncsize, ctime(msdos2time(zh.modtime, zh.moddate)));
			else
				print("%s\n", zh.file);

			if(verbose > 1){
				print("\textract by os %d vers %d.%d\n", zh.extos, zh.extvers / 10, zh.extvers % 10);
				print("\tflags %x\n", zh.flags);
				print("\tmethod %d\n", zh.meth);
				print("\tmod time %d\n", zh.modtime);
				print("\tmod date %d\n", zh.moddate);
				print("\tcrc %lux\n", zh.crc);
				print("\tcompressed size %lud\n", zh.csize);
				print("\tuncompressed size %lud\n", zh.uncsize);
				if((zh.flags & ZTrailInfo) && (hcrc || hcsize || huncsize)){
					print("\theader crc %lux\n", zh.crc);
					print("\theader compressed size %lud\n", zh.csize);
					print("\theader uncompressed size %lud\n", zh.uncsize);
				}
			}
		}

		if(zh.crc != crc)
			error("crc mismatch for %s", zh.file);
		if(zh.uncsize != wlen)
			error("output size mismatch for %s", zh.file);
		if(zh.csize != rlen)
			error("input size mismatch for %s", zh.file);


		free(zh.file);
		zh.file = nil;
	}
}

/*
 * extract files using the info in the central directory structure
 */
static int
unzip(Biobuf *bin, char *file)
{
	ZipHead zh;
	vlong off;
	int volatile ok, eok, entries;

	entries = findCDir(bin, file);
	if(entries < 0)
		return 0;

	ok = 1;
	while(entries-- > 0){
		if(setjmp(zjmp)){
			free(zh.file);
			return 0;
		}
		memset(&zh, 0, sizeof(zh));
		if(!cheader(bin, &zh))
			return ok;


		off = Boffset(bin);
		if(wantFile(zh.file)){
			if(Bseek(bin, zh.off, 0) < 0){
				fprint(2, "unzip: can't seek to start of %s, skipping\n", zh.file);
				ok = 0;
			}else{
				eok = unzipEntry(bin, &zh);
				if(eok <= 0){
					fprint(2, "unzip: skipping %s\n", zh.file);
					ok = 0;
				}
			}
		}

		free(zh.file);
		zh.file = nil;

		if(Bseek(bin, off, 0) < 0){
			fprint(2, "unzip: can't seek to start of next entry, terminating extraction\n");
			return 0;
		}
	}

	return ok;
}

/*
 * extract files using the info the "local file headers"
 */
static int
sunzip(Biobuf *bin)
{
	int eok;

	for(;;){
		eok = unzipEntry(bin, nil);
		if(eok == 0)
			return 1;
		if(eok < 0)
			return 0;
	}
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

/*
 * extracts a single entry from a zip file
 * czh is the optional corresponding central directory entry
 */
static int
unzipEntry(Biobuf *bin, ZipHead *czh)
{
	Dir *d;
	ZipHead zh;
	char *p;
	vlong off;
	int fd, isdir, ok, err;

	zh.file = nil;
	if(setjmp(zjmp)){
		delfile = nil;
		free(zh.file);
		return -1;
	}

	memset(&zh, 0, sizeof(zh));
	if(!header(bin, &zh))
		return 0;

	ok = 1;
	isdir = 0;

	fd = -1;
	if(wantFile(zh.file)){
		if(verbose)
			fprint(2, "extracting %s\n", zh.file);

		if(czh != nil && czh->extos == ZDos){
			isdir = czh->eattr & ZDDir;
			if(isdir && zh.uncsize != 0)
				fprint(2, "unzip: ignoring directory data for %s\n", zh.file);
		}
		if(zh.meth == 0 && zh.uncsize == 0){
			p = strchr(zh.file, '\0');
			if(p > zh.file && p[-1] == '/')
				isdir = 1;
		}

		if(stdout){
			if(ok && !isdir)
				fd = 1;
		}else if(isdir){
			fd = create(zh.file, OREAD, DMDIR | 0775);
			if(fd < 0){
				mkpdirs(zh.file);
				fd = create(zh.file, OREAD, DMDIR | 0775);
			}
			if(fd < 0){
				d = dirstat(zh.file);
				if(d == nil || (d->mode & DMDIR) != DMDIR){
					fprint(2, "unzip: can't create directory %s: %r\n", zh.file);
					ok = 0;
				}
				free(d);
			}
		}else if(ok){
			fd = create(zh.file, OWRITE, 0664);
			if(fd < 0){
				mkpdirs(zh.file);
				fd = create(zh.file, OWRITE, 0664);
			}
			if(fd < 0){
				fprint(2, "unzip: can't create %s: %r\n", zh.file);
				ok = 0;
			}else
				delfile = zh.file;
		}
	}

	wlen = 0;
	rlen = 0;
	crc = 0;
	wbad = 0;

	if(zh.meth == 0){
		if(!copyout(fd, bin, zh.csize))
			error("copying data for %s failed: %r", zh.file);
	}else if(zh.meth == 8){
		off = Boffset(bin);
		err = inflate((void*)(uintptr)fd, crcwrite, bin, (int(*)(void*))Bgetc);
		if(err != FlateOk)
			error("inflate failed: %s", flateerr(err));
		rlen = Boffset(bin) - off;
	}else
		error("can't handle compression method %d for %s", zh.meth, zh.file);

	trailer(bin, &zh);

	if(zh.crc != crc)
		error("crc mismatch for %s", zh.file);
	if(zh.uncsize != wlen)
		error("output size mismatch for %s", zh.file);
	if(zh.csize != rlen)
		error("input size mismatch for %s", zh.file);

	delfile = nil;
	free(zh.file);

	if(fd >= 0 && !stdout){
		if(settimes){
			d = dirfstat(fd);
			if(d != nil){
				d->mtime = msdos2time(zh.modtime, zh.moddate);
				if(d->mtime)
					dirfwstat(fd, d);
			}
		}
		close(fd);
	}

	return ok;
}

static int
wantFile(char *file)
{
	int i, n;

	if(nwant == 0)
		return 1;
	for(i = 0; i < nwant; i++){
		if(strcmp(want[i], file) == 0)
			return 1;
		n = strlen(want[i]);
		if(strncmp(want[i], file, n) == 0 && file[n] == '/')
			return 1;
	}
	return 0;
}

/*
 * find the start of the central directory
 * returns the number of entries in the directory,
 * or -1 if there was an error
 */
static int
findCDir(Biobuf *bin, char *file)
{
	vlong ecoff;
	long off, size;
	int entries, zclen, dn, ds, de;

	ecoff = Bseek(bin, -ZECHeadSize, 2);
	if(ecoff < 0){
		fprint(2, "unzip: can't seek to contents of %s; try adding -s\n", file);
		return -1;
	}
	if(setjmp(zjmp))
		return -1;

	if(get4(bin) != ZECHeader){
		fprint(2, "unzip: bad magic number for contents of %s\n", file);
		return -1;
	}
	dn = get2(bin);
	ds = get2(bin);
	de = get2(bin);
	entries = get2(bin);
	size = get4(bin);
	off = get4(bin);
	zclen = get2(bin);
	while(zclen-- > 0)
		get1(bin);

	if(verbose > 1){
		print("table starts at %ld for %ld bytes\n", off, size);
		if(ecoff - size != off)
			print("\ttable should start at %lld-%ld=%lld\n", ecoff, size, ecoff-size);
		if(dn || ds || de != entries)
			print("\tcurrent disk=%d start disk=%d table entries on this disk=%d\n", dn, ds, de);
	}

	if(Bseek(bin, off, 0) != off){
		fprint(2, "unzip: can't seek to start of contents of %s\n", file);
		return -1;
	}

	return entries;
}

static int
cheader(Biobuf *bin, ZipHead *zh)
{
	u32int v;
	int flen, xlen, fclen;

	v = get4(bin);
	if(v != ZCHeader){
		if(v == ZECHeader)
			return 0;
		error("bad magic number %lux", v);
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
	zh->iattr = get2(bin);
	zh->eattr = get4(bin);
	zh->off = get4(bin);

	zh->file = getname(bin, flen);

	while(xlen-- > 0)
		get1(bin);

	while(fclen-- > 0)
		get1(bin);

	return 1;
}

static int
header(Biobuf *bin, ZipHead *zh)
{
	u32int v;
	int flen, xlen;

	v = get4(bin);
	if(v != ZHeader){
		if(v == ZCHeader)
			return 0;
		error("bad magic number %lux at %lld", v, Boffset(bin)-4);
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

static void
trailer(Biobuf *bin, ZipHead *zh)
{
	if(zh->flags & ZTrailInfo){
		zh->crc = get4(bin);
		zh->csize = get4(bin);
		zh->uncsize = get4(bin);
	}
}

static char*
getname(Biobuf *bin, int len)
{
	char *s;
	int i, c;

	s = emalloc(len + 1);
	for(i = 0; i < len; i++){
		c = get1(bin);
		if(lower)
			c = tolower(c);
		s[i] = c;
	}
	s[i] = '\0';
	return s;
}

static int
crcwrite(void *out, void *buf, int n)
{
	int fd, nw;

	wlen += n;
	crc = blockcrc(crctab, crc, buf, n);
	fd = (int)(uintptr)out;
	if(fd < 0)
		return n;
	nw = write(fd, buf, n);
	if(nw != n)
		wbad = 1;
	return nw;
}

static int
copyout(int ofd, Biobuf *bin, long len)
{
	char buf[BufSize];
	int n;

	for(; len > 0; len -= n){
		n = len;
		if(n > BufSize)
			n = BufSize;
		n = Bread(bin, buf, n);
		if(n <= 0)
			return 0;
		rlen += n;
		if(crcwrite((void*)(uintptr)ofd, buf, n) != n)
			return 0;
	}
	return 1;
}

static u32int
get4(Biobuf *b)
{
	u32int v;
	int i, c;

	v = 0;
	for(i = 0; i < 4; i++){
		c = Bgetc(b);
		if(c < 0)
			error("unexpected eof reading file information");
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
			error("unexpected eof reading file information");
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
		error("unexpected eof reading file information");
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

static void*
emalloc(u32int n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("out of memory");
	return p;
}

static void
error(char *fmt, ...)
{
	va_list arg;

	fprint(2, "unzip: ");
	va_start(arg, fmt);
	vfprint(2, fmt, arg);
	va_end(arg);
	fprint(2, "\n");

	if(delfile != nil){
		fprint(2, "unzip: removing output file %s\n", delfile);
		remove(delfile);
		delfile = nil;
	}

	longjmp(zjmp, 1);
}
