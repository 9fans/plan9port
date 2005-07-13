#include <u.h>
#include <libc.h>
#include <bio.h>
#include <diskfs.h>

Block*
diskread(Disk *disk, u32int count, u64int offset)
{
	if(disk == nil)
		return nil;

	if(!disk->_read){
		werrstr("no disk read dispatch function");
		return nil;
	}
	return (*disk->_read)(disk, count, offset);
}

int
disksync(Disk *disk)
{
	if(disk == nil)
		return 0;
	if(!disk->_sync)
		return 0;
	return (*disk->_sync)(disk);
}

void
diskclose(Disk *disk)
{
	if(disk == nil)
		return;
	if(!disk->_close){
		fprint(2, "no diskClose\n");
		abort();
	}
	(*disk->_close)(disk);
}
