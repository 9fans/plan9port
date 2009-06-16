#include <u.h>
#include <libc.h>
#include <venti.h>
#include <diskfs.h>
#include <thread.h>

void
usage(void)
{
	fprint(2, "usage: vcat [-z] score >diskfile\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	extern int nfilereads;
	char *pref;
	int zerotoo;
	uchar score[VtScoreSize];
	u8int *zero;
	u32int i;
	u32int n;
	Block *b;
	Disk *disk;
	Fsys *fsys;
	VtCache *c;
	VtConn *z;

	zerotoo = 0;
	ARGBEGIN{
	case 'z':
		zerotoo = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fmtinstall('V', vtscorefmt);

	if(vtparsescore(argv[0], &pref, score) < 0)
		sysfatal("bad score '%s'", argv[0]);
	if((z = vtdial(nil)) == nil)
		sysfatal("vtdial: %r");
	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");
	if((c = vtcachealloc(z, 16384*32)) == nil)
		sysfatal("vtcache: %r");
	if((disk = diskopenventi(c, score)) == nil)
		sysfatal("diskopenventi: %r");
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	zero = emalloc(fsys->blocksize);
	fprint(2, "%d blocks total\n", fsys->nblock);
	n = 0;
	for(i=0; i<fsys->nblock; i++){
		if((b = fsysreadblock(fsys, i)) != nil){
			if(pwrite(1, b->data, fsys->blocksize,
			    (u64int)fsys->blocksize*i) != fsys->blocksize)
				fprint(2, "error writing block %lud: %r\n", i);
			n++;
			blockput(b);
		}else if(zerotoo || i==fsys->nblock-1)
			if(pwrite(1, zero, fsys->blocksize,
			    (u64int)fsys->blocksize*i) != fsys->blocksize)
				fprint(2, "error writing block %lud: %r\n", i);
	}
	fprint(2, "%d blocks in use, %d file reads\n", n, nfilereads);
	threadexitsall(nil);
}
