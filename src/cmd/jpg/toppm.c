#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <ctype.h>
#include <bio.h>
#include "imagefile.h"

void
usage(void)
{
	fprint(2, "usage: toppm [-c 'comment'] [file]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Biobuf bout;
	Memimage *i, *ni;
	int fd;
	char buf[256];
	char *err, *comment;

	comment = nil;
	ARGBEGIN{
	case 'c':
		comment = ARGF();
		if(comment == nil)
			usage();
		if(strchr(comment, '\n') != nil){
			fprint(2, "ppm: comment cannot contain newlines\n");
			usage();
		}
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if(Binit(&bout, 1, OWRITE) < 0)
		sysfatal("Binit failed: %r");

	memimageinit();

	err = nil;

	if(argc == 0){
		i = readmemimage(0);
		if(i == nil)
			sysfatal("reading input: %r");
		ni = memmultichan(i);
		if(ni == nil)
			sysfatal("converting image to RGBV: %r");
		if(i != ni){
			freememimage(i);
			i = ni;
		}
		if(err == nil)
			err = memwriteppm(&bout, i, comment);
	}else{
		fd = open(argv[0], OREAD);
		if(fd < 0)
			sysfatal("can't open %s: %r", argv[0]);
		i = readmemimage(fd);
		if(i == nil)
			sysfatal("can't readimage %s: %r", argv[0]);
		close(fd);
		ni = memmultichan(i);
		if(ni == nil)
			sysfatal("converting image to RGBV: %r");
		if(i != ni){
			freememimage(i);
			i = ni;
		}
		if(comment)
			err = memwriteppm(&bout, i, comment);
		else{
			snprint(buf, sizeof buf, "Converted by Plan 9 from %s", argv[0]);
			err = memwriteppm(&bout, i, buf);
		}
		freememimage(i);
	}

	if(err != nil)
		fprint(2, "toppm: %s\n", err);
	exits(err);
}
