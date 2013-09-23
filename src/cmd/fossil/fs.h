typedef struct Fs Fs;
typedef struct File File;
typedef struct DirEntryEnum DirEntryEnum;

#pragma incomplete Fs
#pragma incomplete File
#pragma incomplete DirEntryEnum

/* modes */

enum {
	OReadOnly,
	OReadWrite,
	OOverWrite,
};

extern char *currfsysname;
extern char *foptname;

void	fsClose(Fs*);
int	fsEpochLow(Fs*, u32int);
File	*fsGetRoot(Fs*);
int	fsHalt(Fs*);
Fs	*fsOpen(char*, VtSession*, long, int);
int	fsRedial(Fs*, char*);
void	fsSnapshotCleanup(Fs*, u32int);
int	fsSnapshot(Fs*, char*, char*, int);
void	fsSnapshotRemove(Fs*);
int	fsSync(Fs*);
int	fsUnhalt(Fs*);
int	fsVac(Fs*, char*, uchar[VtScoreSize]);

void	deeClose(DirEntryEnum*);
DirEntryEnum *deeOpen(File*);
int	deeRead(DirEntryEnum*, DirEntry*);
int	fileClri(File*, char*, char*);
int	fileClriPath(Fs*, char*, char*);
File	*fileCreate(File*, char*, ulong, char*);
int	fileDecRef(File*);
int	fileGetDir(File*, DirEntry*);
uvlong	fileGetId(File*);
ulong	fileGetMcount(File*);
ulong	fileGetMode(File*);
File	*fileGetParent(File*);
int	fileGetSize(File*, uvlong*);
File	*fileIncRef(File*);
int	fileIsDir(File*);
int	fileIsTemporary(File*);
int	fileIsAppend(File*);
int	fileIsExclusive(File*);
int	fileIsRoFs(File*);
int	fileIsRoot(File*);
int	fileMapBlock(File*, ulong, uchar[VtScoreSize], ulong);
int	fileMetaFlush(File*, int);
char	*fileName(File *f);
File	*fileOpen(Fs*, char*);
int	fileRead(File*, void *, int, vlong);
int	fileRemove(File*, char*);
int	fileSetDir(File*, DirEntry*, char*);
int	fileSetQidSpace(File*, u64int, u64int);
int	fileSetSize(File*, uvlong);
int	fileSync(File*);
int	fileTruncate(File*, char*);
File	*fileWalk(File*, char*);
File	*_fileWalk(File*, char*, int);
int	fileWalkSources(File*);
int	fileWrite(File*, void *, int, vlong, char*);
