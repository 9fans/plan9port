/*
	Supports HFS Plus and HFSX file systems with or without an HFS
	wrapper.

	Apple technical note 1150 documents the file system:

	http://developer.apple.com/technotes/tn/tn1150.html

	Briefly an hfs file system comprises a volume header, an
	optional journal, and a set of forks.

	Most fs metadata resides in forks including a block allocation
	bitmap, a tree storing extents (q.v.) for forks and bad disk
	blocks, and a tree storing catalog (file and directory)
	information.

	An extent comprises a starting block number and block count.
	The fs maintains a list of k*NEXTENTS extents for each fork.
	These are used to map fork block numbers to disk block
	numbers.  A fork's initial extents are in its catalog record
	or (for fs forks) the volume header.  The rest are in the
	extents tree.

	Fs trees are layed out (in a fork) as an array of fixed-size
	nodes.  A node comprises a header, a sorted list of
	variable-size records, and trailing record offsets.  The
	records in interior nodes map keys to (child) node numbers.
	The records in leaf nodes map keys to data.  The nodes at each
	level in a tree are also sorted via (sibling) node numbers
	stored in each node header.
*/

typedef struct Extent Extent;
typedef struct Fork Fork;
typedef struct Inode Inode;
typedef struct Tree Tree;
typedef struct Node Node;
typedef struct Treeref Treeref;
typedef struct Key Key;
typedef struct Extentkey Extentkey;
typedef struct Name Name;
typedef struct Catalogkey Catalogkey;
typedef struct Hfs Hfs;

enum
{
	Hfssig = 0x4244,
	Hfsplussig = 0x482B,
	Hfsxsig = 0x4858,
 	Hfsplusmagic = (Hfsplussig<<16)|4,
 	Hfsxmagic = (Hfsxsig<<16)|5,

	NAMELEN = 255,
	UTFNAMELEN = NAMELEN*UTFmax,

	NEXTENTS = 8,

	Dfork = 0, Rfork = 255,

	/* fixed cnids */
	RootpId = 1, RootId, ExtentsId, CatalogId,
	BadblockId, AllocId, MinuserId = 16,

	/* size of a few structures on disk */
	Extentlen = 8,		/* Extent */
	Ndlen = 14,		/* Node */
	Folderlen = 88, Filelen = 248,		/* Inode */
	Adlen = 82,		/* Apple double header */
		Fioff = 50,
	Filen = 32,		/* Finder info */

	/* values in Node.type */
	LeafNode = -1, IndexNode, HeaderNode, MapNode,

	/* catalog record types */
	Folder = 1, File, FolderThread, FileThread,

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

	#define IEXEC	HFS_IEXEC
	#define IWRITE	HFS_IWRITE
	#define IREAD	HFS_IREAD
	#define ISVTX	HFS_ISVTX
	#define ISGID	HFS_ISGID
	#define ISUID	HFS_ISUID
	#define IFMT	HFS_IFMT
	#define IFIFO	HFS_IFIFO
	#define IFCHR	HFS_IFCHR
	#define IFDIR	HFS_IFDIR
	#define IFBLK	HFS_IFBLK
	#define IFREG	HFS_IFREG
	#define IFLNK	HFS_IFLNK
	#define IFSOCK	HFS_IFSOCK
	#define IFWHT	HFS_IFWHT

	/* permissions in Inode.mode */
	IEXEC = 00100,
	IWRITE = 0200,
	IREAD = 0400,
	ISTXT = 01000,
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
};

struct Extent
{
	u32int	start;		/* first block in extent */
	u32int	count;		/* number of blocks in extent */
};

struct Fork
{
	u32int	cnid;		/* catalog node id (in memory only) */
	int	type;		/* Dfork or Rfork (in memory only) */
	u64int	size;		/* size in bytes */
	u32int	nblocks;
	Extent	extent[NEXTENTS];		/* initial extents */
};

/*
 * In-core catalog record for a file or folder.
 */
struct Inode
{
	u32int	cnid;
	u64int	fileid;		/* in memory only */
	u32int	mtime;		/* modification */
	u32int	ctime;		/* attribute modification */
	u32int	atime;		/* access */
	u32int	nlink;		/* in memory only */
	u32int	uid;
	u32int	gid;
	int	mode;
	u32int	special;
	union{
		u32int	nentries;		/* directories */
		struct{		/* files */
			Fork	dfork;
			Fork	rfork;
			uchar	info[Filen];

			/* in memory only */
			int	nhdr;		/* 0 or Adlen */
			Fork	*fork;		/* dfork or rfork */
		} f;
	} u;
};

struct Tree
{
	int	nodesize;		/* node size in bytes */
	u32int	nnodes;		/* number of nodes in tree */
	u32int	root;		/* node number of the tree's root */
	int	height;
	int	maxkeylen;		/* maximum key size in bytes */
	int	indexkeylen;		/* 0 or length of index node keys */
	int	sensitive;		/* are key strings case sensitive */
	Hfs	*fs;
	Fork	*fork;
};

struct Node
{
	int	type;		/* type of this node */
	u32int	next;		/* next related node or 0 */
	int	nrec;		/* number of records in this node */
};

struct Treeref
{
	Tree	*tree;
	u32int	cnid;		/* tree->fork->cnid, for debugging prints */

	Block	*block;		/* a node in the tree */
	u32int	nno;
	Node	node;

	int	rno;		/* a record in the node */
	int	klen;
	uchar	*key;
	int	dlen;
	uchar	*data;
};

struct Key
{
	int	(*_cmp)(uchar *k, int len, int *order, Key *key);
	void	*priv;
};

struct Extentkey
{
	u32int	cnid;
	int	type;
	u32int	bno;
};

struct Name
{
	int	len;
	Rune	name[NAMELEN];		/* only len runes on disk */
};

struct Catalogkey
{
	u32int	parent;
	union{
		Name	name;
		uchar	*b;		/* not yet decoded */
	} u;
};

struct Hfs
{
	u32int	blocksize;
	u32int	nblock;
	u32int	nfree;		/* for debugging */
	int	hasbadblocks;
	Fork	alloc;		/* block allocation bitmap */
	Fork	extentsfork;
	Fork	catalogfork;
	Tree	extents;		/* Extentkey -> Extent[NEXTENT] */
	Tree	catalog;		/* Catalogkey -> Catalogkey + Inode */
	u32int	hlinkparent;		/* 0 or cnid */
	Disk	*disk;
	Fsys	*fsys;
};
