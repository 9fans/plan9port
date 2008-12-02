#define	VOLDESC	16	/* sector number */

/*
 * L means little-endian, M means big-endian, and LM means little-endian
 * then again big-endian.
 */
typedef uchar		Byte2L[2];
typedef uchar		Byte2M[2];
typedef uchar		Byte4LM[4];
typedef uchar		Byte4L[4];
typedef uchar		Byte4M[4];
typedef uchar		Byte8LM[8];
typedef union Drec	Drec;
typedef union Voldesc	Voldesc;

enum
{
	Boot		= 0,
	Primary		= 1,
	Supplementary	= 2,
	Partition	= 3,
	Terminator	= 255
};

union	Voldesc
{			/* volume descriptor */
	uchar	byte[Sectorsize];
	union {			/* for CD001, the ECMA standard */
		struct
		{
			uchar	type;
			uchar	stdid[5];
			uchar	version;
			uchar	unused;
			uchar	sysid[32];
			uchar	bootid[32];
			uchar	data[1977];
		} boot;
		struct
		{
			uchar	type;
			uchar	stdid[5];
			uchar	version;
			uchar	flags;
			uchar	sysid[32];
			uchar	volid[32];
			Byte8LM	partloc;
			Byte8LM	size;
			uchar	escapes[32];
			Byte4LM	vsetsize;
			Byte4LM	vseqno;
			Byte4LM	blksize;
			Byte8LM	ptabsize;
			Byte4L	lptable;
			Byte4L	optlptable;
			Byte4M	mptable;
			Byte4M	optmptable;
			uchar	rootdir[34];
			uchar	volsetid[128];
			uchar	pubid[128];
			uchar	prepid[128];
			uchar	appid[128];
			uchar	copyright[37];
			uchar	abstract[37];
			uchar	bibliography[37];
			uchar	cdate[17];
			uchar	mdate[17];
			uchar	expdate[17];
			uchar	effdate[17];
			uchar	fsversion;
			uchar	unused3[1];
			uchar	appuse[512];
			uchar	unused4[653];
		} desc;
	} z;
	union
	{			/* for CDROM, the `High Sierra' standard */
		struct
		{
			Byte8LM	number;
			uchar	type;
			uchar	stdid[5];
			uchar	version;
			uchar	flags;
			uchar	sysid[32];
			uchar	volid[32];
			Byte8LM	partloc;
			Byte8LM	size;
			uchar	escapes[32];
			Byte4LM	vsetsize;
			Byte4LM	vseqno;
			Byte4LM	blksize;
			uchar	quux[40];
			uchar	rootdir[34];
			uchar	volsetid[128];
			uchar	pubid[128];
			uchar	prepid[128];
			uchar	appid[128];
			uchar	copyright[32];
			uchar	abstract[32];
			uchar	cdate[16];
			uchar	mdate[16];
			uchar	expdate[16];
			uchar	effdate[16];
			uchar	fsversion;
		} desc;
	} r;
};

union	Drec
{
	struct
	{
		uchar	reclen;
		uchar	attrlen;
		Byte8LM	addr;
		Byte8LM	size;
		uchar	date[6];
		uchar	tzone;		/* flags in high sierra */
		uchar	flags;		/* ? in high sierra */
		uchar	unitsize;	/* ? in high sierra */
		uchar	gapsize;	/* ? in high sierra */
		Byte4LM	vseqno;		/* ? in high sierra */
		uchar	namelen;
		uchar	name[1];
	} z;
	struct
	{
		uchar	pad[24];
		uchar	flags;
	} r;
};

struct	Isofile
{
	short	fmt;		/* 'z' if iso, 'r' if high sierra */
	short	blksize;
	long	offset;		/* true offset when reading directory */
	long odelta;	/* true size of directory just read */
	long	doffset;	/* plan9 offset when reading directory */
	Drec	d;
};
