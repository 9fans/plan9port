typedef struct VacFS VacFS;
typedef struct VacDir VacDir;
typedef struct VacFile VacFile;
typedef struct VacDirEnum VacDirEnum;

/*
 * Mode bits
 */
enum {
	ModeOtherExec = (1<<0),		
	ModeOtherWrite = (1<<1),
	ModeOtherRead = (1<<2),
	ModeGroupExec = (1<<3),
	ModeGroupWrite = (1<<4),
	ModeGroupRead = (1<<5),
	ModeOwnerExec = (1<<6),
	ModeOwnerWrite = (1<<7),
	ModeOwnerRead = (1<<8),
	ModeSticky = (1<<9),
	ModeSetUid = (1<<10),
	ModeSetGid = (1<<11),
	ModeAppend = (1<<12),		/* append only file */
	ModeExclusive = (1<<13),	/* lock file - plan 9 */
	ModeLink = (1<<14),		/* sym link */
	ModeDir	= (1<<15),		/* duplicate of DirEntry */
	ModeHidden = (1<<16),		/* MS-DOS */
	ModeSystem = (1<<17),		/* MS-DOS */
	ModeArchive = (1<<18),		/* MS-DOS */
	ModeTemporary = (1<<19),	/* MS-DOS */
	ModeSnapshot = (1<<20),		/* read only snapshot */
};

enum {
	MetaMagic = 0x5656fc79,
	MetaHeaderSize = 12,
	MetaIndexSize = 4,
	IndexEntrySize = 8,
	DirMagic = 0x1c4d9072,
};

enum {
	DirPlan9Entry = 1,	/* not valid in version >= 9 */
	DirNTEntry,		/* not valid in version >= 9 */
	DirQidSpaceEntry,
	DirGenEntry,		/* not valid in version >= 9 */
};

struct VacDir {
	char *elem;		/* path element */
	ulong entry;		/* entry in directory for data */
	ulong gen;		/* generation of data entry */
	ulong mentry;		/* entry in directory for meta */
	ulong mgen;		/* generation of meta entry */
	uvlong size;		/* size of file */
	uvlong qid;		/* unique file id */
	
	char *uid;		/* owner id */
	char *gid;		/* group id */
	char *mid;		/* last modified by */
	ulong mtime;		/* last modified time */
	ulong mcount;		/* number of modifications: can wrap! */
	ulong ctime;		/* directory entry last changed */
	ulong atime;		/* last time accessed */
	ulong mode;		/* various mode bits */

	/* plan 9 */
	int plan9;
	uvlong p9path;
	ulong p9version;

	/* sub space of qid */
	int qidSpace;
	uvlong qidOffset;	/* qid offset */
	uvlong qidMax;		/* qid maximum */
};

VacFS *vfsOpen(VtSession *z, char *file, int readOnly, long ncache);
VacFS *vfsCreate(VtSession *z, int bsize, long ncache);
int vfsGetBlockSize(VacFS*);
int vfsIsReadOnly(VacFS*);
VacFile *vfsGetRoot(VacFS*);

long vfsGetCacheSize(VacFS*);
int vfsSetCacheSize(VacFS*, long);
int vfsSnapshot(VacFS*, char *src, char *dst);
int vfsSync(VacFS*);
int vfsClose(VacFS*);
int vfsGetScore(VacFS*, uchar score[VtScoreSize]);

/* 
 * other ideas
 *
 * VacFS *vfsSnapshot(VacFS*, char *src);
 * int vfsGraft(VacFS*, char *name, VacFS*);
 */

VacFile *vfOpen(VacFS*, char *path);
VacFile *vfCreate(VacFile*, char *elem, ulong perm, char *user);
VacFile *vfWalk(VacFile*, char *elem);
int vfRemove(VacFile*, char*);
int vfRead(VacFile*, void *, int n, vlong offset);
int vfWrite(VacFile*, void *, int n, vlong offset, char *user);
int vfReadPacket(VacFile*, Packet**, vlong offset);
int vfWritePacket(VacFile*, Packet*, vlong offset, char *user);
uvlong vfGetId(VacFile*);
ulong vfGetMcount(VacFile*);
int vfIsDir(VacFile*);
int vfGetBlockScore(VacFile*, ulong bn, uchar score[VtScoreSize]);
int vfGetSize(VacFile*, uvlong *size);
int vfGetDir(VacFile*, VacDir*);
int vfSetDir(VacFile*, VacDir*);
int vfGetVtEntry(VacFile*, VtEntry*);
VacFile *vfGetParent(VacFile*);
int vfSync(VacFile*);
VacFile *vfIncRef(VacFile*);
void vfDecRef(VacFile*);
VacDirEnum *vfDirEnum(VacFile*);
int vfIsRoot(VacFile *vf);

void	vdCleanup(VacDir *dir);
void	vdCopy(VacDir *dst, VacDir *src);

VacDirEnum *vdeOpen(VacFS*, char *path);
int vdeRead(VacDirEnum*, VacDir *, int n);
void vdeFree(VacDirEnum*);

