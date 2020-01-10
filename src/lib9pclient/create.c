#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

int
fsfcreate(CFid *fid, char *name, int mode, ulong perm)
{
	Fcall tx, rx;

	tx.type = Tcreate;
	tx.name = name;
	tx.fid = fid->fid;
	tx.mode = mode;
	tx.perm = perm;
	tx.extension = nil;
	if(_fsrpc(fid->fs, &tx, &rx, 0) < 0)
		return -1;
	fid->mode = mode;
	return 0;
}

CFid*
fscreate(CFsys *fs, char *name, int mode, ulong perm)
{
	CFid *fid;
	char *p, *dir, *elem;

	p = strrchr(name, '/');
	if(p == nil){
		dir = "";
		elem = name;
	}else{
		dir = name;
		*p = 0;
		elem = p+1;
	}

	if((fid = fswalk(fs->root, dir)) == nil){
		if(p)
			*p = '/';
		return nil;
	}
	if(p)
		*p = '/';
	if(fsfcreate(fid, elem, mode, perm) < 0){
		fsclose(fid);
		return nil;
	}
	return fid;
}
