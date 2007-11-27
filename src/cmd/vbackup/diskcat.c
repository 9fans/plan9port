#include <u.h>
#include <libc.h>
#include <diskfs.h>

void
usage(void)
{
	fprint(2, "usage: fscat fspartition\n");
	exits("usage");
}

int
main(int argc, char **argv)
{
	extern int nfilereads;
	u8int *zero;
	u32int i;
	u32int n;
	Block *b;
	Disk *disk;
	Fsys *fsys;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if((disk = diskopenfile(argv[0])) == nil)
		sysfatal("diskopen: %r");
	if((disk = diskcache(disk, 16384, 16)) == nil)
		sysfatal("diskcache: %r");
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	zero = emalloc(fsys->blocksize);
	fprint(2, "%d blocks total\n", fsys->nblock);
	n = 0;
	for(i=0; i<fsys->nblock; i++){
		if((b = fsysreadblock(fsys, i)) != nil){
			write(1, b->data, fsys->blocksize);
			n++;
			blockput(b);
		}else
			write(1, zero, fsys->blocksize);
		if(b == nil && i < 2)
			sysfatal("block %d not in use", i);
	}
	fprint(2, "%d blocks in use, %d file reads\n", n, nfilereads);
	exits(nil);
	return 0;
}
