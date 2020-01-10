#include <u.h>
#include <libc.h>
#include <auth.h>
#include <thread.h>
#include <9pclient.h>
#include "authlocal.h"

CFsys*
nsamount(char *name, char *aname)
{
	CFid *afid, *fid;
	AuthInfo *ai;
	CFsys *fs;

	fs = nsinit(name);
	if(fs == nil)
		return nil;
	if((afid = fsauth(fs, getuser(), aname)) == nil)
		goto noauth;
	ai = fsauth_proxy(afid, amount_getkey, "proto=p9any role=client");
	if(ai != nil)
		auth_freeAI(ai);
noauth:
	fid = fsattach(fs, afid, getuser(), aname);
	fsclose(afid);
	if(fid == nil){
		_fsunmount(fs);
		return nil;
	}
	fssetroot(fs, fid);
	return fs;
}
