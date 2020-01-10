#include <u.h>
#include <libc.h>
#include <bio.h>
#include <flate.h>
#include "gzip.h"

typedef struct	GZHead	GZHead;

struct GZHead
{
	u32int	mtime;
	char	*file;
};

static	int	crcwrite(void *bout, void *buf, int n);
static	int	get1(Biobuf *b);
static	u32int	get4(Biobuf *b);
static	int	gunzipf(char *file, int stdout);
static	int	gunzip(int ofd, char *ofile, Biobuf *bin);
static	void	header(Biobuf *bin, GZHead *h);
static	void	trailer(Biobuf *bin, long wlen);
static	void	error(char*, ...);
/* #pragma	varargck	argpos	error	1 */

static	Biobuf	bin;
static	u32int	crc;
static	u32int	*crctab;
static	int	debug;
static	char	*delfile;
static	vlong	gzok;
static	char	*infile;
static	int	settimes;
static	int	table;
static	int	verbose;
static	int	wbad;
static	u32int	wlen;
static	jmp_buf	zjmp;

void
usage(void)
{
	fprint(2, "usage: gunzip [-ctvTD] [file ....]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, ok, stdout;

	stdout = 0;
	ARGBEGIN{
	case 'D':
		debug++;
		break;
	case 'c':
		stdout++;
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

	crctab = mkcrctab(GZCRCPOLY);
	ok = inflateinit();
	if(ok != FlateOk)
		sysfatal("inflateinit failed: %s\n", flateerr(ok));

	if(argc == 0){
		Binit(&bin, 0, OREAD);
		settimes = 0;
		infile = "<stdin>";
		ok = gunzip(1, "<stdout>", &bin);
	}else{
		ok = 1;
		if(stdout)
			settimes = 0;
		for(i = 0; i < argc; i++)
			ok &= gunzipf(argv[i], stdout);
	}

	exits(ok ? nil: "errors");
}

static int
gunzipf(char *file, int stdout)
{
	char ofile[256], *s;
	int ofd, ifd, ok;

	infile = file;
	ifd = open(file, OREAD);
	if(ifd < 0){
		fprint(2, "gunzip: can't open %s: %r\n", file);
		return 0;
	}

	Binit(&bin, ifd, OREAD);
	if(Bgetc(&bin) != GZMAGIC1 || Bgetc(&bin) != GZMAGIC2 || Bgetc(&bin) != GZDEFLATE){
		fprint(2, "gunzip: %s is not a gzip deflate file\n", file);
		Bterm(&bin);
		close(ifd);
		return 0;
	}
	Bungetc(&bin);
	Bungetc(&bin);
	Bungetc(&bin);

	if(table)
		ofd = -1;
	else if(stdout){
		ofd = 1;
		strcpy(ofile, "<stdout>");
	}else{
		s = strrchr(file, '/');
		if(s != nil)
			s++;
		else
			s = file;
		strecpy(ofile, ofile+sizeof ofile, s);
		s = strrchr(ofile, '.');
		if(s != nil && s != ofile && strcmp(s, ".gz") == 0)
			*s = '\0';
		else if(s != nil && strcmp(s, ".tgz") == 0)
			strcpy(s, ".tar");
		else if(strcmp(file, ofile) == 0){
			fprint(2, "gunzip: can't overwrite %s\n", file);
			Bterm(&bin);
			close(ifd);
			return 0;
		}

		ofd = create(ofile, OWRITE, 0666);
		if(ofd < 0){
			fprint(2, "gunzip: can't create %s: %r\n", ofile);
			Bterm(&bin);
			close(ifd);
			return 0;
		}
		delfile = ofile;
	}

	wbad = 0;
	ok = gunzip(ofd, ofile, &bin);
	Bterm(&bin);
	close(ifd);
	if(wbad){
		fprint(2, "gunzip: can't write %s: %r\n", ofile);
		if(delfile)
			remove(delfile);
	}
	delfile = nil;
	if(!stdout && ofd >= 0)
		close(ofd);
	return ok;
}

static int
gunzip(int ofd, char *ofile, Biobuf *bin)
{
	Dir *d;
	GZHead h;
	int err;

	h.file = nil;
	gzok = 0;
	for(;;){
		if(Bgetc(bin) < 0)
			return 1;
		Bungetc(bin);

		if(setjmp(zjmp))
			return 0;
		header(bin, &h);
		gzok = 0;

		wlen = 0;
		crc = 0;

		if(!table && verbose)
			fprint(2, "extracting %s to %s\n", h.file, ofile);

		err = inflate((void*)(uintptr)ofd, crcwrite, bin, (int(*)(void*))Bgetc);
		if(err != FlateOk)
			error("inflate failed: %s", flateerr(err));

		trailer(bin, wlen);

		if(table){
			if(verbose)
				print("%-32s %10ld %s", h.file, wlen, ctime(h.mtime));
			else
				print("%s\n", h.file);
		}else if(settimes && h.mtime && (d=dirfstat(ofd)) != nil){
			d->mtime = h.mtime;
			dirfwstat(ofd, d);
			free(d);
		}

		free(h.file);
		h.file = nil;
		gzok = Boffset(bin);
	}
/*	return 0; */
}

static void
header(Biobuf *bin, GZHead *h)
{
	char *s;
	int i, c, flag, ns, nsa;

	if(get1(bin) != GZMAGIC1 || get1(bin) != GZMAGIC2)
		error("bad gzip file magic");
	if(get1(bin) != GZDEFLATE)
		error("unknown compression type");

	flag = get1(bin);
	if(flag & ~(GZFTEXT|GZFEXTRA|GZFNAME|GZFCOMMENT|GZFHCRC))
		fprint(2, "gunzip: reserved flags set, data may not be decompressed correctly\n");

	/* mod time */
	h->mtime = get4(bin);

	/* extra flags */
	get1(bin);

	/* OS type */
	get1(bin);

	if(flag & GZFEXTRA)
		for(i=get1(bin); i>0; i--)
			get1(bin);

	/* name */
	if(flag & GZFNAME){
		nsa = 32;
		ns = 0;
		s = malloc(nsa);
		if(s == nil)
			error("out of memory");
		while((c = get1(bin)) != 0){
			s[ns++] = c;
			if(ns >= nsa){
				nsa += 32;
				s = realloc(s, nsa);
				if(s == nil)
					error("out of memory");
			}
		}
		s[ns] = '\0';
		h->file = s;
	}else
		h->file = strdup("<unnamed file>");

	/* comment */
	if(flag & GZFCOMMENT)
		while(get1(bin) != 0)
			;

	/* crc16 */
	if(flag & GZFHCRC){
		get1(bin);
		get1(bin);
	}
}

static void
trailer(Biobuf *bin, long wlen)
{
	u32int tcrc;
	long len;

	tcrc = get4(bin);
	if(tcrc != crc)
		error("crc mismatch");

	len = get4(bin);

	if(len != wlen)
		error("bad output length: expected %lud got %lud", wlen, len);
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
get1(Biobuf *b)
{
	int c;

	c = Bgetc(b);
	if(c < 0)
		error("unexpected eof reading file information");
	return c;
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

static void
error(char *fmt, ...)
{
	va_list arg;

	if(gzok)
		fprint(2, "gunzip: %s: corrupted data after byte %lld ignored\n", infile, gzok);
	else{
		fprint(2, "gunzip: ");
		if(infile)
			fprint(2, "%s: ", infile);
		va_start(arg, fmt);
		vfprint(2, fmt, arg);
		va_end(arg);
		fprint(2, "\n");

		if(delfile != nil){
			fprint(2, "gunzip: removing output file %s\n", delfile);
			remove(delfile);
			delfile = nil;
		}
	}

	longjmp(zjmp, 1);
}
