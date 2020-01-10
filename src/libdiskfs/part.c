#include <u.h>
#include <libc.h>
#include <diskfs.h>

typedef struct DiskPart DiskPart;
struct DiskPart
{
	Disk disk;
	Disk *subdisk;
	u64int offset;
	u64int size;
};

static Block*
diskpartread(Disk *dd, u32int len, u64int offset)
{
	DiskPart *d = (DiskPart*)dd;

	if(offset+len > d->size){
		werrstr("read past end of partition %llud + %lud > %llud", offset, len, d->size);
		return nil;
	}
	return diskread(d->subdisk, len, offset+d->offset);
}

static int
diskpartsync(Disk *dd)
{
	DiskPart *d = (DiskPart*)dd;

	if(d->subdisk)
		return disksync(d->subdisk);
	return 0;
}

static void
diskpartclose(Disk *dd)
{
	DiskPart *d = (DiskPart*)dd;

	if(d->subdisk)
		diskclose(d->subdisk);
	free(d);
}

Disk*
diskpart(Disk *subdisk, u64int offset, u64int size)
{
	DiskPart *d;

	d = mallocz(sizeof(DiskPart), 1);
	if(d == nil)
		return nil;

	d->subdisk = subdisk;
	d->offset = offset;
	d->size = size;
	d->disk._read = diskpartread;
	d->disk._sync = diskpartsync;
	d->disk._close = diskpartclose;

	return &d->disk;
}

void
diskpartabandon(Disk *d)
{
	if(d->_read != diskpartread)
		abort();
	((DiskPart*)d)->subdisk = nil;
}
