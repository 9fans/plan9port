/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

CFid*
fsauth(CFsys *fsys, char *uname, char *aname)
{
	Fcall tx, rx;
	void *freep;
	CFid *afid;

	if((afid = _fsgetfid(fsys)) == nil)
		return nil;

	tx.type = Tauth;
	tx.afid = afid->fid;
	tx.uname = uname;
	tx.aname = aname;

	if(_fsrpc(fsys, &tx, &rx, &freep) < 0){
		_fsputfid(afid);
		return nil;
	}
	if(rx.type == Rerror){
		werrstr("%s", rx.ename);
		free(freep);
		_fsputfid(afid);
		return nil;
	}
	afid->qid = rx.aqid;
	free(freep);
	return afid;
}
