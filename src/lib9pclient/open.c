#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

CFid*
fsopen(CFsys *fs, char *name, int mode)
{
	CFid *fid;
	Fcall tx, rx;

	if((fid = _fswalk(fs->root, name)) == nil)
		return nil;
	tx.type = Topen;
	tx.fid = fid->fid;
	tx.mode = mode;
	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
