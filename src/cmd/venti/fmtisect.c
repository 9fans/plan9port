#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
usage(void)
{
	fprint(2, "usage: fmtisect [-Z] [-b blocksize] name file\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	ISect *is;
	Part *part;
	char *file, *name;
	int blocksize, setsize, zero;

	fmtinstall('V', vtscorefmt);
	statsinit();

	blocksize = 8 * 1024;
	setsize = 64 * 1024;
	zero = 1;
	ARGBEGIN{
	case 'b':
		blocksize = unittoull(ARGF());
		if(blocksize == ~0)
			usage();
		if(blocksize > MaxDiskBlock){
			fprint(2, "block size too large, max %d\n", MaxDiskBlock);
			threadexitsall("usage");
		}
		break;
	case 'Z':
		zero = 0;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 2)
		usage();

	name = argv[0];
	file = argv[1];

	if(nameok(name) < 0)
		sysfatal("illegal name %s", name);

	part = initpart(file, 0);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	if(zero)
		zeropart(part, blocksize);

	fprint(2, "configuring index section %s with space for index config bytes=%d\n", name, setsize);
	is = newisect(part, name, blocksize, setsize);
	if(is == nil)
		sysfatal("can't initialize new index: %r");

	if(wbisect(is) < 0)
		fprint(2, "can't write back index section header for %s: %r\n", file);

	threadexitsall(0);
}
