/*
 * An FFS file system is a sequence of cylinder groups.
 *
 * Each cylinder group is laid out as follows:
 *
 *	fs superblock (Fsblk)
 *	cylinder group block (Cgblk)
 *	inodes
 *	data
 *
 * The location of the fs superblock in the first cylinder
 * group is known.  The rest of the info about cylinder group
 * layout can be derived from the super block.
 */

#define daddr_t u32int
#define time_t u32int

typedef struct Cgblk Cgblk;
typedef struct Cylgrp Cylgrp;
typedef struct Cylsum Cylsum;
typedef struct Ffs Ffs;
typedef struct Fsblk Fsblk;
typedef struct Inode Inode;
typedef struct Dirent Dirent;

enum
{
	BYTESPERSEC = 512,

	/* constants for Fsblk */
	FSMAXMNTLEN = 512,
	FSNOCSPTRS = 128 / sizeof(void*) - 3,
	FSMAXSNAP = 20,
	FSMAGIC = 0x011954,
	FSCHECKSUM = 0x7c269d38,
	
	/* Fsblk.inodefmt */
	FS42INODEFMT = -1,
	FS44INODEFMT = 2,

	/* offset and size of first boot block */
	BBOFF = 0,
	BBSIZE = 8192,

	/* offset and size of first super block */
	SBOFF = BBOFF+BBSIZE,
	SBSIZE = 8192,

	/* minimum block size */
	MINBSIZE = 4096,

	/* maximum fragments per block */
	MAXFRAG = 8,

	/* constants for Cgblk */
	CGMAGIC = 0x090255,

	/* inode-related */
	ROOTINODE = 2,
	WHITEOUT = 1,

	NDADDR = 12,
	NIADDR = 3,

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
	IFWHT = 0160000,

	/* type in Dirent.type */
	DTUNKNOWN = 0,
	DTFIFO = 1,
	DTCHR = 2,
	DTDIR = 4,
	DTBLK = 6,
	DTREG = 8,
	DTLNK = 10,
	DTSOCK = 12,
	DTWHT = 14,
};

struct Cylsum
{
	u32int	ndir;
	u32int	nbfree;
	u32int	nifree;
	u32int	nffree;
};

struct Fsblk
{
	u32int	unused0;
	u32int	unused1;
	daddr_t	sfragno;		/* fragment address of super block in file system */
	daddr_t	cfragno;		/* fragment address if cylinder block in file system */
	daddr_t	ifragno;		/* fragment offset of inode blocks in file system */
	daddr_t	dfragno;		/* fragment offset of data blocks in cg */
	u32int	cgoffset;		/* block (maybe fragment?) offset of Cgblk in cylinder */
	u32int	cgmask;
	time_t	time;
	u32int	nfrag;		/* number of blocks in fs * fragsperblock */
	u32int	ndfrag;
	u32int	ncg;			/* number of cylinder groups in fs */
	u32int	blocksize;		/* block size in fs */
	u32int	fragsize;		/* frag size in fs */
	u32int	fragsperblock;	/* fragments per block: blocksize / fragsize */
	u32int	minfree;		/* ignored by us */
	u32int	rotdelay;		/* ... */
	u32int	rps;
	u32int	bmask;
	u32int	fmask;
	u32int	bshift;
	u32int	fshift;
	u32int	maxcontig;
	u32int	maxbpg;
	u32int	fragshift;
	u32int	fsbtodbshift;
	u32int	sbsize;		/* size of super block */
	u32int	unused2;		/* more stuff we don't use ... */
	u32int	unused3;
	u32int	nindir;
	u32int	inosperblock;	/* inodes per block */
	u32int	nspf;
	u32int	optim;
	u32int	npsect;
	u32int	interleave;
	u32int	trackskew;
	u32int	id[2];
	daddr_t	csaddr;		/* blk addr of cyl grp summary area */
	u32int	cssize;		/* size of cyl grp summary area */
	u32int	cgsize;		/* cylinder group size */
	u32int	trackspercyl;	/* tracks per cylinder */
	u32int	secspertrack;	/* sectors per track */
	u32int	secspercyl;	/* sectors per cylinder */
	u32int	ncyl;			/* cylinders in fs */
	u32int	cylspergroup;	/* cylinders per group */
	u32int	inospergroup;	/* inodes per group */
	u32int	fragspergroup;	/* data blocks per group * fragperblock */
	Cylsum	cstotal;		/* more unused... */
	u8int	fmod;
	u8int	clean;
	u8int	ronly;
	u8int	flags;
	char		fsmnt[FSMAXMNTLEN];
	u32int	cgrotor;
	void*	ocsp[FSNOCSPTRS];
	u8int*	contigdirs;
	Cylsum*	csp;
	u32int*	maxcluster;
	u32int	cpc;
	u16int	opostbl[16][8];
	u32int	snapinum[FSMAXSNAP];
	u32int	avgfilesize;
	u32int	avgfpdir;
	u32int	sparecon[26];
	u32int	pendingblocks;
	u32int	pendinginodes;
	u32int	contigsumsize;
	u32int	maxsymlinklen;
	u32int	inodefmt;		/* format of on-disk inodes */
	u64int	maxfilesize;	/* maximum representable file size */
	u64int	qbmask;
	u64int	qfmask;
	u32int	state;
	u32int	postblformat;
	u32int	nrpos;
	u32int	postbloff;
	u32int	rotbloff;
	u32int	magic;		/* FS_MAGIC */
};

/*
 * Cylinder group block for a file system.
 */
struct Cgblk
{
	u32int	unused0;
	u32int	magic;		/* CGMAGIC */
	u32int	time;			/* time last written */
	u32int	num;		/* we are cg #cgnum */
	u16int	ncyl;			/* number of cylinders in gp */
	u16int	nino;		/* number of inodes */
	u32int	nfrag;		/* number of fragments  */
	Cylsum	csum;
	u32int	rotor;
	u32int	frotor;
	u32int	irotor;
	u32int	frsum[MAXFRAG];	/* counts of available frags */
	u32int	btotoff;
	u32int	boff;
	u32int	imapoff;		/* offset to used inode map */
	u32int	fmapoff;		/* offset to free fragment map */
	u32int	nextfrag;		/* next free fragment */
	u32int	csumoff;
	u32int	clusteroff;
	u32int	ncluster;
	u32int	sparecon[13];
};

struct Cylgrp
{
	/* these are block numbers not fragment numbers */
	u32int	bno;			/* disk block address of start of cg */
	u32int	ibno;			/* disk block address of first inode */
	u32int	dbno;		/* disk block address of first data */
	u32int	cgblkno;
};

/*
 * this is the on-disk structure
 */
struct Inode
{
	u16int	mode;
	u16int	nlink;
	u32int	unused;
	u64int	size;
	u32int	atime;
	u32int	atimensec;
	u32int	mtime;
	u32int	mtimensec;
	u32int	ctime;
	u32int	ctimensec;
	/* rdev is db[0] */
	u32int	db[NDADDR];
	u32int	ib[NIADDR];
	u32int	flags;
	u32int	nblock;
	u32int	gen;
	u32int	uid;
	u32int	gid;
	u32int	spare[2];
};

struct Dirent
{
	u32int	ino;
	u16int	reclen;
	u8int	type;
	u8int	namlen;
	char		name[1];
};

/*
 * main file system structure
 */
struct Ffs
{
	int		blocksize;
	int		nblock;
	int		fragsize;
	int		fragsperblock;
	int		inosperblock;
	int		blockspergroup;
	int		fragspergroup;
	int		inospergroup;

	u32int	nfrag;
	u32int	ndfrag;

	int		ncg;
	Cylgrp	*cg;

	Disk		*disk;
};

