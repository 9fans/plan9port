#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

CFid*
fscreate(CFsys *fs, char *name, int mode, ulong perm)
{
	CFid *fid;
	Fcall tx, rx;

	if((fid = _fswalk(fs->root, name)) == nil)
		return nil;
	tx.type = Tcreate;
	tx.fid = fid->fid;
	tx.mode = mode;
	tx.perm = perm;
	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
