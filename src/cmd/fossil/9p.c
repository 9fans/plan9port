#include "stdinc.h"

#include "9.h"

enum {
	OMODE		= 0x7,		/* Topen/Tcreate mode */
};

enum {
	PermX		= 1,
	PermW		= 2,
	PermR		= 4,
};

static char EPermission[] = "permission denied";

static int
permFile(File* file, Fid* fid, int perm)
{
	char *u;
	DirEntry de;

	if(!fileGetDir(file, &de))
		return -1;

	/*
	 * User none only gets other permissions.
	 */
	if(strcmp(fid->uname, unamenone) != 0){
		/*
		 * There is only one uid<->uname mapping
		 * and it's already cached in the Fid, but
		 * it might have changed during the lifetime
		 * if this Fid.
		 */
		if((u = unameByUid(de.uid)) != nil){
			if(strcmp(fid->uname, u) == 0 && ((perm<<6) & de.mode)){
				vtfree(u);
				deCleanup(&de);
				return 1;
			}
			vtfree(u);
		}
		if(groupMember(de.gid, fid->uname) && ((perm<<3) & de.mode)){
			deCleanup(&de);
			return 1;
		}
	}
	if(perm & de.mode){
		if(perm == PermX && (de.mode & ModeDir)){
			deCleanup(&de);
			return 1;
		}
		if(!groupMember(uidnoworld, fid->uname)){
			deCleanup(&de);
			return 1;
		}
	}
	if(fsysNoPermCheck(fid->fsys) || (fid->con->flags&ConNoPermCheck)){
		deCleanup(&de);
		return 1;
	}
	werrstr(EPermission);

	deCleanup(&de);
	return 0;
}

static int
permFid(Fid* fid, int p)
{
	return permFile(fid->file, fid, p);
}

static int
permParent(Fid* fid, int p)
{
	int r;
	File *parent;

	parent = fileGetParent(fid->file);
	r = permFile(parent, fid, p);
	fileDecRef(parent);

	return r;
}

int
validFileName(char* name)
{
	char *p;

	if(name == nil || name[0] == '\0'){
		werrstr("no file name");
		return 0;
	}
	if(name[0] == '.'){
		if(name[1] == '\0' || (name[1] == '.' && name[2] == '\0')){
			werrstr(". and .. illegal as file name");
			return 0;
		}
	}

	for(p = name; *p != '\0'; p++){
		if((*p & 0xFF) < 040){
			werrstr("bad character in file name");
			return 0;
		}
	}

	return 1;
}

static int
rTwstat(Msg* m)
{
	Dir dir;
	Fid *fid;
	ulong mode, oldmode;
	DirEntry de;
	char *gid, *strs, *uid;
	int gl, op, retval, tsync, wstatallow;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock)) == nil)
		return 0;

	gid = uid = nil;
	retval = 0;

	if(strcmp(fid->uname, unamenone) == 0 || (fid->qid.type & QTAUTH)){
		werrstr(EPermission);
		goto error0;
	}
	if(fileIsRoFs(fid->file) || !groupWriteMember(fid->uname)){
		werrstr("read-only filesystem");
		goto error0;
	}

	if(!fileGetDir(fid->file, &de))
		goto error0;

	strs = vtmalloc(m->t.nstat);
	if(convM2D(m->t.stat, m->t.nstat, &dir, strs) == 0){
		werrstr("wstat -- protocol botch");
		goto error;
	}

	/*
	 * Run through each of the (sub-)fields in the provided Dir
	 * checking for validity and whether it's a default:
	 * .type, .dev and .atime are completely ignored and not checked;
	 * .qid.path, .qid.vers and .muid are checked for validity but
	 * any attempt to change them is an error.
	 * .qid.type/.mode, .mtime, .name, .length, .uid and .gid can
	 * possibly be changed.
	 *
	 * 'Op' flags there are changed fields, i.e. it's not a no-op.
	 * 'Tsync' flags all fields are defaulted.
	 */
	tsync = 1;
	if(dir.qid.path != ~0){
		if(dir.qid.path != de.qid){
			werrstr("wstat -- attempt to change qid.path");
			goto error;
		}
		tsync = 0;
	}
	if(dir.qid.vers != (u32int)~0){
		if(dir.qid.vers != de.mcount){
			werrstr("wstat -- attempt to change qid.vers");
			goto error;
		}
		tsync = 0;
	}
	if(dir.muid != nil && *dir.muid != '\0'){
		if((uid = uidByUname(dir.muid)) == nil){
			werrstr("wstat -- unknown muid");
			goto error;
		}
		if(strcmp(uid, de.mid) != 0){
			werrstr("wstat -- attempt to change muid");
			goto error;
		}
		vtfree(uid);
		uid = nil;
		tsync = 0;
	}

	/*
	 * Check .qid.type and .mode agree if neither is defaulted.
	 */
	if(dir.qid.type != (uchar)~0 && dir.mode != (u32int)~0){
		if(dir.qid.type != ((dir.mode>>24) & 0xFF)){
			werrstr("wstat -- qid.type/mode mismatch");
			goto error;
		}
	}

	op = 0;

	oldmode = de.mode;
	if(dir.qid.type != (uchar)~0 || dir.mode != (u32int)~0){
		/*
		 * .qid.type or .mode isn't defaulted, check for unknown bits.
		 */
		if(dir.mode == ~0)
			dir.mode = (dir.qid.type<<24)|(de.mode & 0777);
		if(dir.mode & ~(DMDIR|DMAPPEND|DMEXCL|DMTMP|0777)){
			werrstr("wstat -- unknown bits in qid.type/mode");
			goto error;
		}

		/*
		 * Synthesise a mode to check against the current settings.
		 */
		mode = dir.mode & 0777;
		if(dir.mode & DMEXCL)
			mode |= ModeExclusive;
		if(dir.mode & DMAPPEND)
			mode |= ModeAppend;
		if(dir.mode & DMDIR)
			mode |= ModeDir;
		if(dir.mode & DMTMP)
			mode |= ModeTemporary;

		if((de.mode^mode) & ModeDir){
			werrstr("wstat -- attempt to change directory bit");
			goto error;
		}

		if((de.mode & (ModeAppend|ModeExclusive|ModeTemporary|0777)) != mode){
			de.mode &= ~(ModeAppend|ModeExclusive|ModeTemporary|0777);
			de.mode |= mode;
			op = 1;
		}
		tsync = 0;
	}

	if(dir.mtime != (u32int)~0){
		if(dir.mtime != de.mtime){
			de.mtime = dir.mtime;
			op = 1;
		}
		tsync = 0;
	}

	if(dir.length != ~0){
		if(dir.length != de.size){
			/*
			 * Cannot change length on append-only files.
			 * If we're changing the append bit, it's okay.
			 */
			if(de.mode & oldmode & ModeAppend){
				werrstr("wstat -- attempt to change length of append-only file");
				goto error;
			}
			if(de.mode & ModeDir){
				werrstr("wstat -- attempt to change length of directory");
				goto error;
			}
			de.size = dir.length;
			op = 1;
		}
		tsync = 0;
	}

	/*
	 * Check for permission to change .mode, .mtime or .length,
	 * must be owner or leader of either group, for which test gid
	 * is needed; permission checks on gid will be done later.
	 */
	if(dir.gid != nil && *dir.gid != '\0'){
		if((gid = uidByUname(dir.gid)) == nil){
			werrstr("wstat -- unknown gid");
			goto error;
		}
		tsync = 0;
	}
	else
		gid = vtstrdup(de.gid);

	wstatallow = (fsysWstatAllow(fid->fsys) || (m->con->flags&ConWstatAllow));

	/*
	 * 'Gl' counts whether neither, one or both groups are led.
	 */
	gl = groupLeader(gid, fid->uname) != 0;
	gl += groupLeader(de.gid, fid->uname) != 0;

	if(op && !wstatallow){
		if(strcmp(fid->uid, de.uid) != 0 && !gl){
			werrstr("wstat -- not owner or group leader");
			goto error;
		}
	}

	/*
	 * Check for permission to change group, must be
	 * either owner and in new group or leader of both groups.
	 * If gid is nil here then
	 */
	if(strcmp(gid, de.gid) != 0){
		if(!wstatallow
		&& !(strcmp(fid->uid, de.uid) == 0 && groupMember(gid, fid->uname))
		&& !(gl == 2)){
			werrstr("wstat -- not owner and not group leaders");
			goto error;
		}
		vtfree(de.gid);
		de.gid = gid;
		gid = nil;
		op = 1;
		tsync = 0;
	}

	/*
	 * Rename.
	 * Check .name is valid and different to the current.
	 * If so, check write permission in parent.
	 */
	if(dir.name != nil && *dir.name != '\0'){
		if(!validFileName(dir.name))
			goto error;
		if(strcmp(dir.name, de.elem) != 0){
			if(permParent(fid, PermW) <= 0)
				goto error;
			vtfree(de.elem);
			de.elem = vtstrdup(dir.name);
			op = 1;
		}
		tsync = 0;
	}

	/*
	 * Check for permission to change owner - must be god.
	 */
	if(dir.uid != nil && *dir.uid != '\0'){
		if((uid = uidByUname(dir.uid)) == nil){
			werrstr("wstat -- unknown uid");
			goto error;
		}
		if(strcmp(uid, de.uid) != 0){
			if(!wstatallow){
				werrstr("wstat -- not owner");
				goto error;
			}
			if(strcmp(uid, uidnoworld) == 0){
				werrstr(EPermission);
				goto error;
			}
			vtfree(de.uid);
			de.uid = uid;
			uid = nil;
			op = 1;
		}
		tsync = 0;
	}

	if(op)
		retval = fileSetDir(fid->file, &de, fid->uid);
	else
		retval = 1;

	if(tsync){
		/*
		 * All values were defaulted,
		 * make the state of the file exactly what it
		 * claims to be before returning...
		 */
		USED(tsync);
	}

error:
	deCleanup(&de);
	vtfree(strs);
	if(gid != nil)
		vtfree(gid);
	if(uid != nil)
		vtfree(uid);
error0:
	fidPut(fid);
	return retval;
};

static int
rTstat(Msg* m)
{
	Dir dir;
	Fid *fid;
	DirEntry de;

	if((fid = fidGet(m->con, m->t.fid, 0)) == nil)
		return 0;
	if(fid->qid.type & QTAUTH){
		memset(&dir, 0, sizeof(Dir));
		dir.qid = fid->qid;
		dir.mode = DMAUTH;
		dir.atime = time(0L);
		dir.mtime = dir.atime;
		dir.length = 0;
		dir.name = "#¿";
		dir.uid = fid->uname;
		dir.gid = fid->uname;
		dir.muid = fid->uname;

		if((m->r.nstat = convD2M(&dir, m->data, m->con->msize)) == 0){
			werrstr("stat QTAUTH botch");
			fidPut(fid);
			return 0;
		}
		m->r.stat = m->data;

		fidPut(fid);
		return 1;
	}
	if(!fileGetDir(fid->file, &de)){
		fidPut(fid);
		return 0;
	}
	fidPut(fid);

	/*
	 * TODO: optimise this copy (in convS2M) away somehow.
	 * This pettifoggery with m->data will do for the moment.
	 */
	m->r.nstat = dirDe2M(&de, m->data, m->con->msize);
	m->r.stat = m->data;
	deCleanup(&de);

	return 1;
}

static int
_rTclunk(Fid* fid, int remove)
{
	int rok;

	if(fid->excl)
		exclFree(fid);

	rok = 1;
	if(remove && !(fid->qid.type & QTAUTH)){
		if((rok = permParent(fid, PermW)) > 0)
			rok = fileRemove(fid->file, fid->uid);
	}
	fidClunk(fid);

	return rok;
}

static int
rTremove(Msg* m)
{
	Fid *fid;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock)) == nil)
		return 0;
	return _rTclunk(fid, 1);
}

static int
rTclunk(Msg* m)
{
	Fid *fid;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock)) == nil)
		return 0;
	_rTclunk(fid, (fid->open & FidORclose));

	return 1;
}

static int
rTwrite(Msg* m)
{
	Fid *fid;
	int count, n;

	if((fid = fidGet(m->con, m->t.fid, 0)) == nil)
		return 0;
	if(!(fid->open & FidOWrite)){
		werrstr("fid not open for write");
		goto error;
	}

	count = m->t.count;
	if(count < 0 || count > m->con->msize-IOHDRSZ){
		werrstr("write count too big");
		goto error;
	}
	if(m->t.offset < 0){
		werrstr("write offset negative");
		goto error;
	}
	if(fid->excl != nil && !exclUpdate(fid))
		goto error;

	if(fid->qid.type & QTDIR){
		werrstr("is a directory");
		goto error;
	}
	else if(fid->qid.type & QTAUTH)
		n = authWrite(fid, m->t.data, count);
	else
		n = fileWrite(fid->file, m->t.data, count, m->t.offset, fid->uid);
	if(n < 0)
		goto error;


	m->r.count = n;

	fidPut(fid);
	return 1;

error:
	fidPut(fid);
	return 0;
}

static int
rTread(Msg* m)
{
	Fid *fid;
	uchar *data;
	int count, n;

	if((fid = fidGet(m->con, m->t.fid, 0)) == nil)
		return 0;
	if(!(fid->open & FidORead)){
		werrstr("fid not open for read");
		goto error;
	}

	count = m->t.count;
	if(count < 0 || count > m->con->msize-IOHDRSZ){
		werrstr("read count too big");
		goto error;
	}
	if(m->t.offset < 0){
		werrstr("read offset negative");
		goto error;
	}
	if(fid->excl != nil && !exclUpdate(fid))
		goto error;

	/*
	 * TODO: optimise this copy (in convS2M) away somehow.
	 * This pettifoggery with m->data will do for the moment.
	 */
	data = m->data+IOHDRSZ;
	if(fid->qid.type & QTDIR)
		n = dirRead(fid, data, count, m->t.offset);
	else if(fid->qid.type & QTAUTH)
		n = authRead(fid, data, count);
	else
		n = fileRead(fid->file, data, count, m->t.offset);
	if(n < 0)
		goto error;

	m->r.count = n;
	m->r.data = (char*)data;

	fidPut(fid);
	return 1;

error:
	fidPut(fid);
	return 0;
}

static int
rTcreate(Msg* m)
{
	Fid *fid;
	File *file;
	ulong mode;
	int omode, open, perm;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock)) == nil)
		return 0;
	if(fid->open){
		werrstr("fid open for I/O");
		goto error;
	}
	if(fileIsRoFs(fid->file) || !groupWriteMember(fid->uname)){
		werrstr("read-only filesystem");
		goto error;
	}
	if(!fileIsDir(fid->file)){
		werrstr("not a directory");
		goto error;
	}
	if(permFid(fid, PermW) <= 0)
		goto error;
	if(!validFileName(m->t.name))
		goto error;
	if(strcmp(fid->uid, uidnoworld) == 0){
		werrstr(EPermission);
		goto error;
	}

	omode = m->t.mode & OMODE;
	open = 0;

	if(omode == OREAD || omode == ORDWR || omode == OEXEC)
		open |= FidORead;
	if(omode == OWRITE || omode == ORDWR)
		open |= FidOWrite;
	if((open & (FidOWrite|FidORead)) == 0){
		werrstr("unknown mode");
		goto error;
	}
	if(m->t.perm & DMDIR){
		if((m->t.mode & (ORCLOSE|OTRUNC)) || (open & FidOWrite)){
			werrstr("illegal mode");
			goto error;
		}
		if(m->t.perm & DMAPPEND){
			werrstr("illegal perm");
			goto error;
		}
	}

	mode = fileGetMode(fid->file);
	perm = m->t.perm;
	if(m->t.perm & DMDIR)
		perm &= ~0777|(mode & 0777);
	else
		perm &= ~0666|(mode & 0666);
	mode = perm & 0777;
	if(m->t.perm & DMDIR)
		mode |= ModeDir;
	if(m->t.perm & DMAPPEND)
		mode |= ModeAppend;
	if(m->t.perm & DMEXCL)
		mode |= ModeExclusive;
	if(m->t.perm & DMTMP)
		mode |= ModeTemporary;

	if((file = fileCreate(fid->file, m->t.name, mode, fid->uid)) == nil){
		fidPut(fid);
		return 0;
	}
	fileDecRef(fid->file);

	fid->qid.vers = fileGetMcount(file);
	fid->qid.path = fileGetId(file);
	fid->file = file;
	mode = fileGetMode(fid->file);
	if(mode & ModeDir)
		fid->qid.type = QTDIR;
	else
		fid->qid.type = QTFILE;
	if(mode & ModeAppend)
		fid->qid.type |= QTAPPEND;
	if(mode & ModeExclusive){
		fid->qid.type |= QTEXCL;
		assert(exclAlloc(fid) != 0);
	}
	if(m->t.mode & ORCLOSE)
		open |= FidORclose;
	fid->open = open;

	m->r.qid = fid->qid;
	m->r.iounit = m->con->msize-IOHDRSZ;

	fidPut(fid);
	return 1;

error:
	fidPut(fid);
	return 0;
}

static int
rTopen(Msg* m)
{
	Fid *fid;
	int isdir, mode, omode, open, rofs;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock)) == nil)
		return 0;
	if(fid->open){
		werrstr("fid open for I/O");
		goto error;
	}

	isdir = fileIsDir(fid->file);
	open = 0;
	rofs = fileIsRoFs(fid->file) || !groupWriteMember(fid->uname);

	if(m->t.mode & ORCLOSE){
		if(isdir){
			werrstr("is a directory");
			goto error;
		}
		if(rofs){
			werrstr("read-only filesystem");
			goto error;
		}
		if(permParent(fid, PermW) <= 0)
			goto error;

		open |= FidORclose;
	}

	omode = m->t.mode & OMODE;
	if(omode == OREAD || omode == ORDWR){
		if(permFid(fid, PermR) <= 0)
			goto error;
		open |= FidORead;
	}
	if(omode == OWRITE || omode == ORDWR || (m->t.mode & OTRUNC)){
		if(isdir){
			werrstr("is a directory");
			goto error;
		}
		if(rofs){
			werrstr("read-only filesystem");
			goto error;
		}
		if(permFid(fid, PermW) <= 0)
			goto error;
		open |= FidOWrite;
	}
	if(omode == OEXEC){
		if(isdir){
			werrstr("is a directory");
			goto error;
		}
		if(permFid(fid, PermX) <= 0)
			goto error;
		open |= FidORead;
	}
	if((open & (FidOWrite|FidORead)) == 0){
		werrstr("unknown mode");
		goto error;
	}

	mode = fileGetMode(fid->file);
	if((mode & ModeExclusive) && exclAlloc(fid) == 0)
		goto error;

	/*
	 * Everything checks out, try to commit any changes.
	 */
	if((m->t.mode & OTRUNC) && !(mode & ModeAppend))
		if(!fileTruncate(fid->file, fid->uid))
			goto error;

	if(isdir && fid->db != nil){
		dirBufFree(fid->db);
		fid->db = nil;
	}

	fid->qid.vers = fileGetMcount(fid->file);
	m->r.qid = fid->qid;
	m->r.iounit = m->con->msize-IOHDRSZ;

	fid->open = open;

	fidPut(fid);
	return 1;

error:
	if(fid->excl != nil)
		exclFree(fid);
	fidPut(fid);
	return 0;
}

static int
rTwalk(Msg* m)
{
	Qid qid;
	Fcall *r, *t;
	int nwname, wlock;
	File *file, *nfile;
	Fid *fid, *ofid, *nfid;

	t = &m->t;
	if(t->fid == t->newfid)
		wlock = FidFWlock;
	else
		wlock = 0;

	/*
	 * The file identified by t->fid must be valid in the
	 * current session and must not have been opened for I/O
	 * by an open or create message.
	 */
	if((ofid = fidGet(m->con, t->fid, wlock)) == nil)
		return 0;
	if(ofid->open){
		werrstr("file open for I/O");
		fidPut(ofid);
		return 0;
	}

	/*
	 * If newfid is not the same as fid, allocate a new file;
	 * a side effect is checking newfid is not already in use (error);
	 * if there are no names to walk this will be equivalent to a
	 * simple 'clone' operation.
	 * It's a no-op if newfid is the same as fid and t->nwname is 0.
	 */
	nfid = nil;
	if(t->fid != t->newfid){
		nfid = fidGet(m->con, t->newfid, FidFWlock|FidFCreate);
		if(nfid == nil){
			werrstr("%s: walk: newfid 0x%ud in use",
				argv0, t->newfid);
			fidPut(ofid);
			return 0;
		}
		nfid->open = ofid->open & ~FidORclose;
		nfid->file = fileIncRef(ofid->file);
		nfid->qid = ofid->qid;
		nfid->uid = vtstrdup(ofid->uid);
		nfid->uname = vtstrdup(ofid->uname);
		nfid->fsys = fsysIncRef(ofid->fsys);
		fid = nfid;
	}
	else
		fid = ofid;

	r = &m->r;
	r->nwqid = 0;

	if(t->nwname == 0){
		if(nfid != nil)
			fidPut(nfid);
		fidPut(ofid);

		return 1;
	}

	file = fid->file;
	fileIncRef(file);
	qid = fid->qid;

	for(nwname = 0; nwname < t->nwname; nwname++){
		/*
		 * Walked elements must represent a directory and
		 * the implied user must have permission to search
		 * the directory.  Walking .. is always allowed, so that
		 * you can't walk into a directory and then not be able
		 * to walk out of it.
		 */
		if(!(qid.type & QTDIR)){
			werrstr("not a directory");
			break;
		}
		switch(permFile(file, fid, PermX)){
		case 1:
			break;
		case 0:
			if(strcmp(t->wname[nwname], "..") == 0)
				break;
		case -1:
			goto Out;
		}
		if((nfile = fileWalk(file, t->wname[nwname])) == nil)
			break;
		fileDecRef(file);
		file = nfile;
		qid.type = QTFILE;
		if(fileIsDir(file))
			qid.type = QTDIR;
		if(fileIsAppend(file))
			qid.type |= QTAPPEND;
		if(fileIsTemporary(file))
			qid.type |= QTTMP;
		if(fileIsExclusive(file))
			qid.type |= QTEXCL;
		qid.vers = fileGetMcount(file);
		qid.path = fileGetId(file);
		r->wqid[r->nwqid++] = qid;
	}

	if(nwname == t->nwname){
		/*
		 * Walked all elements. Update the target fid
		 * from the temporary qid used during the walk,
		 * and tidy up.
		 */
		fid->qid = r->wqid[r->nwqid-1];
		fileDecRef(fid->file);
		fid->file = file;

		if(nfid != nil)
			fidPut(nfid);

		fidPut(ofid);
		return 1;
	}

Out:
	/*
	 * Didn't walk all elements, 'clunk' nfid if it exists
	 * and leave fid untouched.
	 * It's not an error if some of the elements were walked OK.
	 */
	fileDecRef(file);
	if(nfid != nil)
		fidClunk(nfid);

	fidPut(ofid);
	if(nwname == 0)
		return 0;
	return 1;
}

static int
rTflush(Msg* m)
{
	if(m->t.oldtag != NOTAG)
		msgFlush(m);
	return 1;
}

static void
parseAname(char *aname, char **fsname, char **path)
{
	char *s;

	if(aname && aname[0])
		s = vtstrdup(aname);
	else
		s = vtstrdup("main/active");
	*fsname = s;
	if((*path = strchr(s, '/')) != nil)
		*(*path)++ = '\0';
	else
		*path = "";
}

#ifndef PLAN9PORT
/*
 * Check remote IP address against /mnt/ipok.
 * Sources.cs.bell-labs.com uses this to disallow
 * network connections from Sudan, Libya, etc.,
 * following U.S. cryptography export regulations.
 */
static int
conIPCheck(Con* con)
{
	char ok[256], *p;
	int fd;

	if(con->flags&ConIPCheck){
		if(con->remote[0] == 0){
			werrstr("cannot verify unknown remote address");
			return 0;
		}
		if(access("/mnt/ipok/ok", AEXIST) < 0){
			/* mount closes the fd on success */
			if((fd = open("/srv/ipok", ORDWR)) >= 0
			&& mount(fd, -1, "/mnt/ipok", MREPL, "") < 0)
				close(fd);
			if(access("/mnt/ipok/ok", AEXIST) < 0){
				werrstr("cannot verify remote address");
				return 0;
			}
		}
		snprint(ok, sizeof ok, "/mnt/ipok/ok/%s", con->remote);
		if((p = strchr(ok, '!')) != nil)
			*p = 0;
		if(access(ok, AEXIST) < 0){
			werrstr("restricted remote address");
			return 0;
		}
	}
	return 1;
}
#endif

static int
rTattach(Msg* m)
{
	Fid *fid;
	Fsys *fsys;
	char *fsname, *path;

	if((fid = fidGet(m->con, m->t.fid, FidFWlock|FidFCreate)) == nil)
		return 0;

	parseAname(m->t.aname, &fsname, &path);
	if((fsys = fsysGet(fsname)) == nil){
		fidClunk(fid);
		vtfree(fsname);
		return 0;
	}
	fid->fsys = fsys;

	if(m->t.uname[0] != '\0')
		fid->uname = vtstrdup(m->t.uname);
	else
		fid->uname = vtstrdup(unamenone);

#ifndef PLAN9PORT
	if((fid->con->flags&ConIPCheck) && !conIPCheck(fid->con)){
		consPrint("reject %s from %s: %r\n", fid->uname, fid->con->remote);
		fidClunk(fid);
		vtfree(fsname);
		return 0;
	}
#endif
	if(fsysNoAuthCheck(fsys) || (m->con->flags&ConNoAuthCheck)){
		if((fid->uid = uidByUname(fid->uname)) == nil)
			fid->uid = vtstrdup(unamenone);
	}
	else if(!authCheck(&m->t, fid, fsys)){
		fidClunk(fid);
		vtfree(fsname);
		return 0;
	}

	fsysFsRlock(fsys);
	if((fid->file = fsysGetRoot(fsys, path)) == nil){
		fsysFsRUnlock(fsys);
		fidClunk(fid);
		vtfree(fsname);
		return 0;
	}
	fsysFsRUnlock(fsys);
	vtfree(fsname);

	fid->qid = (Qid){fileGetId(fid->file), 0, QTDIR};
	m->r.qid = fid->qid;

	fidPut(fid);
	return 1;
}

static int
rTauth(Msg* m)
{
#ifndef PLAN9PORT
	int afd;
#endif
	Con *con;
	Fid *afid;
	Fsys *fsys;
	char *fsname, *path;

	parseAname(m->t.aname, &fsname, &path);
	if((fsys = fsysGet(fsname)) == nil){
		vtfree(fsname);
		return 0;
	}
	vtfree(fsname);

	if(fsysNoAuthCheck(fsys) || (m->con->flags&ConNoAuthCheck)){
		m->con->aok = 1;
		werrstr("authentication disabled");
		fsysPut(fsys);
		return 0;
	}
	if(strcmp(m->t.uname, unamenone) == 0){
		werrstr("user 'none' requires no authentication");
		fsysPut(fsys);
		return 0;
	}

	con = m->con;
	if((afid = fidGet(con, m->t.afid, FidFWlock|FidFCreate)) == nil){
		fsysPut(fsys);
		return 0;
	}
	afid->fsys = fsys;

#ifndef PLAN9PORT
	if((afd = open("/mnt/factotum/rpc", ORDWR)) < 0){
		werrstr("can't open \"/mnt/factotum/rpc\"");
		fidClunk(afid);
		return 0;
	}
#endif

#ifdef PLAN9PORT
	if((afid->rpc = auth_allocrpc()) == nil){
#else
	if((afid->rpc = auth_allocrpc(afd)) == nil){
		close(afd);
#endif
		werrstr("can't auth_allocrpc");
		fidClunk(afid);
		return 0;
	}
	if(auth_rpc(afid->rpc, "start", "proto=p9any role=server", 23) != ARok){
		werrstr("can't auth_rpc");
		fidClunk(afid);
		return 0;
	}

	afid->open = FidOWrite|FidORead;
	afid->qid.type = QTAUTH;
	afid->qid.path = m->t.afid;
	afid->uname = vtstrdup(m->t.uname);

	m->r.qid = afid->qid;

	fidPut(afid);
	return 1;
}

static int
rTversion(Msg* m)
{
	int v;
	Con *con;
	Fcall *r, *t;

	t = &m->t;
	r = &m->r;
	con = m->con;

	qlock(&con->lock);
	if(con->state != ConInit){
		qunlock(&con->lock);
		werrstr("Tversion: down");
		return 0;
	}
	con->state = ConNew;

	/*
	 * Release the karma of past lives and suffering.
	 * Should this be done before or after checking the
	 * validity of the Tversion?
	 */
	fidClunkAll(con);

	if(t->tag != NOTAG){
		qunlock(&con->lock);
		werrstr("Tversion: invalid tag");
		return 0;
	}

	if(t->msize < 256){
		qunlock(&con->lock);
		werrstr("Tversion: message size too small");
		return 0;
	}
	if(t->msize < con->msize)
		r->msize = t->msize;
	else
		r->msize = con->msize;

	r->version = "unknown";
	if(t->version[0] == '9' && t->version[1] == 'P'){
		/*
		 * Currently, the only defined version
		 * is "9P2000"; ignore any later versions.
		 */
		v = strtol(&t->version[2], 0, 10);
		if(v >= 2000){
			r->version = VERSION9P;
			con->msize = r->msize;
			con->state = ConUp;
		}
		else if(strcmp(t->version, "9PEoF") == 0){
			r->version = "9PEoF";
			con->msize = r->msize;
			con->state = ConMoribund;

			/*
			 * Don't want to attempt to write this
			 * message as the connection may be already
			 * closed.
			 */
			m->state = MsgF;
		}
	}
	qunlock(&con->lock);

	return 1;
}

int (*rFcall[Tmax])(Msg*) = {
	[Tversion]	= rTversion,
	[Tauth]		= rTauth,
	[Tattach]	= rTattach,
	[Tflush]	= rTflush,
	[Twalk]		= rTwalk,
	[Topen]		= rTopen,
	[Tcreate]	= rTcreate,
	[Tread]		= rTread,
	[Twrite]	= rTwrite,
	[Tclunk]	= rTclunk,
	[Tremove]	= rTremove,
	[Tstat]		= rTstat,
	[Twstat]	= rTwstat,
};
