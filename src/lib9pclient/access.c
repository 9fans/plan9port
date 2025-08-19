#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include <9pdefs.h>
#include "fsimpl.h"

int
fsaccess(CFsys *fsys, char *name, int mode)
{
	CFid *fid;
	Dir *db;
	static char omode[] = {
		0,
		OEXEC_9P,
		OWRITE_9P,
		ORDWR_9P,
		OREAD_9P,
		OEXEC_9P,	/* only approximate */
		ORDWR_9P,
		ORDWR_9P	/* only approximate */
	};

	if(mode == AEXIST){
		db = fsdirstat(fsys, name);
		free(db);
		if(db != nil)
			return 0;
		return -1;
	}
	fid = fsopen(fsys, name, omode[mode&7]);
	if(fid != nil){
		fsclose(fid);
		return 0;
	}
	return -1;
}
