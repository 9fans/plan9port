#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

CFid*
fsopen(CFsys *fs, char *name, int mode)
{
	char e[ERRMAX];
	CFid *fid;
	Fcall tx, rx;

	if((fid = _fswalk(fs->root, name)) == nil)
		return nil;
	tx.type = Topen;
	tx.fid = fid->fid;
	tx.mode = mode;
	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		rerrstr(e, sizeof e);
		fsclose(fid);
		errstr(e, sizeof e);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
