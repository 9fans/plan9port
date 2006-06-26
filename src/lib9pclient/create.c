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

	if((fid = _fswalk(fs->root, dir)) == nil){
		if(p)
			*p = '/';
		return nil;
	}
	tx.type = Tcreate;
	tx.name = elem;
	tx.fid = fid->fid;
	tx.mode = mode;
	tx.perm = perm;
	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		if(p)
			*p = '/';
		fsclose(fid);
		return nil;
	}
	if(p)
		*p = '/';
	fid->mode = mode;
	return fid;
}
