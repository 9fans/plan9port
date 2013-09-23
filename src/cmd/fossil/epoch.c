#include "stdinc.h"
#include "dat.h"
#include "fns.h"

uchar buf[65536];

void
usage(void)
{
	fprint(2, "usage: fossil/epoch fs [new-low-epoch]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int fd;
	Header h;
	Super s;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc == 0 || argc > 2)
		usage();

	if((fd = open(argv[0], argc==2 ? ORDWR : OREAD)) < 0)
		sysfatal("open %s: %r", argv[0]);

	if(pread(fd, buf, HeaderSize, HeaderOffset) != HeaderSize)
		sysfatal("reading header: %r");
	if(!headerUnpack(&h, buf))
		sysfatal("unpacking header: %r");

	if(pread(fd, buf, h.blockSize, (vlong)h.super*h.blockSize) != h.blockSize)
		sysfatal("reading super block: %r");

	if(!superUnpack(&s, buf))
		sysfatal("unpacking super block: %r");

	print("epoch %d\n", s.epochLow);
	if(argc == 2){
		s.epochLow = strtoul(argv[1], 0, 0);
		superPack(&s, buf);
		if(pwrite(fd, buf, h.blockSize, (vlong)h.super*h.blockSize) != h.blockSize)
			sysfatal("writing super block: %r");
	}
	exits(0);
}
