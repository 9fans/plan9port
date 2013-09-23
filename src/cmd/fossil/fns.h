Block*	sourceBlock(Source*, ulong, int);
Block*	_sourceBlock(Source*, ulong, int, int, ulong);
void	sourceClose(Source*);
Source*	sourceCreate(Source*, int, int, u32int);
ulong	sourceGetDirSize(Source*);
int	sourceGetEntry(Source*, Entry*);
uvlong	sourceGetSize(Source*);
int	sourceLock2(Source*, Source*, int);
int	sourceLock(Source*, int);
char	*sourceName(Source *s);
Source*	sourceOpen(Source*, ulong, int, int);
int	sourceRemove(Source*);
Source*	sourceRoot(Fs*, u32int, int);
int	sourceSetDirSize(Source*, ulong);
int	sourceSetEntry(Source*, Entry*);
int	sourceSetSize(Source*, uvlong);
int	sourceTruncate(Source*);
void	sourceUnlock(Source*);

Block*	cacheAllocBlock(Cache*, int, u32int, u32int, u32int);
Cache*	cacheAlloc(Disk*, VtSession*, ulong, int);
void	cacheCountUsed(Cache*, u32int, u32int*, u32int*, u32int*);
int	cacheDirty(Cache*);
void	cacheFlush(Cache*, int);
void	cacheFree(Cache*);
Block*	cacheGlobal(Cache*, uchar[VtScoreSize], int, u32int, int);
Block*	cacheLocal(Cache*, int, u32int, int);
Block*	cacheLocalData(Cache*, u32int, int, u32int, int, u32int);
u32int	cacheLocalSize(Cache*, int);
int	readLabel(Cache*, Label*, u32int addr);

Block*	blockCopy(Block*, u32int, u32int, u32int);
void	blockDependency(Block*, Block*, int, uchar*, Entry*);
int	blockDirty(Block*);
void	blockDupLock(Block*);
void	blockPut(Block*);
void	blockRemoveLink(Block*, u32int, int, u32int, int);
uchar*	blockRollback(Block*, uchar*);
void	blockSetIOState(Block*, int);
Block*	_blockSetLabel(Block*, Label*);
int	blockSetLabel(Block*, Label*, int);
int	blockWrite(Block*, int);

Disk*	diskAlloc(int);
int	diskBlockSize(Disk*);
int	diskFlush(Disk*);
void	diskFree(Disk*);
void	diskRead(Disk*, Block*);
int	diskReadRaw(Disk*, int, u32int, uchar*);
u32int	diskSize(Disk*, int);
void	diskWriteAndWait(Disk*,	Block*);
void	diskWrite(Disk*, Block*);
int	diskWriteRaw(Disk*, int, u32int, uchar*);

char*	bioStr(int);
char*	bsStr(int);
char*	btStr(int);
u32int	globalToLocal(uchar[VtScoreSize]);
void	localToGlobal(u32int, uchar[VtScoreSize]);

void	headerPack(Header*, uchar*);
int	headerUnpack(Header*, uchar*);

int	labelFmt(Fmt*);
void	labelPack(Label*, uchar*, int);
int	labelUnpack(Label*, uchar*, int);

int	scoreFmt(Fmt*);

void	superPack(Super*, uchar*);
int	superUnpack(Super*, uchar*);

void	entryPack(Entry*, uchar*, int);
int	entryType(Entry*);
int	entryUnpack(Entry*, uchar*, int);

Periodic* periodicAlloc(void (*)(void*), void*, int);
void	periodicKill(Periodic*);

int	fileGetSources(File*, Entry*, Entry*);
File*	fileRoot(Source*);
int	fileSnapshot(File*, File*, u32int, int);
int	fsNextQid(Fs*, u64int*);
int	mkVac(VtSession*, uint, Entry*, Entry*, DirEntry*, uchar[VtScoreSize]);
Block*	superGet(Cache*, Super*);

void	archFree(Arch*);
Arch*	archInit(Cache*, Disk*, Fs*, VtSession*);
void	archKick(Arch*);

void	bwatchDependency(Block*);
void	bwatchInit(void);
void	bwatchLock(Block*);
void	bwatchReset(uchar[VtScoreSize]);
void	bwatchSetBlockSize(uint);
void	bwatchUnlock(Block*);

void	initWalk(WalkPtr*, Block*, uint);
int	nextWalk(WalkPtr*, uchar[VtScoreSize], uchar*, u32int*, Entry**);

void	snapGetTimes(Snap*, u32int*, u32int*, u32int*);
void	snapSetTimes(Snap*, u32int, u32int, u32int);

void	fsCheck(Fsck*);

#pragma varargck type "L" Label*
