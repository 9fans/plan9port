#include <u.h>
#include <libc.h>
#include <bio.h>
#include "bzlib.h"

static	int	bzipf(char*, int);
static	int	bzip(char*, long, int, Biobuf*);

static	Biobuf	bout;
static	int	level;
static	int	debug;
static	int	verbose;


static void
usage(void)
{
	fprint(2, "usage: bzip2 [-vcD] [-1-9] [file ...]\n");
	exits("usage");
}

void
main(int argc, char **argv)
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
	case 'v':
		verbose++;
		break;
	case 'c':
		stdout++;
		break;
	case 'f':
		/* force */
		break;
	case 'd':
		/*
		 * gnu tar expects bzip2 -d to decompress
		 * humor it.  ugh.
		 */
		/* remove -d from command line - magic! */
		if(strcmp(argv[0], "-d") == 0){
			while(*argv++)
				*(argv-1) = *argv;
		}else
			memmove(_args-1, _args, strlen(_args)+1);
		exec("bunzip2", oargv);
		sysfatal("exec bunzip2 failed");
		break;
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		level = ARGC() - '0';
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc == 0){
		Binit(&bout, 1, OWRITE);
		ok = bzip(nil, time(0), 0, &bout);
		Bterm(&bout);
	}else{
		ok = 1;
		for(i = 0; i < argc; i++)
			ok &= bzipf(argv[i], stdout);
	}
	exits(ok ? nil: "errors");
}

static int
bzipf(char *file, int stdout)
{
	Dir *dir;
	char ofile[128], *f, *s;
	int ifd, ofd, ok;

	ifd = open(file, OREAD);
	if(ifd < 0){
		fprint(2, "bzip2: can't open %s: %r\n", file);
		return 0;
	}
	dir = dirfstat(ifd);
	if(dir == nil){
		fprint(2, "bzip2: can't stat %s: %r\n", file);
		close(ifd);
		return 0;
	}
	if(dir->mode & DMDIR){
		fprint(2, "bzip2: can't compress a directory\n");
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
			snprint(ofile, sizeof(ofile), "%s.tbz", f);
		}else
			snprint(ofile, sizeof(ofile), "%s.bz2", f);
		ofd = create(ofile, OWRITE, 0666);
		if(ofd < 0){
			fprint(2, "bzip2: can't open %s: %r\n", ofile);
			free(dir);
			close(ifd);
			return 0;
		}
	}

	if(verbose)
		fprint(2, "compressing %s to %s\n", file, ofile);

	Binit(&bout, ofd, OWRITE);
	ok = bzip(file, dir->mtime, ifd, &bout);
	if(!ok || Bflush(&bout) < 0){
		fprint(2, "bzip2: error writing %s: %r\n", ofile);
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
bzip(char *file, long mtime, int ifd, Biobuf *bout)
{
	int e, n, done, onemore;
	char buf[8192];
	char obuf[8192];
	Biobuf bin;
	bz_stream strm;

	USED(file);
	USED(mtime);

	memset(&strm, 0, sizeof strm);
	BZ2_bzCompressInit(&strm, level, verbose, 0);

	strm.next_in = buf;
	strm.avail_in = 0;
	strm.next_out = obuf;
	strm.avail_out = sizeof obuf;

	done = 0;
	Binit(&bin, ifd, OREAD);

	/*
	 * onemore is a crummy hack to go 'round the loop
	 * once after we finish, to flush the output buffer.
	 */
	onemore = 1;
	SET(e);
	do {
		if(!done && strm.avail_in < sizeof buf) {
			if(strm.avail_in)
				memmove(buf, strm.next_in, strm.avail_in);

			n = Bread(&bin, buf+strm.avail_in, sizeof(buf)-strm.avail_in);
			if(n <= 0)
				done = 1;
			else
				strm.avail_in += n;
			strm.next_in = buf;
		}
		if(strm.avail_out < sizeof obuf) {
			Bwrite(bout, obuf, sizeof(obuf)-strm.avail_out);
			strm.next_out = obuf;
			strm.avail_out = sizeof obuf;
		}

		if(onemore == 0)
			break;
	} while((e=BZ2_bzCompress(&strm, done ? BZ_FINISH : BZ_RUN)) == BZ_RUN_OK || e == BZ_FINISH_OK || onemore--);

	if(e != BZ_STREAM_END) {
		fprint(2, "bzip2: compress failed\n");
		return 0;
	}

	if(BZ2_bzCompressEnd(&strm) != BZ_OK) {
		fprint(2, "bzip2: compress end failed (can't happen)\n");
		return 0;
	}

	Bterm(&bin);

	return 1;
}
