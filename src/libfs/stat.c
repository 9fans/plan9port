/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

Dir*
fsdirstat(Fsys *fs, char *name)
{
	Dir *d;
	Fid *fid;

	if((fid = fswalk(fs->root, name)) == nil)
		return nil;
	
	d = fsdirfstat(fid);
	fsclose(fid);
	return d;
}

Dir*
fsdirfstat(Fid *fid)
{
	Dir *d;
	Fsys *fs;
	Fcall tx, rx;
	void *freep;
	int n;

	fs = fid->fs;
	tx.type = Tstat;
	tx.fid = fid->fid;

	if(fsrpc(fs, &tx, &rx, &freep) < 0)
		return nil;

	d = malloc(sizeof(Dir)+rx.nstat);
	if(d == nil){
		free(freep);
		return nil;
	}
	n = convM2D(rx.stat, rx.nstat, d, (char*)&d[1]);
	free(freep);
	if(n != rx.nstat){
		free(d);
		werrstr("rx.nstat and convM2D disagree about dir length");
		return nil;
	}
	return d;
}

