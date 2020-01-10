#include "stdinc.h"
#include "dat.h"
#include "fns.h"

Bloom b;

void
usage(void)
{
	fprint(2, "usage: fmtbloom [-n nblocks | -N nhash] [-s size] file\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	Part *part;
	char *file;
	vlong bits, size, size2;
	int nhash;
	vlong nblocks;

	ventifmtinstall();
	statsinit();

	size = 0;
	nhash = 0;
	nblocks = 0;
	ARGBEGIN{
	case 'n':
		if(nhash || nblocks)
			usage();
		nblocks = unittoull(EARGF(usage()));
		break;
	case 'N':
		if(nhash || nblocks)
			usage();
		nhash = unittoull(EARGF(usage()));
		if(nhash > BloomMaxHash){
			fprint(2, "maximum possible is -N %d", BloomMaxHash);
			usage();
		}
		break;
	case 's':
		size = unittoull(ARGF());
		if(size == ~0)
			usage();
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 1)
		usage();

	file = argv[0];

	part = initpart(file, ORDWR|ODIRECT);
	if(part == nil)
		sysfatal("can't open partition %s: %r", file);

	if(size == 0)
		size = part->size;

	if(size < 1024*1024)
		sysfatal("bloom filter too small");

	if(size > MaxBloomSize){
		fprint(2, "warning: not using entire %,lld bytes; using only %,lld bytes\n",
			size, (vlong)MaxBloomSize);
		size = MaxBloomSize;
	}
	if(size&(size-1)){
		for(size2=1; size2<size; size2*=2)
			;
		size = size2/2;
		fprint(2, "warning: size not a power of 2; only using %lldMB\n", size/1024/1024);
	}

	if(nblocks){
		/*
		 * no use for more than 32 bits per block
		 * shoot for less than 64 bits per block
		 */
		size2 = size;
		while(size2*8 >= nblocks*64)
			size2 >>= 1;
		if(size2 != size){
			size = size2;
			fprint(2, "warning: using only %lldMB - not enough blocks to warrant more\n",
				size/1024/1024);
		}

		/*
		 * optimal is to use ln 2 times as many hash functions as we have bits per blocks.
		 */
		bits = (8*size)/nblocks;
		nhash = bits*7/10;
		if(nhash > BloomMaxHash)
			nhash = BloomMaxHash;
	}
	if(!nhash)
		nhash = BloomMaxHash;
	if(bloominit(&b, size, nil) < 0)
		sysfatal("bloominit: %r");
	b.nhash = nhash;
	bits = nhash*10/7;
	nblocks = (8*size)/bits;
	fprint(2, "fmtbloom: using %lldMB, %d hashes/score, best up to %,lld blocks\n", size/1024/1024, nhash, nblocks);
	b.data = vtmallocz(size);
	b.part = part;
	if(writebloom(&b) < 0)
		sysfatal("writing %s: %r", file);
	threadexitsall(0);
}
