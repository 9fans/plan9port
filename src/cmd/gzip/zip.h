typedef struct ZipHead	ZipHead;

enum
{
	/*
	 * magic numbers
	 */
	ZHeader		= 0x04034b50,
	ZCHeader	= 0x02014b50,
	ZECHeader	= 0x06054b50,

	/*
	 * "general purpose flag" bits
	 */
	ZEncrypted	= 1 << 0,
	ZTrailInfo	= 1 << 3,	/* uncsize, csize, and crc are in trailer */
	ZCompPatch	= 1 << 5,	/* compression patched data */

	ZCrcPoly	= 0xedb88320,

	/*
	 * compression method
	 */
	ZDeflate	= 8,

	/*
	 * internal file attributes
	 */
	ZIsText		= 1 << 0,

	/*
	 * file attribute interpretation, from high byte of version
	 */
	ZDos		= 0,
	ZAmiga		= 1,
	ZVMS		= 2,
	ZUnix		= 3,
	ZVMCMS		= 4,
	ZAtariST	= 5,
	ZOS2HPFS	= 6,
	ZMac		= 7,
	ZZsys		= 8,
	ZCPM		= 9,
	ZNtfs		= 10,

	/*
	 * external attribute flags for ZDos
	 */
	ZDROnly		= 0x01,
	ZDHidden	= 0x02,
	ZDSystem	= 0x04,
	ZDVLable	= 0x08,
	ZDDir		= 0x10,
	ZDArch		= 0x20,

	ZHeadSize	= 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2,
	ZHeadCrc	= 4 + 2 + 2 + 2 + 2 + 2,
	ZTrailSize	= 4 + 4 + 4,
	ZCHeadSize	= 4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 2 + 2 + 2 + 4 + 4,
	ZECHeadSize	= 4 + 2 + 2 + 2 + 2 + 4 + 4 + 2,
};

/*
 * interesting info from a zip header
 */
struct ZipHead
{
	int	madeos;			/* version made by */
	int	madevers;
	int	extos;			/* version needed to extract */
	int	extvers;
	int	flags;			/* general purpose bit flag */
	int	meth;
	int	modtime;
	int	moddate;
	ulong	crc;
	ulong	csize;
	ulong	uncsize;
	int	iattr;
	ulong	eattr;
	ulong	off;
	char	*file;
};
