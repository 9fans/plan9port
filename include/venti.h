#ifndef _VENTI_H_
#define _VENTI_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif
/* XXX should be own library? */
/*
 * Packets
 */
enum
{
	MaxFragSize = 9*1024,
};

typedef struct Packet Packet;
Packet *packetalloc(void);
void packetfree(Packet*);
Packet *packetforeign(uchar *buf, int n, void (*free)(void *a), void *a);
Packet *packetdup(Packet*, int offset, int n);
Packet *packetsplit(Packet*, int n);
int packetconsume(Packet*, uchar *buf, int n);
int packettrim(Packet*, int offset, int n);
uchar *packetheader(Packet*, int n);
uchar *packettrailer(Packet*, int n);
int packetprefix(Packet*, uchar *buf, int n);
int packetappend(Packet*, uchar *buf, int n);
int packetconcat(Packet*, Packet*);
uchar *packetpeek(Packet*, uchar *buf, int offset, int n);
int packetcopy(Packet*, uchar *buf, int offset, int n);
int packetfragments(Packet*, IOchunk*, int nio, int offset);
uint packetsize(Packet*);
uint packetasize(Packet*);
int packetcompact(Packet*);
int packetcmp(Packet*, Packet*);
void packetstats(void);
void packetsha1(Packet*, uchar sha1[20]);

/* XXX begin actual venti.h */

/*
#pragma lib "libnventi.a"
#pragma src "/sys/src/libnventi"
*/

typedef struct VtFcall VtFcall;
typedef struct VtSha1 VtSha1;
typedef struct VtConn VtConn;
typedef struct VtEntry VtEntry;
typedef struct VtRoot VtRoot;

/*
 * Fundamental constants.
 */
enum
{
	VtScoreSize = 20,
	VtMaxStringSize = 1024,
	VtMaxLumpSize	= 56*1024,
	VtPointerDepth	= 7,
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
	VtCorruptType,
	VtMaxType,

	VtTypeDepthMask = 7,
};

/* convert to/from on-disk type numbers */
uint vttodisktype(uint);
uint vtfromdisktype(uint);

/*
 * VtEntry describes a Venti stream
 */
enum
{
	VtEntryActive = 1<<0,		/* entry is in use */
	VtEntryDir = 1<<1,		/* a directory */
	VtEntryDepthShift = 2,		/* shift for pointer depth */
	VtEntryDepthMask = 7<<2,	/* mask for pointer depth */
	VtEntryLocal = 1<<5,		/* for local storage only */
};
enum
{
	VtEntrySize = 40,
};
struct VtEntry
{
	ulong gen;			/* generation number */
	ushort psize;			/* pointer block size */
	ushort dsize;			/* data block size */
	uchar type;
	uchar flags;
	uvlong size;
	uchar score[VtScoreSize];
};

void vtentrypack(VtEntry*, uchar*, int index);
int vtentryunpack(VtEntry*, uchar*, int index);

struct VtRoot
{
	char name[128];
	char type[128];
	uchar score[VtScoreSize];	/* to a Dir block */
	ushort blocksize;		/* maximum block size */
	uchar prev[VtScoreSize];	/* last root block */
};

enum
{
	VtRootSize = 300,
	VtRootVersion = 2,
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
int vtparsescore(char *s, uint len, char **prefix, uchar[VtScoreSize]);

/*
 * formatting
 * other than noted, these formats all ignore
 * the width and precision arguments, and all flags
 *
 * V	a venti score
 */
/* #pragma	varargck	type	"V"		uchar* */

int vtscorefmt(Fmt*);

/*
 * error-checking malloc et al.
 */
void vtfree(void *);
void *vtmalloc(int);
void *vtmallocz(int);
void *vtrealloc(void *p, int);
void *vtbrk(int n);
char *vtstrdup(char *);

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
	VtCryptoStrengthStrong,
};

/*
 * Crypto suites
 */
enum
{
	VtCryptoNone,
	VtCryptoSSL3,
	VtCryptoTLS1,
	VtCryptoMax,
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
	uchar	type;
	uchar	tag;

	char	*error;	/* Rerror */

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
	uchar	dtype;		/* Tread, Twrite */
	ushort	count;		/* Tread */
	Packet	*data;		/* Rread, Twrite */
};

Packet *vtfcallpack(VtFcall*);
int vtfcallunpack(VtFcall*, Packet*);
void vtfcallclear(VtFcall*);
int vtfcallfmt(Fmt*);

enum
{
	VtStateAlloc,
	VtStateConnected,
	VtStateClosed,
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
	void *wait[256];
	uint	ntag;
	uint	nsleep;
	Packet	*part;
	Rendez	tagrend;
	Rendez	rpcfork;
	char	*version;
	char	*uid;
	char *sid;
};

VtConn *vtconn(int infd, int outfd);
VtConn *vtdial(char*);
void vtfreeconn(VtConn*);
int vtsend(VtConn*, Packet*);
Packet *vtrecv(VtConn*);
int vtversion(VtConn *z);
void vtdebug(VtConn *z, char*, ...);
void vthangup(VtConn *z);
/* #pragma varargck argpos vtdebug 2 */

/* server */
typedef struct VtSrv VtSrv;
typedef struct VtReq VtReq;
struct VtReq
{
	VtFcall tx;
	VtFcall rx;
/* private */
	VtSrv *srv;
	void *sc;
};

int vtsrvhello(VtConn*);
VtSrv *vtlisten(char *addr);
VtReq *vtgetreq(VtSrv*);
void vtrespond(VtReq*);

/* client */
Packet *vtrpc(VtConn*, Packet*);
void vtrecvproc(void*);	/* VtConn* */
void vtsendproc(void*);	/* VtConn* */

int vtconnect(VtConn*);
int vthello(VtConn*);
int vtread(VtConn*, uchar score[VtScoreSize], uint type, uchar *buf, int n);
int vtwrite(VtConn*, uchar score[VtScoreSize], uint type, uchar *buf, int n);
Packet *vtreadpacket(VtConn*, uchar score[VtScoreSize], uint type, int n);
int vtwritepacket(VtConn*, uchar score[VtScoreSize], uint type, Packet *p);
int vtsync(VtConn*);
int vtping(VtConn*);

/*
 * Data blocks and block cache.
 */
enum
{
	NilBlock = ~0,
};

typedef struct VtBlock VtBlock;
typedef struct VtCache VtCache;

struct VtBlock
{
	VtCache	*c;
	QLock	lk;

	uchar	*data;
	uchar	score[VtScoreSize];
	uchar	type;	/* BtXXX */

	/* internal to cache */
	int		nlock;
	int		iostate;
	int		ref;
	u32int	heap;
	VtBlock	*next;
	VtBlock	**prev;
	u32int	used;
	u32int	used2;
	u32int	addr;

	/* internal to efile (HACK) */
	int		decrypted;
};

u32int vtglobaltolocal(uchar[VtScoreSize]);
void vtlocaltoglobal(u32int, uchar[VtScoreSize]);

VtCache *vtcachealloc(VtConn*, int blocksize, ulong nblocks, int mode);
void vtcachefree(VtCache*);
VtBlock *vtcachelocal(VtCache*, u32int addr, int type);
VtBlock *vtcacheglobal(VtCache*, uchar[VtScoreSize], int type);
VtBlock *vtcacheallocblock(VtCache*, int type);
void vtblockput(VtBlock*);
u32int vtcacheblocksize(VtCache*);
int vtblockwrite(VtBlock*);
VtBlock *vtblockcopy(VtBlock*);
void vtblockduplock(VtBlock*);

/*
 * Hash tree file tree.
 */
typedef struct VtFile VtFile;

enum
{
	VtOREAD,
	VtOWRITE,
	VtORDWR,
	VtOCREATE = 0x100,
};

VtFile *vtfileopenroot(VtCache*, VtEntry*);
VtFile *vtfilecreateroot(VtCache*, int psize, int dsize, int type);
VtFile *vtfileopen(VtFile*, u32int, int);
VtFile *vtfilecreate(VtFile*, int psize, int dsize, int dir);
VtBlock *vtfileblock(VtFile*, u32int, int mode);
int vtfileblockhash(VtFile*, u32int, uchar[VtScoreSize]);
long vtfileread(VtFile*, void*, long, vlong);
long vtfilewrite(VtFile*, void*, long, vlong);
int vtfileflush(VtFile*);
void vtfileincref(VtFile*);
void vtfileclose(VtFile*);
int vtfilegetentry(VtFile*, VtEntry*);
int vtfileblockscore(VtFile*, u32int, uchar[VtScoreSize]);
u32int vtfilegetdirsize(VtFile*);
int vtfilesetdirsize(VtFile*, u32int);
void	vtfileunlock(VtFile*);
int vtfilelock(VtFile*, int);
int vtfilelock2(VtFile*, VtFile*, int);
int vtfileflushbefore(VtFile*, u64int);

#if defined(__cplusplus)
}
#endif
#endif
