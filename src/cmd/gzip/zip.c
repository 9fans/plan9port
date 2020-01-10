#include <u.h>
#include <libc.h>
#include <bio.h>
#include <flate.h>
#include "zip.h"

enum
{
	HeadAlloc	= 64
};

static	void	zip(Biobuf *bout, char *file, int stdout);
static	void	zipDir(Biobuf *bout, int fd, ZipHead *zh, int stdout);
static	int	crcread(void *fd, void *buf, int n);
static	int	zwrite(void *bout, void *buf, int n);
static	void	put4(Biobuf *b, u32int v);
static	void	put2(Biobuf *b, int v);
static	void	put1(Biobuf *b, int v);
static	void	header(Biobuf *bout, ZipHead *zh);
static	void	trailer(Biobuf *bout, ZipHead *zh, vlong off);
static	void	putCDir(Biobuf *bout);

static	void	error(char*, ...);
/* #pragma	varargck	argpos	error	1 */

static	Biobuf	bout;
static	u32int	crc;
static	u32int	*crctab;
static	int	debug;
static	int	eof;
static	int	level;
static	int	nzheads;
static	u32int	totr;
static	u32int	totw;
static	int	verbose;
static	int	zhalloc;
static	ZipHead	*zheads;
static	jmp_buf	zjmp;

void
usage(void)
{
	fprint(2, "usage: zip [-vD] [-1-9] [-f zipfile] file ...\n");
	exits("usage");
}

void
main(int volatile argc, char **volatile argv)
{
	char *volatile zfile;
	int i, fd, err;

	zfile = nil;
	level = 6;
	ARGBEGIN{
	case 'D':
		debug++;
		break;
	case 'f':
		zfile = ARGF();
		if(zfile == nil)
			usage();
		break;
	case 'v':
		verbose++;
		break;
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		level = ARGC() - '0';
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc == 0)
		usage();

	crctab = mkcrctab(ZCrcPoly);
	err = deflateinit();
	if(err != FlateOk)
		sysfatal("deflateinit failed: %s\n", flateerr(err));

	if(zfile == nil)
		fd = 1;
	else{
		fd = create(zfile, OWRITE, 0664);
		if(fd < 0)
			sysfatal("can't create %s: %r\n", zfile);
	}
	Binit(&bout, fd, OWRITE);

	if(setjmp(zjmp)){
		if(zfile != nil){
			fprint(2, "zip: removing output file %s\n", zfile);
			remove(zfile);
		}
		exits("errors");
	}

	for(i = 0; i < argc; i++)
		zip(&bout, argv[i], zfile == nil);

	putCDir(&bout);

	exits(nil);
}

static void
zip(Biobuf *bout, char *file, int stdout)
{
	Tm *t;
	ZipHead *zh;
	Dir *dir;
	vlong off;
	int fd, err;

	fd = open(file, OREAD);
	if(fd < 0)
		error("can't open %s: %r", file);
	dir = dirfstat(fd);
	if(dir == nil)
		error("can't stat %s: %r", file);

	/*
	 * create the header
	 */
	if(nzheads >= zhalloc){
		zhalloc += HeadAlloc;
		zheads = realloc(zheads, zhalloc * sizeof(ZipHead));
		if(zheads == nil)
			error("out of memory");
	}
	zh = &zheads[nzheads++];
	zh->madeos = ZDos;
	zh->madevers = (2 * 10) + 0;
	zh->extos = ZDos;
	zh->extvers = (2 * 10) + 0;

	t = localtime(dir->mtime);
	zh->modtime = (t->hour<<11) | (t->min<<5) | (t->sec>>1);
	zh->moddate = ((t->year-80)<<9) | ((t->mon+1)<<5) | t->mday;

	zh->flags = 0;
	zh->crc = 0;
	zh->csize = 0;
	zh->uncsize = 0;
	zh->file = strdup(file);
	if(zh->file == nil)
		error("out of memory");
	zh->iattr = 0;
	zh->eattr = ZDArch;
	if((dir->mode & 0700) == 0)
		zh->eattr |= ZDROnly;
	zh->off = Boffset(bout);

	if(dir->mode & DMDIR){
		zh->eattr |= ZDDir;
		zh->meth = 0;
		zipDir(bout, fd, zh, stdout);
	}else{
		zh->meth = 8;
		if(stdout)
			zh->flags |= ZTrailInfo;
		off = Boffset(bout);
		header(bout, zh);

		crc = 0;
		eof = 0;
		totr = 0;
		totw = 0;
		err = deflate(bout, zwrite, (void*)(uintptr)fd, crcread, level, debug);
		if(err != FlateOk)
			error("deflate failed: %s: %r", flateerr(err));

		zh->csize = totw;
		zh->uncsize = totr;
		zh->crc = crc;
		trailer(bout, zh, off);
	}
	close(fd);
	free(dir);
}

static void
zipDir(Biobuf *bout, int fd, ZipHead *zh, int stdout)
{
	Dir *dirs;
	char *file, *pfile;
	int i, nf, nd;

	nf = strlen(zh->file) + 1;
	if(strcmp(zh->file, ".") == 0){
		nzheads--;
		free(zh->file);
		pfile = "";
		nf = 1;
	}else{
		nf++;
		pfile = malloc(nf);
		if(pfile == nil)
			error("out of memory");
		snprint(pfile, nf, "%s/", zh->file);
		free(zh->file);
		zh->file = pfile;
		header(bout, zh);
	}

	nf += 256;	/* plenty of room */
	file = malloc(nf);
	if(file == nil)
		error("out of memory");
	while((nd = dirread(fd, &dirs)) > 0){
		for(i = 0; i < nd; i++){
			snprint(file, nf, "%s%s", pfile, dirs[i].name);
			zip(bout, file, stdout);
		}
		free(dirs);
	}
}

static void
header(Biobuf *bout, ZipHead *zh)
{
	int flen;

	if(verbose)
		fprint(2, "adding %s\n", zh->file);
	put4(bout, ZHeader);
	put1(bout, zh->extvers);
	put1(bout, zh->extos);
	put2(bout, zh->flags);
	put2(bout, zh->meth);
	put2(bout, zh->modtime);
	put2(bout, zh->moddate);
	put4(bout, zh->crc);
	put4(bout, zh->csize);
	put4(bout, zh->uncsize);
	flen = strlen(zh->file);
	put2(bout, flen);
	put2(bout, 0);
	if(Bwrite(bout, zh->file, flen) != flen)
		error("write error");
}

static void
trailer(Biobuf *bout, ZipHead *zh, vlong off)
{
	vlong coff;

	coff = -1;
	if(!(zh->flags & ZTrailInfo)){
		coff = Boffset(bout);
		if(Bseek(bout, off + ZHeadCrc, 0) < 0)
			error("can't seek in archive");
	}
	put4(bout, zh->crc);
	put4(bout, zh->csize);
	put4(bout, zh->uncsize);
	if(!(zh->flags & ZTrailInfo)){
		if(Bseek(bout, coff, 0) < 0)
			error("can't seek in archive");
	}
}

static void
cheader(Biobuf *bout, ZipHead *zh)
{
	int flen;

	put4(bout, ZCHeader);
	put1(bout, zh->madevers);
	put1(bout, zh->madeos);
	put1(bout, zh->extvers);
	put1(bout, zh->extos);
	put2(bout, zh->flags & ~ZTrailInfo);
	put2(bout, zh->meth);
	put2(bout, zh->modtime);
	put2(bout, zh->moddate);
	put4(bout, zh->crc);
	put4(bout, zh->csize);
	put4(bout, zh->uncsize);
	flen = strlen(zh->file);
	put2(bout, flen);
	put2(bout, 0);
	put2(bout, 0);
	put2(bout, 0);
	put2(bout, zh->iattr);
	put4(bout, zh->eattr);
	put4(bout, zh->off);
	if(Bwrite(bout, zh->file, flen) != flen)
		error("write error");
}

static void
putCDir(Biobuf *bout)
{
	vlong hoff, ecoff;
	int i;

	hoff = Boffset(bout);

	for(i = 0; i < nzheads; i++)
		cheader(bout, &zheads[i]);

	ecoff = Boffset(bout);

	if(nzheads >= (1 << 16))
		error("too many entries in zip file: max %d", (1 << 16) - 1);
	put4(bout, ZECHeader);
	put2(bout, 0);
	put2(bout, 0);
	put2(bout, nzheads);
	put2(bout, nzheads);
	put4(bout, ecoff - hoff);
	put4(bout, hoff);
	put2(bout, 0);
}

static int
crcread(void *fd, void *buf, int n)
{
	int nr, m;

	nr = 0;
	for(; !eof && n > 0; n -= m){
		m = read((int)(uintptr)fd, (char*)buf+nr, n);
		if(m <= 0){
			eof = 1;
			if(m < 0)
{
fprint(2, "input error %r\n");
				return -1;
}
			break;
		}
		nr += m;
	}
	crc = blockcrc(crctab, crc, buf, nr);
	totr += nr;
	return nr;
}

static int
zwrite(void *bout, void *buf, int n)
{
	if(n != Bwrite(bout, buf, n)){
		eof = 1;
		return -1;
	}
	totw += n;
	return n;
}

static void
put4(Biobuf *b, u32int v)
{
	int i;

	for(i = 0; i < 4; i++){
		if(Bputc(b, v) < 0)
			error("write error");
		v >>= 8;
	}
}

static void
put2(Biobuf *b, int v)
{
	int i;

	for(i = 0; i < 2; i++){
		if(Bputc(b, v) < 0)
			error("write error");
		v >>= 8;
	}
}

static void
put1(Biobuf *b, int v)
{
	if(Bputc(b, v)< 0)
		error("unexpected eof reading file information");
}

static void
error(char *fmt, ...)
{
	va_list arg;

	fprint(2, "zip: ");
	va_start(arg, fmt);
	vfprint(2, fmt, arg);
	va_end(arg);
	fprint(2, "\n");

	longjmp(zjmp, 1);
}
