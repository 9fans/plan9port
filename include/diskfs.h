/* Copyright (c) Russ Cox, MIT; see src/libdiskfs/COPYRIGHT */

AUTOLIB(diskfs)

typedef struct Block Block;
typedef struct Disk Disk;
typedef struct Fsys Fsys;

#ifndef _NFS3H_	/* in case sunrpc.h, nfs3.h are not included */
typedef struct SunAuthUnix SunAuthUnix;
typedef struct Nfs3Attr Nfs3Attr;
typedef struct Nfs3Entry Nfs3Entry;
typedef struct Nfs3Handle Nfs3Handle;
typedef int Nfs3Status;
#endif
struct VtCache;

struct Disk
{
	Block *(*_read)(Disk *disk, u32int count, u64int offset);
	int (*_sync)(Disk*);
	void (*_close)(Disk*);
	void *priv;
};

struct Block
{
	Disk *disk;
	u32int len;
	uchar *data;
	void (*_close)(Block*);
	void *priv;
};

struct Fsys
{
	u32int blocksize;
	u32int nblock;
	char *type;

	Disk *disk;
	Block *(*_readblock)(Fsys *fsys, u64int blockno);
	int (*_sync)(Fsys *fsys);
	void (*_close)(Fsys *fsys);

	Nfs3Status (*_root)(Fsys*, Nfs3Handle*);
	Nfs3Status (*_access)(Fsys*, SunAuthUnix*, Nfs3Handle*, u32int, u32int*, Nfs3Attr*);
	Nfs3Status (*_lookup)(Fsys*, SunAuthUnix*, Nfs3Handle*, char*, Nfs3Handle*);
	Nfs3Status (*_getattr)(Fsys*, SunAuthUnix*, Nfs3Handle*, Nfs3Attr*);
	Nfs3Status (*_readdir)(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, u32int count, u64int cookie, uchar**, u32int*, uchar*);
	Nfs3Status (*_readfile)(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, u32int count, u64int offset, uchar**, u32int*, uchar*);
	Nfs3Status (*_readlink)(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, char **link);

	void *priv;
	
	u64int (*fileblock)(Fsys *fsys, Nfs3Handle *h, u64int offset);
};

struct Handle
{
	uchar h[64];
	int len;
};

void		blockdump(Block *b, char *desc);
void		blockput(Block *b);

Disk*	diskcache(Disk*, uint, uint);
Disk*	diskopenventi(struct VtCache*, uchar*);
Disk*	diskopenfile(char *file);
Disk*	diskpart(Disk*, u64int offset, u64int count);
void		diskpartabandon(Disk*);

Disk*	diskopen(char *file);
void		diskclose(Disk *disk);
Block*	diskread(Disk *disk, u32int, u64int offset);
int		disksync(Disk *disk);

Fsys*	fsysopenffs(Disk*);
Fsys*	fsysopenhfs(Disk*);
Fsys*	fsysopenkfs(Disk*);
Fsys*	fsysopenext2(Disk*);
Fsys*	fsysopenfat(Disk*);

Fsys*	fsysopen(Disk *disk);
Block*	fsysreadblock(Fsys *fsys, u64int blockno);
int		fsyssync(Fsys *fsys);
void		fsysclose(Fsys *fsys);

Nfs3Status		fsysroot(Fsys *fsys, Nfs3Handle *h);
Nfs3Status		fsyslookup(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, char *name, Nfs3Handle *nh);
Nfs3Status		fsysgetattr(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, Nfs3Attr *attr);
Nfs3Status		fsysreaddir(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, u32int count, u64int cookie, uchar **e, u32int *ne, uchar*);
Nfs3Status		fsysreadfile(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, u32int, u64int, uchar**, u32int*, uchar*);
Nfs3Status		fsysreadlink(Fsys *fsys, SunAuthUnix*, Nfs3Handle *h, char **plink);
Nfs3Status		fsysaccess(Fsys *fsys, SunAuthUnix*, Nfs3Handle*, u32int, u32int*, Nfs3Attr*);
void*	emalloc(ulong size);	/* provided by caller */

extern int allowall;
