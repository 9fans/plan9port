#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

Fid*
fsopen(Fsys *fs, char *name, int mode)
{
	Fid *fid;
	Fcall tx, rx;

	if((fid = fswalk(fs->root, name)) == nil)
		return nil;
	tx.type = Topen;
	tx.fid = fid->fid;
	tx.mode = mode;
	if(fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return nil;
	}
	fid->mode = mode;
	return fid;
}
