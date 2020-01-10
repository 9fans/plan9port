#include <u.h>
#include <libc.h>
#include <diskfs.h>

Fsys*
fsysopenkfs(Disk *disk)
{
	USED(disk);
	return nil;
}
