#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

int
fsopenfd(Fsys *fs, char *name, int mode)
{
	Fid *fid;
	Fcall tx, rx;

	if((fid = fswalk(fs->root, name)) == nil)
		return -1;
	tx.type = Topenfd;
	tx.fid = fid->fid;
	tx.mode = mode&~OCEXEC;
	if(fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return -1;
	}
	_fsputfid(fid);
	if(mode&OCEXEC && rx.unixfd>=0)
		fcntl(rx.unixfd, F_SETFL, FD_CLOEXEC);
	return rx.unixfd;
}
