/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

static void
fidclunk(Fid *fid)
{
	Fcall tx, rx;

	tx.type = Tclunk;
	tx.fid = fid->fid;
	fsrpc(fid->fs, &tx, &rx, 0);
	_fsputfid(fid);
}

void
fsclose(Fid *fid)
{
	/* maybe someday there will be a ref count */
	fidclunk(fid);
}
