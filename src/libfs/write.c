/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

long
fspwrite(Fid *fd, void *buf, long n, vlong offset)
{
	Fcall tx, rx;
	void *freep;

	tx.type = Tread;
	if(offset == -1){
		qlock(&fd->lk);
		tx.offset = fd->offset;
		fd->offset += n;
		qunlock(&fd->lk);
	}else
		tx.offset = offset;
	tx.count = n;
	tx.data = buf;

	fsrpc(fd->fs, &tx, &rx, &freep);
	if(rx.type == Rerror){
		if(offset == -1){
			qlock(&fd->lk);
			fd->offset -= n;
			qunlock(&fd->lk);
		}
		werrstr("%s", rx.ename);
		free(freep);
		return -1;
	}
	free(freep);
	return rx.count;
}

long
fswrite(Fid *fd, void *buf, long n)
{
	return fspwrite(fd, buf, n, -1);
}
