#include <u.h>
#include <libc.h>
#include <bio.h>
#include "bzlib.h"

static	Biobuf	bin;
static	int	debug;
static	int	verbose;
static	char	*delfile;
static	char	*infile;
static	int	bunzipf(char *file, int stdout);
static	int	bunzip(int ofd, char *ofile, Biobuf *bin);

void
usage(void)
{
	fprint(2, "usage: bunzip2 [-cvD] [file ...]\n");
	exits("usage");
}

void
main(int argc, char **argv)
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
	case 'v':
		verbose++;
		break;
	}ARGEND

	if(argc == 0){
		Binit(&bin, 0, OREAD);
		infile = "<stdin>";
		ok = bunzip(1, "<stdout>", &bin);
	}else{
		ok = 1;
		for(i = 0; i < argc; i++)
			ok &= bunzipf(argv[i], stdout);
	}

	exits(ok ? nil: "errors");
}

static int
bunzipf(char *file, int stdout)
{
	char ofile[64], *s;
	int ofd, ifd, ok;

	infile = file;
	ifd = open(file, OREAD);
	if(ifd < 0){
		fprint(2, "gunzip: can't open %s: %r\n", file);
		return 0;
	}

	Binit(&bin, ifd, OREAD);
	if(Bgetc(&bin) != 'B' || Bgetc(&bin) != 'Z' || Bgetc(&bin) != 'h'){
		fprint(2, "bunzip2: %s is not a bzip2 file\n", file);
		Bterm(&bin);
		close(ifd);
		return 0;
	}
	Bungetc(&bin);
	Bungetc(&bin);
	Bungetc(&bin);

	if(stdout){
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
		if(s != nil && s != ofile && strcmp(s, ".bz2") == 0)
			*s = '\0';
		else if(s != nil && (strcmp(s, ".tbz") == 0 || strcmp(s, ".tbz2") == 0))
			strcpy(s, ".tar");
		else if(strcmp(file, ofile) == 0){
			fprint(2, "bunzip2: can't overwrite %s\n", file);
			Bterm(&bin);
			close(ifd);
			return 0;
		}

		ofd = create(ofile, OWRITE, 0666);
		if(ofd < 0){
			fprint(2, "bunzip2: can't create %s: %r\n", ofile);
			Bterm(&bin);
			close(ifd);
			return 0;
		}
		delfile = ofile;
	}

	ok = bunzip(ofd, ofile, &bin);
	Bterm(&bin);
	close(ifd);
	if(!ok){
		fprint(2, "bunzip2: can't write %s: %r\n", ofile);
		if(delfile)
			remove(delfile);
	}
	delfile = nil;
	if(!stdout && ofd >= 0)
		close(ofd);
	return ok;
}

static int
bunzip(int ofd, char *ofile, Biobuf *bin)
{
	int e, n, done, onemore;
	char buf[8192];
	char obuf[8192];
	Biobuf bout;
	bz_stream strm;

	USED(ofile);

	memset(&strm, 0, sizeof strm);
	BZ2_bzDecompressInit(&strm, verbose, 0);

	strm.next_in = buf;
	strm.avail_in = 0;
	strm.next_out = obuf;
	strm.avail_out = sizeof obuf;

	done = 0;
	Binit(&bout, ofd, OWRITE);

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

			n = Bread(bin, buf+strm.avail_in, sizeof(buf)-strm.avail_in);
			if(n <= 0)
				done = 1;
			else
				strm.avail_in += n;
			strm.next_in = buf;
		}
		if(strm.avail_out < sizeof obuf) {
			Bwrite(&bout, obuf, sizeof(obuf)-strm.avail_out);
			strm.next_out = obuf;
			strm.avail_out = sizeof obuf;
		}
		if(onemore == 0)
			break;
		if(strm.avail_in == 0 && strm.avail_out == sizeof obuf)
			break;
	} while((e=BZ2_bzDecompress(&strm)) == BZ_OK || onemore--);

	if(e != BZ_STREAM_END) {
		fprint(2, "bunzip2: decompress failed\n");
		return 0;
	}

	if(BZ2_bzDecompressEnd(&strm) != BZ_OK) {
		fprint(2, "bunzip2: decompress end failed (can't happen)\n");
		return 0;
	}

	Bterm(&bout);

	return 1;
}
