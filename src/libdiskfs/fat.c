#include <u.h>
#include <libc.h>
#include <diskfs.h>

Fsys*
fsysopenfat(Disk *disk)
{
	USED(disk);
	return nil;
}
