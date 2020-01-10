/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

int
fsremove(CFsys *fs, char *name)
{
	CFid *fid;

	if((fid = fswalk(fs->root, name)) == nil)
		return -1;
	return fsfremove(fid);
}
