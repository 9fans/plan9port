#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

Fid*
fscreate(Fsys *fs, char *name, int mode, ulong perm)
{
	Fid *fid;
	Fcall tx, rx;

	if((fid = fswalk(fs->root, name)) == nil)
		return nil;
	tx.type = Tcreate;
	tx.fid = fid->fid;
	tx.mode = mode;
	tx.perm = perm;
	if(fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
