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
	fprint(2, "usage: togif [-l loopcount] [-c 'comment'] [-d Δt (ms)] [-t transparency-index] [file ... [-d Δt] file ...]\n");
	exits("usage");
}

#define	UNSET (-12345678)

void
main(int argc, char *argv[])
{
	Biobuf bout;
	Memimage *i, *ni;
	int fd, j, dt, trans, loop;
	char buf[256];
	char *err, *comment, *s;

	comment = nil;
	dt = -1;
	trans = -1;
	loop = UNSET;
	ARGBEGIN{
	case 'l':
		s = ARGF();
		if(s==nil || (!isdigit(s[0]) && s[0]!='-'))
			usage();
		loop = atoi(s);
		break;
	case 'c':
		comment = ARGF();
		if(comment == nil)
			usage();
		break;
	case 'd':
		s = ARGF();
		if(s==nil || !isdigit(s[0]))
			usage();
		dt = atoi(s);
		break;
	case 't':
		s = ARGF();
		if(s==nil || !isdigit(s[0]))
			usage();
		trans = atoi(s);
		if(trans > 255)
			usage();
		break;
	default:
		usage();
	}ARGEND

	if(Binit(&bout, 1, OWRITE) < 0)
		sysfatal("Binit failed: %r");

	memimageinit();

	err = nil;

	if(argc == 0){
		i = readmemimage(0);
		if(i == nil)
			sysfatal("reading input: %r");
		ni = memonechan(i);
		if(ni == nil)
			sysfatal("converting image to RGBV: %r");
		if(i != ni){
			freememimage(i);
			i = ni;
		}
		err = memstartgif(&bout, i, -1);
		if(err == nil){
			if(comment)
				err = memwritegif(&bout, i, comment, dt, trans);
			else{
				snprint(buf, sizeof buf, "Converted by Plan 9 from <stdin>");
				err = memwritegif(&bout, i, buf, dt, trans);
			}
		}
	}else{
		if(loop == UNSET){
			if(argc == 1)
				loop = -1;	/* no loop for single image */
			else
				loop = 0;	/* the default case: 0 means infinite loop */
		}
		for(j=0; j<argc; j++){
			if(argv[j][0] == '-' && argv[j][1]=='d'){
				/* time change */
				if(argv[j][2] == '\0'){
					s = argv[++j];
					if(j == argc)
						usage();
				}else
					s = &argv[j][2];
				if(!isdigit(s[0]))
					usage();
				dt = atoi(s);
				if(j == argc-1)	/* last argument must be file */
					usage();
				continue;
			}
			fd = open(argv[j], OREAD);
			if(fd < 0)
				sysfatal("can't open %s: %r", argv[j]);
			i = readmemimage(fd);
			if(i == nil)
				sysfatal("can't readimage %s: %r", argv[j]);
			close(fd);
			ni = memonechan(i);
			if(ni == nil)
				sysfatal("converting image to RGBV: %r");
			if(i != ni){
				freememimage(i);
				i = ni;
			}
			if(j == 0){
				err = memstartgif(&bout, i, loop);
				if(err != nil)
					break;
			}
			if(comment)
				err = memwritegif(&bout, i, comment, dt, trans);
			else{
				snprint(buf, sizeof buf, "Converted by Plan 9 from %s", argv[j]);
				err = memwritegif(&bout, i, buf, dt, trans);
			}
			if(err != nil)
				break;
			freememimage(i);
			comment = nil;
		}
	}
	memendgif(&bout);

	if(err != nil)
		fprint(2, "togif: %s\n", err);
	exits(err);
}
