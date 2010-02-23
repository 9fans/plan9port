#include <u.h>
#include <libc.h>
#include <bio.h>
#include <flate.h>
#include "gzip.h"

static	int	gzipf(char*, int);
static	int	gzip(char*, long, int, Biobuf*);
static	int	crcread(void *fd, void *buf, int n);
static	int	gzwrite(void *bout, void *buf, int n);

static	Biobuf	bout;
static	u32int	crc;
static	u32int	*crctab;
static	int	debug;
static	int	eof;
static	int	level;
static	u32int	totr;
static	int	verbose;

void
usage(void)
{
	fprint(2, "usage: gzip [-vcD] [-1-9] [file ...]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, ok, stdout;
	char **oargv;

	oargv = argv;
	level = 6;
	stdout = 0;
	ARGBEGIN{
	case 'D':
		debug++;
		break;
	case 'd':
		/*
		 * gnu tar expects gzip -d to decompress
		 * humor it.  ugh.
		 */
		/* remove -d from command line - magic! */
		if(strcmp(argv[0], "-d") == 0){
			while(*argv++)
				*(argv-1) = *argv;
		}else
			memmove(_args-1, _args, strlen(_args)+1);
		exec("gunzip", oargv);
		sysfatal("exec gunzip failed");
		break;
	case 'f':
		/* force */
		break;
	case 'v':
		verbose++;
		break;
	case 'c':
		stdout = 1;
		break;
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		level = ARGC() - '0';
		break;
	default:
		usage();
		break;
	}ARGEND

	crctab = mkcrctab(GZCRCPOLY);
	ok = deflateinit();
	if(ok != FlateOk)
		sysfatal("deflateinit failed: %s\n", flateerr(ok));

	if(argc == 0){
		Binit(&bout, 1, OWRITE);
		ok = gzip(nil, time(0), 0, &bout);
		Bterm(&bout);
	}else{
		ok = 1;
		for(i = 0; i < argc; i++)
			ok &= gzipf(argv[i], stdout);
	}
	exits(ok ? nil: "errors");
}

static int
gzipf(char *file, int stdout)
{
	Dir *dir;
	char ofile[256], *f, *s;
	int ifd, ofd, ok;

	ifd = open(file, OREAD);
	if(ifd < 0){
		fprint(2, "gzip: can't open %s: %r\n", file);
		return 0;
	}
	dir = dirfstat(ifd);
	if(dir == nil){
		fprint(2, "gzip: can't stat %s: %r\n", file);
		close(ifd);
		return 0;
	}
	if(dir->mode & DMDIR){
		fprint(2, "gzip: can't compress a directory\n");
		close(ifd);
		free(dir);
		return 0;
	}

	if(stdout){
		ofd = 1;
		strcpy(ofile, "<stdout>");
	}else{
		f = strrchr(file, '/');
		if(f != nil)
			f++;
		else
			f = file;
		s = strrchr(f, '.');
		if(s != nil && s != ofile && strcmp(s, ".tar") == 0){
			*s = '\0';
			snprint(ofile, sizeof(ofile), "%s.tgz", f);
		}else
			snprint(ofile, sizeof(ofile), "%s.gz", f);
		ofd = create(ofile, OWRITE, 0666);
		if(ofd < 0){
			fprint(2, "gzip: can't open %s: %r\n", ofile);
			close(ifd);
			return 0;
		}
	}

	if(verbose)
		fprint(2, "compressing %s to %s\n", file, ofile);

	Binit(&bout, ofd, OWRITE);
	ok = gzip(file, dir->mtime, ifd, &bout);
	if(!ok || Bflush(&bout) < 0){
		fprint(2, "gzip: error writing %s: %r\n", ofile);
		if(!stdout)
			remove(ofile);
	}
	Bterm(&bout);
	free(dir);
	close(ifd);
	close(ofd);
	return ok;
}

static int
gzip(char *file, long mtime, int ifd, Biobuf *bout)
{
	int flags, err;

	flags = 0;
	Bputc(bout, GZMAGIC1);
	Bputc(bout, GZMAGIC2);
	Bputc(bout, GZDEFLATE);

	if(file != nil)
		flags |= GZFNAME;
	Bputc(bout, flags);

	Bputc(bout, mtime);
	Bputc(bout, mtime>>8);
	Bputc(bout, mtime>>16);
	Bputc(bout, mtime>>24);

	Bputc(bout, 0);
	Bputc(bout, GZOSINFERNO);

	if(flags & GZFNAME)
		Bwrite(bout, file, strlen(file)+1);

	crc = 0;
	eof = 0;
	totr = 0;
	err = deflate(bout, gzwrite, (void*)(uintptr)ifd, crcread, level, debug);
	if(err != FlateOk){
		fprint(2, "gzip: deflate failed: %s\n", flateerr(err));
		return 0;
	}

	Bputc(bout, crc);
	Bputc(bout, crc>>8);
	Bputc(bout, crc>>16);
	Bputc(bout, crc>>24);

	Bputc(bout, totr);
	Bputc(bout, totr>>8);
	Bputc(bout, totr>>16);
	Bputc(bout, totr>>24);

	return 1;
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
				return -1;
			break;
		}
		nr += m;
	}
	crc = blockcrc(crctab, crc, buf, nr);
	totr += nr;
	return nr;
}

static int
gzwrite(void *bout, void *buf, int n)
{
	if(n != Bwrite(bout, buf, n)){
		eof = 1;
		return -1;
	}
	return n;
}
