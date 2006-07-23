#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

int
fsfopen(CFid *fid, int mode)
{
	Fcall tx, rx;

	tx.type = Topen;
	tx.fid = fid->fid;
	tx.mode = mode;
	if(_fsrpc(fid->fs, &tx, &rx, 0) < 0)
		return -1;
	fid->mode = mode;
	return 0;
}

CFid*
fsopen(CFsys *fs, char *name, int mode)
{
	char e[ERRMAX];
	CFid *fid;

	if((fid = fswalk(fs->root, name)) == nil)
		return nil;
	if(fsfopen(fid, mode) < 0){
		rerrstr(e, sizeof e);
		fsclose(fid);
		errstr(e, sizeof e);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
