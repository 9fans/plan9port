typedef struct VacFs VacFs;
typedef struct VacDir VacDir;
typedef struct VacFile VacFile;
typedef struct VacDirEnum VacDirEnum;

/*
 * Mode bits
 */
enum
{
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
	ModeDevice = (1<<21),		/* Unix device */
	ModeNamedPipe = (1<<22),	/* Unix named pipe */
};

enum
{
	MetaMagic = 0x5656fc79,
	MetaHeaderSize = 12,
	MetaIndexSize = 4,
	IndexEntrySize = 8,
	DirMagic = 0x1c4d9072,
};

enum
{
	DirPlan9Entry = 1,	/* not valid in version >= 9 */
	DirNTEntry,		/* not valid in version >= 9 */
	DirQidSpaceEntry,
	DirGenEntry,		/* not valid in version >= 9 */
};

struct VacDir
{
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
	int qidspace;
	uvlong qidoffset;	/* qid offset */
	uvlong qidmax;		/* qid maximum */
};


struct VacFs
{
	int		ref;
	uchar	score[VtScoreSize];
	VacFile	*root;
	VtConn	*z;
	int		mode;
	int		bsize;
	uvlong	qid;
	VtCache	*cache;
};

VacFs	*vacfsopen(VtConn *z, char *file, int mode, int ncache);
VacFs	*vacfsopenscore(VtConn *z, u8int *score, int mode, int ncache);
VacFs	*vacfscreate(VtConn *z, int bsize, int ncache);
void		vacfsclose(VacFs *fs);
int		vacfssync(VacFs *fs);
int		vacfssnapshot(VacFs *fs, char *src, char *dst);
int		vacfsgetscore(VacFs *fs, u8int *score);

/* 
 * other ideas
 *
 * VacFs *vfsSnapshot(VacFs*, char *src);
 * int vfsGraft(VacFs*, char *name, VacFs*);
 */

VacFile *vacfsgetroot(VacFs *fs);
VacFile	*vacfileopen(VacFs *fs, char *path);
VacFile	*vacfilecreate(VacFile *file, char *elem, ulong perm, char *muid);
VacFile	*vacfilewalk(VacFile *file, char *elem);
int		vacfileremove(VacFile *file, char *muid);
int		vacfileread(VacFile *file, void *buf, int n, vlong offset);
int		vacfileblockscore(VacFile *file, u32int, u8int*);
int		vacfilewrite(VacFile *file, void *buf, int n, vlong offset, char *muid);
int		vacfilereadpacket(VacFile *file, Packet **pp, vlong offset);
int		vacfilewritepacket(VacFile *file, Packet *p, vlong offset, char *muid);
uvlong	vacfilegetid(VacFile *file);
ulong	vacfilegetmcount(VacFile *file);
int		vacfileisdir(VacFile *file);
int		vacfileisroot(VacFile *file);
int		vacfilegetmode(VacFile *file);
int		vacfilegetblocksize(VacFile *file, u32int bn, u8int *score);
int		vacfilegetsize(VacFile *file, uvlong *size);
int		vacfilegetdir(VacFile *file, VacDir *dir);
int		vacfilesetdir(VacFile *file, VacDir *dir, char *muid);
int		vacfilegetvtentry(VacFile *file, VtEntry *entry);
VacFile	*vacfilegetparent(VacFile *file);
int		vacfilesync(VacFile*);
VacFile	*vacfileincref(VacFile*);
int		vacfiledecref(VacFile*);

void		vdcleanup(VacDir *dir);
void		vdcopy(VacDir *dst, VacDir *src);


VacDirEnum	*vdeopen(VacFile*);
int			vderead(VacDirEnum*, VacDir *);
void			vdeclose(VacDirEnum*);

