#include "stdinc.h"

#include "9.h"

/* one entry buffer for reading directories */
struct DirBuf {
	DirEntryEnum*	dee;
	int		valid;
	DirEntry	de;
};

static DirBuf*
dirBufAlloc(File* file)
{
	DirBuf *db;

	db = vtMemAllocZ(sizeof(DirBuf));
	db->dee = deeOpen(file);
	if(db->dee == nil){
		/* can happen if dir is removed from under us */
		vtMemFree(db);
		return nil;
	}
	return db;
}

void
dirBufFree(DirBuf* db)
{
	if(db == nil)
		return;

	if(db->valid)
		deCleanup(&db->de);
	deeClose(db->dee);
	vtMemFree(db);
}

int
dirDe2M(DirEntry* de, uchar* p, int np)
{
	int n;
	Dir dir;

	memset(&dir, 0, sizeof(Dir));

	dir.qid.path = de->qid;
	dir.qid.vers = de->mcount;
	dir.mode = de->mode & 0777;
	if(de->mode & ModeAppend){
		dir.qid.type |= QTAPPEND;
		dir.mode |= DMAPPEND;
	}
	if(de->mode & ModeExclusive){
		dir.qid.type |= QTEXCL;
		dir.mode |= DMEXCL;
	}
	if(de->mode & ModeDir){
		dir.qid.type |= QTDIR;
		dir.mode |= DMDIR;
	}
	if(de->mode & ModeSnapshot){
		dir.qid.type |= QTMOUNT;	/* just for debugging */
		dir.mode |= DMMOUNT;
	}
	if(de->mode & ModeTemporary){
		dir.qid.type |= QTTMP;
		dir.mode |= DMTMP;
	}

	dir.atime = de->atime;
	dir.mtime = de->mtime;
	dir.length = de->size;

	dir.name = de->elem;
	if((dir.uid = unameByUid(de->uid)) == nil)
		dir.uid = smprint("(%s)", de->uid);
	if((dir.gid = unameByUid(de->gid)) == nil)
		dir.gid = smprint("(%s)", de->gid);
	if((dir.muid = unameByUid(de->mid)) == nil)
		dir.muid = smprint("(%s)", de->mid);

	n = convD2M(&dir, p, np);

	vtMemFree(dir.muid);
	vtMemFree(dir.gid);
	vtMemFree(dir.uid);

	return n;
}

int
dirRead(Fid* fid, uchar* p, int count, vlong offset)
{
	int n, nb;
	DirBuf *db;

	/*
	 * special case of rewinding a directory
	 * otherwise ignore the offset
	 */
	if(offset == 0 && fid->db){
		dirBufFree(fid->db);
		fid->db = nil;
	}

	if(fid->db == nil){
		fid->db = dirBufAlloc(fid->file);
		if(fid->db == nil)
			return -1;
	}

	db = fid->db;

	for(nb = 0; nb < count; nb += n){
		if(!db->valid){
			n = deeRead(db->dee, &db->de);
			if(n < 0)
				return -1;
			if(n == 0)
				break;
			db->valid = 1;
		}
		n = dirDe2M(&db->de, p+nb, count-nb);
		if(n <= BIT16SZ)
			break;
		db->valid = 0;
		deCleanup(&db->de);
	}

	return nb;
}
