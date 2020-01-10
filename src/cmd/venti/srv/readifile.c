#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
usage(void)
{
	fprint(2, "usage: readifile file\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	IFile ifile;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if(readifile(&ifile, argv[0]) < 0)
		sysfatal("readifile %s: %r", argv[0]);
	write(1, ifile.b->data, ifile.b->len);
	threadexitsall(nil);
}
