/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

static void
fidclunk(CFid *fid)
{
	Fcall tx, rx;

	tx.type = Tclunk;
	tx.fid = fid->fid;
	_fsrpc(fid->fs, &tx, &rx, 0);
	_fsputfid(fid);
}

void
fsclose(CFid *fid)
{
	if(fid == nil)
		return;

	/* maybe someday there will be a ref count */
	fidclunk(fid);
}

int
fsfremove(CFid *fid)
{
	int n;
	Fcall tx, rx;

	tx.type = Tremove;
	tx.fid = fid->fid;
	n = _fsrpc(fid->fs, &tx, &rx, 0);
	_fsputfid(fid);
	return n;
}
