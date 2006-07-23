/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

CFid*
fswalk(CFid *fid, char *oname)
{
	char *freep, *name;
	int i, nwalk;
	char *p;
	CFid *wfid;
	Fcall tx, rx;

	freep = nil;
	name = oname;
	if(name){
		freep = malloc(strlen(name)+1);
		if(freep == nil)
			return nil;
		strcpy(freep, name);
		name = freep;
	}

	if((wfid = _fsgetfid(fid->fs)) == nil){
		free(freep);
		return nil;
	}

	nwalk = 0;
	do{
		/* collect names */
		for(i=0; name && *name && i < MAXWELEM; ){
			p = name;
			name = strchr(name, '/');
			if(name)
				*name++ = 0;
			if(*p == 0 || (*p == '.' && *(p+1) == 0))
				continue;
			tx.wname[i++] = p;
		}

		/* do a walk */
		tx.type = Twalk;
		tx.fid = nwalk ? wfid->fid : fid->fid;
		tx.newfid = wfid->fid;
		tx.nwname = i;
		if(_fsrpc(fid->fs, &tx, &rx, 0) < 0){
		Error:
			free(freep);
			if(nwalk)
				fsclose(wfid);
			else
				_fsputfid(wfid);
			return nil;
		}
		if(rx.nwqid != tx.nwname){
			/* XXX lame error */
			werrstr("file '%s' not found", oname);
			goto Error;
		}
		if(rx.nwqid == 0)
			wfid->qid = fid->qid;
		else
			wfid->qid = rx.wqid[rx.nwqid-1];
		nwalk++;
	}while(name && *name);
	return wfid;
}
