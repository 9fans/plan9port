#include "stdinc.h"
#include "vac.h"
#include "dat.h"
#include "fns.h"

static char EBadVacFormat[] = "bad format for vac file";

static VacFs *
vacfsalloc(VtConn *z, int bsize, int ncache, int mode)
{
	VacFs *fs;

	fs = vtmallocz(sizeof(VacFs));
	fs->ref = 1;
	fs->z = z;
	fs->bsize = bsize;
	fs->cache = vtcachealloc(z, bsize, ncache, mode);
	return fs;
}

static int
readscore(int fd, uchar score[VtScoreSize])
{
	char buf[45], *pref;
	int n;

	n = readn(fd, buf, sizeof(buf)-1);
	if(n < sizeof(buf)-1) {
		werrstr("short read");
		return -1;
	}
	buf[n] = 0;

	if(vtparsescore(buf, &pref, score) < 0){
		werrstr(EBadVacFormat);
		return -1;
	}
	if(pref==nil || strcmp(pref, "vac") != 0) {
		werrstr("not a vac file");
		return -1;
	}
	return 0;
}

VacFs*
vacfsopen(VtConn *z, char *file, int mode, int ncache)
{
	int fd;
	uchar score[VtScoreSize];

	fd = open(file, OREAD);
	if(fd < 0)
		return nil;

	if(readscore(fd, score) < 0){
		close(fd);
		return nil;
	}
	close(fd);

	return vacfsopenscore(z, score, mode, ncache);
}

VacFs*
vacfsopenscore(VtConn *z, u8int *score, int mode, int ncache)
{
	VacFs *fs;
	int n;
	VtRoot rt;
	uchar buf[VtRootSize];
	VacFile *root;
	VtFile *r;
	VtEntry e;

	n = vtread(z, score, VtRootType, buf, VtRootSize);
	if(n < 0)
		return nil;
	if(n != VtRootSize){
		werrstr("vtread on root too short");
		return nil;
	}

	if(vtrootunpack(&rt, buf) < 0)
		return nil;

	if(strcmp(rt.type, "vac") != 0) {
		werrstr("not a vac root");
		return nil;
	}

	fs = vacfsalloc(z, rt.blocksize, ncache, mode);
	memmove(fs->score, score, VtScoreSize);
	fs->mode = mode;

	memmove(e.score, rt.score, VtScoreSize);
	e.gen = 0;
	e.psize = (rt.blocksize/VtEntrySize)*VtEntrySize;
	e.dsize = rt.blocksize;
	e.type = VtDirType;
	e.flags = VtEntryActive;
	e.size = 3*VtEntrySize;

	root = nil;
	if((r = vtfileopenroot(fs->cache, &e)) == nil)
		goto Err;

	root = _vacfileroot(fs, r);
	vtfileclose(r);
	if(root == nil)
		goto Err;
	fs->root = root;

	return fs;
Err:
	if(root)
		vacfiledecref(root);
	vacfsclose(fs);
	return nil;
}

VacFs *
vacfscreate(VtConn *z, int bsize, int ncache)
{
	return vacfsalloc(z, bsize, ncache, VtORDWR);
}

int
vacfsmode(VacFs *fs)
{
	return fs->mode;
}

VacFile*
vacfsgetroot(VacFs *fs)
{
	return vacfileincref(fs->root);
}

int
vacfsgetblocksize(VacFs *fs)
{
	return fs->bsize;
}

int
vacfsgetscore(VacFs *fs, u8int *score)
{
	memmove(score, fs->score, VtScoreSize);
	return 0;
}

int
_vacfsnextqid(VacFs *fs, uvlong *qid)
{
	++fs->qid;
	*qid = fs->qid;
	return 0;
}

int
vacfssync(VacFs *fs)
{
	return 0;
}

void
vacfsclose(VacFs *fs)
{
	if(fs->root)
		vacfiledecref(fs->root);
	fs->root = nil;
	vtcachefree(fs->cache);
	vtfree(fs);
}

