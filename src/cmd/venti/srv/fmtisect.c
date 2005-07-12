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
	int vers;
	ISect *is;
	Part *part;
	char *file, *name;
	int blocksize, setsize, zero;

	ventifmtinstall();
	statsinit();

	blocksize = 8 * 1024;
	setsize = 512 * 1024;
	zero = -1;
	vers = ISectVersion2;
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
	case '1':
		vers = ISectVersion1;
		break;
	case 'Z':
		zero = 0;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(zero == -1){
		if(vers == ISectVersion1)
			zero = 1;
		else
			zero = 0;
	}

	if(argc != 2)
		usage();

	name = argv[0];
	file = argv[1];

	if(nameok(name) < 0)
		sysfatal("illegal name %s", name);

	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	if(zero)
		zeropart(part, blocksize);

	is = newisect(part, vers, name, blocksize, setsize);
	if(is == nil)
		sysfatal("can't initialize new index: %r");

	fprint(2, "fmtisect %s: %,d buckets of %,d entries, %,d bytes for index map\n",
		file, is->blocks, is->buckmax, setsize);

	if(wbisect(is) < 0)
		fprint(2, "can't write back index section header for %s: %r\n", file);

	threadexitsall(0);
}
