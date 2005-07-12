#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include <bio.h>

Biobuf bout;

static void
pie(IEntry *ie)
{
	Bprint(&bout, "%22lld %V %3d %5d\n",
		ie->ia.addr, ie->score, ie->ia.type, ie->ia.size);
}

void
usage(void)
{
	fprint(2, "usage: printindex [-B blockcachesize] config [isectname...]\n");
	threadexitsall(0);
}

Config conf;

int
shoulddump(char *name, int argc, char **argv)
{
	int i;

	if(argc == 0)
		return 1;
	for(i=0; i<argc; i++)
		if(strcmp(name, argv[i]) == 0)
			return 1;
	return 0;
}

void
dumpisect(ISect *is)
{
	int j;
	uchar *buf;
	u32int i;
	u64int off;
	IBucket ib;
	IEntry ie;

	buf = emalloc(is->blocksize);
	for(i=0; i<is->blocks; i++){
		off = is->blockbase+(u64int)is->blocksize*i;
		if(readpart(is->part, off, buf, is->blocksize) < 0)
			fprint(2, "read %s at 0x%llux: %r\n", is->part->name, off);
		else{
			unpackibucket(&ib, buf, is->bucketmagic);
			for(j=0; j<ib.n; j++){
				unpackientry(&ie, &ib.data[j*IEntrySize]);
				pie(&ie);
			}
		}
	}
}

void
threadmain(int argc, char *argv[])
{
	int i;
	Index *ix;
	u32int bcmem;

	bcmem = 0;
	ARGBEGIN{
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc < 1)
		usage();

	fmtinstall('H', encodefmt);

	if(initventi(argv[0], &conf) < 0)
		sysfatal("can't init venti: %r");

	if(bcmem < maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16))
		bcmem = maxblocksize * (mainindex->narenas + mainindex->nsects * 4 + 16);
	if(0) fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	ix = mainindex;
	Binit(&bout, 1, OWRITE);
	for(i=0; i<ix->nsects; i++)
		if(shoulddump(ix->sects[i]->name, argc-1, argv+1))
			dumpisect(ix->sects[i]);
	Bterm(&bout);
	threadexitsall(0);
}
