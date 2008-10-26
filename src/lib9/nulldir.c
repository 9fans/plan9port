#include <u.h>
#include <libc.h>

void
nulldir(Dir *d)
{
	memset(d, ~0, sizeof(Dir));
	d->name = d->uid = d->gid = d->muid = d->ext = "";
}
