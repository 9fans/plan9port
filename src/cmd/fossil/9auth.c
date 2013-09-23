#include "stdinc.h"
#include "9.h"

int
authRead(Fid* afid, void* data, int count)
{
	AuthInfo *ai;
	AuthRpc *rpc;

	if((rpc = afid->rpc) == nil){
		werrstr("not an auth fid");
		return -1;
	}

	switch(auth_rpc(rpc, "read", nil, 0)){
	default:
		werrstr("fossil authRead: auth protocol not finished");
		return -1;
	case ARdone:
		if((ai = auth_getinfo(rpc)) == nil){
			werrstr("%r");
			break;
		}
		if(ai->cuid == nil || *ai->cuid == '\0'){
			werrstr("auth with no cuid");
			auth_freeAI(ai);
			break;
		}
		assert(afid->cuname == nil);
		afid->cuname = vtstrdup(ai->cuid);
		auth_freeAI(ai);
		if(Dflag)
			fprint(2, "authRead cuname %s\n", afid->cuname);
		assert(afid->uid == nil);
		if((afid->uid = uidByUname(afid->cuname)) == nil){
			werrstr("unknown user %#q", afid->cuname);
			break;
		}
		return 0;
	case ARok:
		if(count < rpc->narg){
			werrstr("not enough data in auth read");
			break;
		}
		memmove(data, rpc->arg, rpc->narg);
		return rpc->narg;
	case ARphase:
		werrstr("%r");
		break;
	}
	return -1;
}

int
authWrite(Fid* afid, void* data, int count)
{
	assert(afid->rpc != nil);
	if(auth_rpc(afid->rpc, "write", data, count) != ARok)
		return -1;
	return count;
}

int
authCheck(Fcall* t, Fid* fid, Fsys* fsys)
{
	Con *con;
	Fid *afid;
	uchar buf[1];

	/*
	 * Can't lookup with FidWlock here as there may be
	 * protocol to do. Use a separate lock to protect altering
	 * the auth information inside afid.
	 */
	con = fid->con;
	if(t->afid == NOFID){
		/*
		 * If no authentication is asked for, allow
		 * "none" provided the connection has already
		 * been authenticatated.
		 *
		 * The console is allowed to attach without
		 * authentication.
		 */
		rlock(&con->alock);
		if(con->isconsole){
			/* anything goes */
		}else if((con->flags&ConNoneAllow) || con->aok){
			static int noneprint;

			if(noneprint++ < 10)
				consPrint("attach %s as %s: allowing as none\n",
					fsysGetName(fsys), fid->uname);
			vtfree(fid->uname);
			fid->uname = vtstrdup(unamenone);
		}else{
			runlock(&con->alock);
			consPrint("attach %s as %s: connection not authenticated, not console\n",
				fsysGetName(fsys), fid->uname);
			werrstr("cannot attach as none before authentication");
			return 0;
		}
		runlock(&con->alock);

		if((fid->uid = uidByUname(fid->uname)) == nil){
			consPrint("attach %s as %s: unknown uname\n",
				fsysGetName(fsys), fid->uname);
			werrstr("unknown user");
			return 0;
		}
		return 1;
	}

	if((afid = fidGet(con, t->afid, 0)) == nil){
		consPrint("attach %s as %s: bad afid\n",
			fsysGetName(fsys), fid->uname);
		werrstr("bad authentication fid");
		return 0;
	}

	/*
	 * Check valid afid;
	 * check uname and aname match.
	 */
	if(!(afid->qid.type & QTAUTH)){
		consPrint("attach %s as %s: afid not an auth file\n",
			fsysGetName(fsys), fid->uname);
		fidPut(afid);
		werrstr("bad authentication fid");
		return 0;
	}
	if(strcmp(afid->uname, fid->uname) != 0 || afid->fsys != fsys){
		consPrint("attach %s as %s: afid is for %s as %s\n",
			fsysGetName(fsys), fid->uname,
			fsysGetName(afid->fsys), afid->uname);
		fidPut(afid);
		werrstr("attach/auth mismatch");
		return 0;
	}

	qlock(&afid->alock);
	if(afid->cuname == nil){
		if(authRead(afid, buf, 0) != 0 || afid->cuname == nil){
			qunlock(&afid->alock);
			consPrint("attach %s as %s: %r\n",
				fsysGetName(fsys), fid->uname);
			fidPut(afid);
			werrstr("fossil authCheck: auth protocol not finished");
			return 0;
		}
	}
	qunlock(&afid->alock);

	assert(fid->uid == nil);
	if((fid->uid = uidByUname(afid->cuname)) == nil){
		consPrint("attach %s as %s: unknown cuname %s\n",
			fsysGetName(fsys), fid->uname, afid->cuname);
		fidPut(afid);
		werrstr("unknown user");
		return 0;
	}

	vtfree(fid->uname);
	fid->uname = vtstrdup(afid->cuname);
	fidPut(afid);

	/*
	 * Allow "none" once the connection has been authenticated.
	 */
	wlock(&con->alock);
	con->aok = 1;
	wunlock(&con->alock);

	return 1;
}
