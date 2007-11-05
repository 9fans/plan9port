#include <u.h>
#include <libc.h>
#include <diskfs.h>

int nfilereads;
void _nfilereads_darwin_sucks(void) { }

typedef struct DiskFile DiskFile;
struct DiskFile
{
	Disk disk;
	int fd;
};

static long
preadn(int fd, void *vdata, u32int ulen, u64int offset)
{
	long n;
	uchar *data;
	long len;

	nfilereads++;
	len = ulen;
	data = vdata;
/*	fprint(2, "readn 0x%llux 0x%ux\n", offset, ulen); */
	while(len > 0){
		n = pread(fd, data, len, offset);
		if(n <= 0)
			break;
		data += n;
		offset += n;
		len -= n;
	}
	return data-(uchar*)vdata;
}

static void
diskfileblockput(Block *b)
{
	free(b);
}

uvlong nreadx;
static Block*
diskfileread(Disk *dd, u32int len, u64int offset)
{
	int n;
	Block *b;
	DiskFile *d = (DiskFile*)dd;

	b = mallocz(sizeof(Block)+len, 1);
	if(b == nil)
		return nil;
	b->data = (uchar*)&b[1];
nreadx += len;
	n = preadn(d->fd, b->data, len, offset);
	if(n <= 0){
		free(b);
		return nil;
	}
	b->_close = diskfileblockput;
	b->len = n;
	return b;
}

static int
diskfilesync(Disk *dd)
{
	USED(dd);
	return 0;
}

static void
diskfileclose(Disk *dd)
{
	DiskFile *d = (DiskFile*)dd;

	close(d->fd);
	free(d);
}

Disk*
diskopenfile(char *file)
{
	int fd;
	DiskFile *d;

	if((fd = open(file, OREAD)) < 0)
		return nil;
	d = mallocz(sizeof(DiskFile), 1);
	if(d == nil){
		close(fd);
		return nil;
	}
	d->disk._read = diskfileread;
	d->disk._sync = diskfilesync;
	d->disk._close = diskfileclose;
	d->fd = fd;
	return &d->disk;
}
