#ifndef _VENTI_H_
#define _VENTI_H_ 1
#ifdef __cplusplus
extern "C" { 
#endif


AUTOLIB(venti)

/* XXX should be own library? */
/*
 * Packets
 */
enum
{
	MaxFragSize = 9*1024
};

typedef struct Packet Packet;

Packet*	packetalloc(void);
void	packetappend(Packet*, uchar *buf, int n);
uint	packetasize(Packet*);
int	packetcmp(Packet*, Packet*);
int	packetcompact(Packet*);
void	packetconcat(Packet*, Packet*);
int	packetconsume(Packet*, uchar *buf, int n);
int	packetcopy(Packet*, uchar *buf, int offset, int n);
Packet*	packetdup(Packet*, int offset, int n);
Packet*	packetforeign(uchar *buf, int n, void (*free)(void *a), void *a);
int	packetfragments(Packet*, IOchunk*, int nio, int offset);
void	packetfree(Packet*);
uchar*	packetheader(Packet*, int n);
uchar*	packetpeek(Packet*, uchar *buf, int offset, int n);
void	packetprefix(Packet*, uchar *buf, int n);
void	packetsha1(Packet*, uchar sha1[20]);
uint	packetsize(Packet*);
Packet*	packetsplit(Packet*, int n);
void	packetstats(void);
uchar*	packettrailer(Packet*, int n);
int	packettrim(Packet*, int offset, int n);

/* XXX should be own library? */
/*
 * Logging
 */
typedef struct VtLog VtLog;
typedef struct VtLogChunk VtLogChunk;

struct VtLog
{
	VtLog	*next;		/* in hash table */
	char	*name;
	VtLogChunk *chunk;
	uint	nchunk;
	VtLogChunk *w;
	QLock	lk;
	int	ref;
};

struct VtLogChunk
{
	char	*p;
	char	*ep;
	char	*wp;
};

VtLog*	vtlogopen(char *name, uint size);
void	vtlogprint(VtLog *log, char *fmt, ...);
void	vtlog(char *name, char *fmt, ...);
void	vtlogclose(VtLog*);
void	vtlogremove(char *name);
char**	vtlognames(int*);
void	vtlogdump(int fd, VtLog*);

/* XXX begin actual venti.h */

typedef struct VtFcall VtFcall;
typedef struct VtConn VtConn;
typedef struct VtEntry VtEntry;
typedef struct VtRoot VtRoot;

/*
 * Fundamental constants.
 */
enum
{
	VtScoreSize	= 20,
	VtMaxStringSize = 1024,
	VtPointerDepth	= 7
};
#define VtMaxFileSize ((1ULL<<48)-1)


/* 
 * Strings in packets.
 */
int vtputstring(Packet*, char*);
int vtgetstring(Packet*, char**);

/*
 * Block types.
 * 
 * The initial Venti protocol had a much
 * less regular list of block types.
 * VtToDiskType converts from new to old.
 */
enum
{
	VtDataType	= 0<<3,
	/* VtDataType+1, ... */
	VtDirType	= 1<<3,
	/* VtDirType+1, ... */
	VtRootType	= 2<<3,
	VtMaxType,
	VtCorruptType = 0xFF,

	VtTypeDepthMask = 7,
	VtTypeBaseMask = ~VtTypeDepthMask
};

/* convert to/from on-disk type numbers */
uint vttodisktype(uint);
uint vtfromdisktype(uint);

/*
 * VtEntry describes a Venti stream
 *
 * The _ enums are only used on the wire.
 * They are not present in the VtEntry structure
 * and should not be used by client programs.
 * (The info is in the type field.)
 */
enum
{
	VtEntryActive = 1<<0,		/* entry is in use */
	_VtEntryDir = 1<<1,		/* a directory */
	_VtEntryDepthShift = 2,		/* shift for pointer depth */
	_VtEntryDepthMask = 7<<2,	/* mask for pointer depth */
	VtEntryLocal = 1<<5,		/* for local storage only */
	_VtEntryBig = 1<<6,
	VtEntryNoArchive = 1<<7,	/* for local storage only */
};
enum
{
	VtEntrySize = 40
};
struct VtEntry
{
	ulong	gen;			/* generation number */
	ulong	psize;			/* pointer block size */
	ulong	dsize;			/* data block size */
	uchar	type;
	uchar	flags;
	uvlong	size;
	uchar	score[VtScoreSize];
};

void vtentrypack(VtEntry*, uchar*, int index);
int vtentryunpack(VtEntry*, uchar*, int index);

struct VtRoot
{
	char	name[128];
	char	type[128];
	uchar	score[VtScoreSize];	/* to a Dir block */
	ulong	blocksize;		/* maximum block size */
	uchar	prev[VtScoreSize];	/* last root block */
};

enum
{
	VtRootSize = 300,
	VtRootVersion = 2,
	_VtRootVersionBig = 1<<15,
};

void vtrootpack(VtRoot*, uchar*);
int vtrootunpack(VtRoot*, uchar*);

/*
 * score of zero length block
 */
extern uchar vtzeroscore[VtScoreSize];

/*
 * zero extend and truncate blocks
 */
void vtzeroextend(int type, uchar *buf, uint n, uint nn);
uint vtzerotruncate(int type, uchar *buf, uint n);

/*
 * parse score: mungs s
 */
int vtparsescore(char *s, char **prefix, uchar[VtScoreSize]);

/*
 * formatting
 * other than noted, these formats all ignore
 * the width and precision arguments, and all flags
 *
 * V	a venti score
 */

int vtscorefmt(Fmt*);

/*
 * error-checking malloc et al.
 */
void	vtfree(void *);
void*	vtmalloc(int);
void*	vtmallocz(int);
void*	vtrealloc(void *p, int);
void*	vtbrk(int n);
char*	vtstrdup(char *);

/*
 * Venti protocol
 */

/*
 * Crypto strengths
 */
enum
{
	VtCryptoStrengthNone,
	VtCryptoStrengthAuth,
	VtCryptoStrengthWeak,
	VtCryptoStrengthStrong
};

/*
 * Crypto suites
 */
enum
{
	VtCryptoNone,
	VtCryptoSSL3,
	VtCryptoTLS1,
	VtCryptoMax
};

/* 
 * Codecs
 */
enum
{
	VtCodecNone,
	VtCodecDeflate,
	VtCodecThwack,
	VtCodecMax
};

enum
{
	VtRerror	= 1,
	VtTping		= 2,
	VtRping,
	VtThello	= 4,
	VtRhello,
	VtTgoodbye	= 6,
	VtRgoodbye,	/* not used */
	VtTauth0	= 8,
	VtRauth0,
	VtTauth1	= 10,
	VtRauth1,
	VtTread		= 12,
	VtRread,
	VtTwrite	= 14,
	VtRwrite,
	VtTsync		= 16,
	VtRsync,

	VtTmax
};

struct VtFcall
{
	uchar	msgtype;
	uchar	tag;

	char	*error;		/* Rerror */

	char	*version;	/* Thello */
	char	*uid;		/* Thello */
	uchar	strength;	/* Thello */
	uchar	*crypto;	/* Thello */
	uint	ncrypto;	/* Thello */
	uchar	*codec;		/* Thello */
	uint	ncodec;		/* Thello */
	char	*sid;		/* Rhello */
	uchar	rcrypto;	/* Rhello */
	uchar	rcodec;		/* Rhello */
	uchar	*auth;		/* TauthX, RauthX */
	uint	nauth;		/* TauthX, RauthX */
	uchar	score[VtScoreSize];	/* Tread, Rwrite */
	uchar	blocktype;	/* Tread, Twrite */
	uint	count;		/* Tread */
	Packet	*data;		/* Rread, Twrite */
};

Packet*	vtfcallpack(VtFcall*);
int	vtfcallunpack(VtFcall*, Packet*);
void	vtfcallclear(VtFcall*);
int	vtfcallfmt(Fmt*);

enum
{
	VtStateAlloc,
	VtStateConnected,
	VtStateClosed
};

struct VtConn
{
	QLock	lk;
	QLock	inlk;
	QLock	outlk;
	int	debug;
	int	infd;
	int	outfd;
	int	muxer;
	void	*writeq;
	void	*readq;
	int	state;
	void	*wait[256];
	uint	ntag;
	uint	nsleep;
	Packet	*part;
	Rendez	tagrend;
	Rendez	rpcfork;
	char	*version;
	char	*uid;
	char	*sid;
	char	addr[256];	/* address of other side */
};

VtConn*	vtconn(int infd, int outfd);
int	vtreconn(VtConn*, int, int);
VtConn*	vtdial(char*);
int	vtredial(VtConn*, char *);
void	vtfreeconn(VtConn*);
int	vtsend(VtConn*, Packet*);
Packet*	vtrecv(VtConn*);
int	vtversion(VtConn* z);
void	vtdebug(VtConn* z, char*, ...);
void	vthangup(VtConn* z);
int	vtgoodbye(VtConn* z);

/* #pragma varargck argpos vtdebug 2 */

/* server */
typedef struct VtSrv VtSrv;
typedef struct VtReq VtReq;
struct VtReq
{
	VtFcall	tx;
	VtFcall	rx;
/* private */
	VtSrv	*srv;
	void	*sc;
};

int	vtsrvhello(VtConn*);
VtSrv*	vtlisten(char *addr);
VtReq*	vtgetreq(VtSrv*);
void	vtrespond(VtReq*);

/* client */
Packet*	vtrpc(VtConn*, Packet*);
Packet*	_vtrpc(VtConn*, Packet*, VtFcall*);
void	vtrecvproc(void*);	/* VtConn */
void	vtsendproc(void*);	/* VtConn */

int	vtconnect(VtConn*);
int	vthello(VtConn*);
int	vtread(VtConn*, uchar score[VtScoreSize], uint type, uchar *buf, int n);
int	vtwrite(VtConn*, uchar score[VtScoreSize], uint type, uchar *buf, int n);
Packet*	vtreadpacket(VtConn*, uchar score[VtScoreSize], uint type, int n);
int	vtwritepacket(VtConn*, uchar score[VtScoreSize], uint type, Packet *p);
int	vtsync(VtConn*);
int	vtping(VtConn*);

/* sha1 */
void	vtsha1(uchar score[VtScoreSize], uchar*, int);
int	vtsha1check(uchar score[VtScoreSize], uchar*, int);

/*
 * Data blocks and block cache.
 */
enum
{
	NilBlock = ~0
};

typedef struct VtBlock VtBlock;
typedef struct VtCache VtCache;

struct VtBlock
{
	VtCache	*c;
	QLock	lk;

	uchar	*data;
	uchar	score[VtScoreSize];
	uchar	type;	/* VtXXX */
	ulong	size;

	/* internal to cache */
	int	nlock;
	int	iostate;
	int	ref;
	u32int	heap;
	VtBlock	*next;
	VtBlock	**prev;
	u32int	used;
	u32int	used2;
	u32int	addr;
	uintptr	pc;
};

u32int	vtglobaltolocal(uchar[VtScoreSize]);
void	vtlocaltoglobal(u32int, uchar[VtScoreSize]);

VtCache*vtcachealloc(VtConn*, ulong maxmem);
void	vtcachefree(VtCache*);
VtBlock*vtcachelocal(VtCache*, u32int addr, int type);
VtBlock*vtcacheglobal(VtCache*, uchar[VtScoreSize], int type, ulong size);
VtBlock*vtcacheallocblock(VtCache*, int type, ulong size);
void	vtcachesetwrite(VtCache*,
	int(*)(VtConn*, uchar[VtScoreSize], uint, uchar*, int));
void	vtblockput(VtBlock*);
int	vtblockwrite(VtBlock*);
VtBlock*vtblockcopy(VtBlock*);
void	vtblockduplock(VtBlock*);

extern int vtcachencopy, vtcachenread, vtcachenwrite;
extern int vttracelevel;

/*
 * Hash tree file tree.
 */
typedef struct VtFile VtFile;
struct VtFile
{
	QLock	lk;
	int	ref;
	int	local;
	VtBlock	*b;			/* block containing this file */
	uchar	score[VtScoreSize];	/* score of block containing this file */
	int	bsize;			/* size of block */

/* immutable */
	VtCache	*c;
	int	mode;
	u32int	gen;
	int	dsize;
	int	psize;
	int	dir;
	VtFile	*parent;
	int	epb;			/* entries per block in parent */
	u32int	offset; 		/* entry offset in parent */
};

enum
{
	VtOREAD,
	VtOWRITE,
	VtORDWR
};

VtBlock*vtfileblock(VtFile*, u32int, int mode);
int	vtfileblockscore(VtFile*, u32int, uchar[VtScoreSize]);
void	vtfileclose(VtFile*);
VtFile*	_vtfilecreate(VtFile*, int offset, int psize, int dsize, int dir);
VtFile*	vtfilecreate(VtFile*, int psize, int dsize, int dir);
VtFile*	vtfilecreateroot(VtCache*, int psize, int dsize, int type);
int	vtfileflush(VtFile*);
int	vtfileflushbefore(VtFile*, u64int);
u32int	vtfilegetdirsize(VtFile*);
int	vtfilegetentry(VtFile*, VtEntry*);
uvlong	vtfilegetsize(VtFile*);
void	vtfileincref(VtFile*);
int	vtfilelock2(VtFile*, VtFile*, int);
int	vtfilelock(VtFile*, int);
VtFile*	vtfileopen(VtFile*, u32int, int);
VtFile*	vtfileopenroot(VtCache*, VtEntry*);
long	vtfileread(VtFile*, void*, long, vlong);
int	vtfileremove(VtFile*);
int	vtfilesetdirsize(VtFile*, u32int);
int	vtfilesetentry(VtFile*, VtEntry*);
int	vtfilesetsize(VtFile*, u64int);
int	vtfiletruncate(VtFile*);
void	vtfileunlock(VtFile*);
long	vtfilewrite(VtFile*, void*, long, vlong);

int	vttimefmt(Fmt*);

extern int chattyventi;
extern int ventidoublechecksha1;
extern int ventilogging;

extern char *VtServerLog;

#ifdef __cplusplus
}
#endif
#endif
