/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

int
fsdirwstat(Fsys *fs, char *name, Dir *d)
{
	int n;
	Fid *fid;

	if((fid = fswalk(fs->root, name)) == nil)
		return -1;
	
	n = fsdirfwstat(fid, d);
	fsclose(fid);
	return n;
}

int
fsdirfwstat(Fid *fid, Dir *d)
{
	uchar *a;
	int n, nn;
	Fcall tx, rx;

	n = sizeD2M(d);
	a = malloc(n);
	if(a == nil)
		return -1;
	nn = convD2M(d, a, n);
	if(n != nn){
		werrstr("convD2M and sizeD2M disagree");
		free(a);
		return -1;
	}

	tx.type = Twstat;
	tx.fid = fid->fid;
	tx.stat = a;
	tx.nstat = n;
	n = fsrpc(fid->fs, &tx, &rx, 0);
	free(a);
	return n;
}
