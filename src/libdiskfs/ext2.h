typedef struct Super Super;
typedef struct Group Group;
typedef struct Inode Inode;
typedef struct Dirent Dirent;
typedef struct Ext2 Ext2;

enum
{
	BYTESPERSEC	= 512,

	SBOFF = 1024,
	SBSIZE = 1024,

	SUPERMAGIC = 0xEF53,
	MINBLOCKSIZE = 1024,
	MAXBLOCKSIZE = 4096,
	ROOTINODE = 2,
	FIRSTINODE = 11,
	VALIDFS = 0x0001,
	ERRORFS = 0x0002,

	NDIRBLOCKS = 12,
	INDBLOCK = NDIRBLOCKS,
	DINDBLOCK = INDBLOCK+1,
	TINDBLOCK = DINDBLOCK+1,
	NBLOCKS = TINDBLOCK+1,

	NAMELEN = 255,

	/* some systems have these defined */
	#undef IEXEC
	#undef IWRITE
	#undef IREAD
	#undef ISVTX
	#undef ISGID
	#undef ISUID
	#undef IFMT
	#undef IFIFO
	#undef IFCHR
	#undef IFDIR
	#undef IFBLK
	#undef IFREG
	#undef IFLNK
	#undef IFSOCK
	#undef IFWHT

	#define IEXEC	EXT2_IEXEC
	#define IWRITE	EXT2_IWRITE
	#define IREAD	EXT2_IREAD
	#define ISVTX	EXT2_ISVTX
	#define ISGID	EXT2_ISGID
	#define ISUID	EXT2_ISUID
	#define IFMT	EXT2_IFMT
	#define IFIFO	EXT2_IFIFO
	#define IFCHR	EXT2_IFCHR
	#define IFDIR	EXT2_IFDIR
	#define IFBLK	EXT2_IFBLK
	#define IFREG	EXT2_IFREG
	#define IFLNK	EXT2_IFLNK
	#define IFSOCK	EXT2_IFSOCK
	#define IFWHT	EXT2_IFWHT

	/* permissions in Inode.mode */
	IEXEC = 00100,
	IWRITE = 0200,
	IREAD = 0400,
	ISVTX = 01000,
	ISGID = 02000,
	ISUID = 04000,

	/* type in Inode.mode */
	IFMT = 0170000,
	IFIFO = 0010000,
	IFCHR = 0020000,
	IFDIR = 0040000,
	IFBLK = 0060000,
	IFREG = 0100000,
	IFLNK = 0120000,
	IFSOCK = 0140000,
	IFWHT = 0160000
};

#define DIRLEN(namlen)	(((namlen)+8+3)&~3)


/*
 * Super block
 */
struct Super
{
	u32int	ninode;		/* Inodes count */
	u32int	nblock;		/* Blocks count */
	u32int	rblockcount;	/* Reserved blocks count */
	u32int	freeblockcount;	/* Free blocks count */
	u32int	freeinodecount;	/* Free inodes count */
	u32int	firstdatablock;	/* First Data Block */
	u32int	logblocksize;	/* Block size */
	u32int	logfragsize;	/* Fragment size */
	u32int	blockspergroup;	/* # Blocks per group */
	u32int	fragpergroup;	/* # Fragments per group */
	u32int	inospergroup;	/* # Inodes per group */
	u32int	mtime;		/* Mount time */
	u32int	wtime;		/* Write time */
	u16int	mntcount;		/* Mount count */
	u16int	maxmntcount;	/* Maximal mount count */
	u16int	magic;		/* Magic signature */
	u16int	state;		/* File system state */
	u16int	errors;		/* Behaviour when detecting errors */
	u16int	pad;
	u32int	lastcheck;		/* time of last check */
	u32int	checkinterval;	/* max. time between checks */
	u32int	creatoros;		/* OS */
	u32int	revlevel;		/* Revision level */
	u16int	defresuid;		/* Default uid for reserved blocks */
	u16int	defresgid;		/* Default gid for reserved blocks */

	/* the following are only available with revlevel = 1 */
	u32int	firstino;		/* First non-reserved inode */
	u16int	inosize;		/* size of inode structure */
	u16int	blockgroupnr;	/* block group # of this super block */
	u32int	reserved[233];	/* Padding to the end of the block */
};

/*
 * Block group
 */
struct Group
{
	u32int	bitblock;		/* Blocks bitmap block */
	u32int	inodebitblock;		/* Inodes bitmap block */
	u32int	inodeaddr;		/* Inodes table block */
	u16int	freeblockscount;	/* Free blocks count */
	u16int	freeinodescount;	/* Free inodes count */
	u16int	useddirscount;	/* Directories count */
};
enum
{
	GroupSize = 32
};

/*
 * Inode
 */
struct Inode
{
	u16int	mode;		/* File mode */
	u16int	uid;		/* Owner Uid */
	u32int	size;		/* Size in bytes */
	u32int	atime;		/* Access time */
	u32int	ctime;		/* Creation time */
	u32int	mtime;		/* Modification time */
	u32int	dtime;		/* Deletion Time */
	u16int	gid;		/* Group Id */
	u16int	nlink;	/* Links count */
	u32int	nblock;	/* Blocks count */
	u32int	flags;		/* File flags */
	u32int	block[NBLOCKS];/* Pointers to blocks */
	u32int	version;	/* File version (for NFS) */
	u32int	fileacl;	/* File ACL */
	u32int	diracl;	/* Directory ACL or high size bits */
	u32int	faddr;		/* Fragment address */
};

/*
 * Directory entry
 */
struct Dirent
{
	u32int	ino;			/* Inode number */
	u16int	reclen;		/* Directory entry length */
	u8int	namlen;		/* Name length */
	char	*name;	/* File name */
};
enum
{
	MinDirentSize = 4+2+1+1
};

/*
 * In-core fs info.
 */
struct Ext2
{
	uint	blocksize;
	uint	nblock;
	uint	ngroup;
	uint	inospergroup;
	uint	blockspergroup;
	uint	inosperblock;
	uint	inosize;
	uint	groupaddr;
	uint	descperblock;
	uint	firstblock;
	Disk *disk;
	Fsys *fsys;
};
