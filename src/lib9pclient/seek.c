#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

vlong
fsseek(CFid *fid, vlong n, int whence)
{
	Dir *d;

	switch(whence){
	case 0:
		qlock(&fid->lk);
		fid->offset = n;
		qunlock(&fid->lk);
		break;
	case 1:
		qlock(&fid->lk);
		n += fid->offset;
		if(n < 0){
			qunlock(&fid->lk);
			werrstr("negative offset");
			return -1;
		}
		fid->offset = n;
		qunlock(&fid->lk);
		break;
	case 2:
		if((d = fsdirfstat(fid)) == nil)
			return -1;
		n += d->length;
		if(n < 0){
			werrstr("negative offset");
			return -1;
		}
		qlock(&fid->lk);
		fid->offset = n;
		qunlock(&fid->lk);
		break;
	default:
		werrstr("bad whence in fsseek");
		return -1;
	}
	return n;
}
