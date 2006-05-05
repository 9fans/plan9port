/* 
 * NFS mounter V3;  see RFC 1813
 */
/*
#pragma lib "libsunrpc.a"
#pragma src "/sys/src/libsunrpc"
*/
#define _NFS3H_ /* sorry */

enum {
	NfsMount1HandleSize = 32,
	NfsMount3MaxPathSize = 1024,
	NfsMount3MaxNameSize = 255,
	NfsMount3MaxHandleSize = 64,
	NfsMount3Program = 100005,
	NfsMount3Version = 3,
	NfsMount1Program = 100005,
	NfsMount1Version = 1
};
typedef struct NfsMount3TNull NfsMount3TNull;
typedef struct NfsMount3RNull NfsMount3RNull;
typedef struct NfsMount3TMnt NfsMount3TMnt;
typedef struct NfsMount3RMnt NfsMount3RMnt;
typedef struct NfsMount3TDump NfsMount3TDump;
typedef struct NfsMount3Entry NfsMount3Entry;
typedef struct NfsMount3RDump NfsMount3RDump;
typedef struct NfsMount3TUmnt NfsMount3TUmnt;
typedef struct NfsMount3RUmnt NfsMount3RUmnt;
typedef struct NfsMount3Export NfsMount3Export;
typedef struct NfsMount3TUmntall NfsMount3TUmntall;
typedef struct NfsMount3RUmntall NfsMount3RUmntall;
typedef struct NfsMount3TExport NfsMount3TExport;
typedef struct NfsMount3RExport NfsMount3RExport;

typedef enum
{
	NfsMount3CallTNull,
	NfsMount3CallRNull,
	NfsMount3CallTMnt,
	NfsMount3CallRMnt,
	NfsMount3CallTDump,
	NfsMount3CallRDump,
	NfsMount3CallTUmnt,
	NfsMount3CallRUmnt,
	NfsMount3CallTUmntall,
	NfsMount3CallRUmntall,
	NfsMount3CallTExport,
	NfsMount3CallRExport
} NfsMount3CallType;

struct NfsMount3TNull {
	SunCall call;
};

struct NfsMount3RNull {
	SunCall call;
};

struct NfsMount3TMnt {
	SunCall call;
	char *path;
};

struct NfsMount3RMnt {
	SunCall call;
	uint status;
	uchar *handle;
	uint len;
	u32int *auth;
	u32int nauth;
};

struct NfsMount3TDump {
	SunCall call;
};

struct NfsMount3Entry {
	char *host;
	char *path;
};

struct NfsMount3RDump {
	SunCall call;
	uchar *data;
	u32int count;
};

struct NfsMount3TUmnt {
	SunCall call;
	char *path;
};

struct NfsMount3RUmnt {
	SunCall call;
};

struct NfsMount3Export {
	char *path;
	char **g;
	u32int ng;
};

struct NfsMount3TUmntall {
	SunCall call;
};

struct NfsMount3RUmntall {
	SunCall call;
};

struct NfsMount3TExport {
	SunCall call;
};

struct NfsMount3RExport {
	SunCall call;
	uchar *data;
	u32int count;
};

uint nfsmount3exportgroupsize(uchar*);
uint nfsmount3exportsize(NfsMount3Export*);
int nfsmount3exportpack(uchar*, uchar*, uchar**, NfsMount3Export*);
int nfsmount3exportunpack(uchar*, uchar*, uchar**, char**, char***, NfsMount3Export*);
int nfsmount3entrypack(uchar*, uchar*, uchar**, NfsMount3Entry*);
int nfsmount3entryunpack(uchar*, uchar*, uchar**, NfsMount3Entry*);
uint nfsmount3entrysize(NfsMount3Entry*);

extern SunProg nfsmount3prog;

/*
 * NFS V3; see RFC 1813
 */
enum {
	Nfs3MaxHandleSize = 64,
	Nfs3CookieVerfSize = 8,
	Nfs3CreateVerfSize = 8,
	Nfs3WriteVerfSize = 8,
	Nfs3AccessRead = 1,
	Nfs3AccessLookup = 2,
	Nfs3AccessModify = 4,
	Nfs3AccessExtend = 8,
	Nfs3AccessDelete = 16,
	Nfs3AccessExecute = 32,
	Nfs3FsHasLinks = 1,
	Nfs3FsHasSymlinks = 2,
	Nfs3FsHomogeneous = 8,
	Nfs3FsCanSetTime = 16,

	Nfs3Version = 3,	
	Nfs3Program = 100003
};
typedef enum
{
	Nfs3Ok = 0,
	Nfs3ErrNotOwner = 1,
	Nfs3ErrNoEnt = 2,
	Nfs3ErrIo = 5,
	Nfs3ErrNxio = 6,
	Nfs3ErrNoMem = 12,
	Nfs3ErrAcces = 13,
	Nfs3ErrExist = 17,
	Nfs3ErrXDev = 18,
	Nfs3ErrNoDev = 19,
	Nfs3ErrNotDir = 20,
	Nfs3ErrIsDir = 21,
	Nfs3ErrInval = 22,
	Nfs3ErrFbig = 27,
	Nfs3ErrNoSpc = 28,
	Nfs3ErrRoFs = 30,
	Nfs3ErrMLink = 31,
	Nfs3ErrNameTooLong = 63,
	Nfs3ErrNotEmpty = 66,
	Nfs3ErrDQuot = 69,
	Nfs3ErrStale = 70,
	Nfs3ErrRemote = 71,
	Nfs3ErrBadHandle = 10001,
	Nfs3ErrNotSync = 10002,
	Nfs3ErrBadCookie = 10003,
	Nfs3ErrNotSupp = 10004,
	Nfs3ErrTooSmall = 10005,
	Nfs3ErrServerFault = 10006,
	Nfs3ErrBadType = 10007,
	Nfs3ErrJukebox = 10008,
	Nfs3ErrFprintNotFound = 10009,
	Nfs3ErrAborted = 10010
} Nfs3Status;

void nfs3errstr(Nfs3Status);

typedef enum
{
	Nfs3FileReg = 1,
	Nfs3FileDir = 2,
	Nfs3FileBlock = 3,
	Nfs3FileChar = 4,
	Nfs3FileSymlink = 5,
	Nfs3FileSocket = 6,
	Nfs3FileFifo = 7
} Nfs3FileType;

enum
{
	Nfs3ModeSetUid = 0x800,
	Nfs3ModeSetGid = 0x400,
	Nfs3ModeSticky = 0x200
};

typedef enum
{
	Nfs3CallTNull,
	Nfs3CallRNull,
	Nfs3CallTGetattr,
	Nfs3CallRGetattr,
	Nfs3CallTSetattr,
	Nfs3CallRSetattr,
	Nfs3CallTLookup,
	Nfs3CallRLookup,
	Nfs3CallTAccess,
	Nfs3CallRAccess,
	Nfs3CallTReadlink,
	Nfs3CallRReadlink,
	Nfs3CallTRead,
	Nfs3CallRRead,
	Nfs3CallTWrite,
	Nfs3CallRWrite,
	Nfs3CallTCreate,
	Nfs3CallRCreate,
	Nfs3CallTMkdir,
	Nfs3CallRMkdir,
	Nfs3CallTSymlink,
	Nfs3CallRSymlink,
	Nfs3CallTMknod,
	Nfs3CallRMknod,
	Nfs3CallTRemove,
	Nfs3CallRRemove,
	Nfs3CallTRmdir,
	Nfs3CallRRmdir,
	Nfs3CallTRename,
	Nfs3CallRRename,
	Nfs3CallTLink,
	Nfs3CallRLink,
	Nfs3CallTReadDir,
	Nfs3CallRReadDir,
	Nfs3CallTReadDirPlus,
	Nfs3CallRReadDirPlus,
	Nfs3CallTFsStat,
	Nfs3CallRFsStat,
	Nfs3CallTFsInfo,
	Nfs3CallRFsInfo,
	Nfs3CallTPathconf,
	Nfs3CallRPathconf,
	Nfs3CallTCommit,
	Nfs3CallRCommit
} Nfs3CallType;

typedef struct Nfs3Handle Nfs3Handle;
typedef struct Nfs3Time Nfs3Time;
typedef struct Nfs3Attr Nfs3Attr;
typedef struct Nfs3WccAttr Nfs3WccAttr;
typedef struct Nfs3Wcc Nfs3Wcc;
typedef enum
{
	Nfs3SetTimeDont = 0,
	Nfs3SetTimeServer = 1,
	Nfs3SetTimeClient = 2
} Nfs3SetTime;

typedef struct Nfs3SetAttr Nfs3SetAttr;
typedef struct Nfs3TNull Nfs3TNull;
typedef struct Nfs3RNull Nfs3RNull;
typedef struct Nfs3TGetattr Nfs3TGetattr;
typedef struct Nfs3RGetattr Nfs3RGetattr;
typedef struct Nfs3TSetattr Nfs3TSetattr;
typedef struct Nfs3RSetattr Nfs3RSetattr;
typedef struct Nfs3TLookup Nfs3TLookup;
typedef struct Nfs3RLookup Nfs3RLookup;
typedef struct Nfs3TAccess Nfs3TAccess;
typedef struct Nfs3RAccess Nfs3RAccess;
typedef struct Nfs3TReadlink Nfs3TReadlink;
typedef struct Nfs3RReadlink Nfs3RReadlink;
typedef struct Nfs3TRead Nfs3TRead;
typedef struct Nfs3RRead Nfs3RRead;
typedef enum
{
	Nfs3SyncNone = 0,
	Nfs3SyncData = 1,
	Nfs3SyncFile = 2
} Nfs3Sync;

typedef struct Nfs3TWrite Nfs3TWrite;
typedef struct Nfs3RWrite Nfs3RWrite;
typedef enum
{
	Nfs3CreateUnchecked = 0,
	Nfs3CreateGuarded = 1,
	Nfs3CreateExclusive = 2
} Nfs3Create;

typedef struct Nfs3TCreate Nfs3TCreate;
typedef struct Nfs3RCreate Nfs3RCreate;
typedef struct Nfs3TMkdir Nfs3TMkdir;
typedef struct Nfs3RMkdir Nfs3RMkdir;
typedef struct Nfs3TSymlink Nfs3TSymlink;
typedef struct Nfs3RSymlink Nfs3RSymlink;
typedef struct Nfs3TMknod Nfs3TMknod;
typedef struct Nfs3RMknod Nfs3RMknod;
typedef struct Nfs3TRemove Nfs3TRemove;
typedef struct Nfs3RRemove Nfs3RRemove;
typedef struct Nfs3TRmdir Nfs3TRmdir;
typedef struct Nfs3RRmdir Nfs3RRmdir;
typedef struct Nfs3TRename Nfs3TRename;
typedef struct Nfs3RRename Nfs3RRename;
typedef struct Nfs3TLink Nfs3TLink;
typedef struct Nfs3RLink Nfs3RLink;
typedef struct Nfs3TReadDir Nfs3TReadDir;
typedef struct Nfs3Entry Nfs3Entry;
typedef struct Nfs3RReadDir Nfs3RReadDir;
typedef struct Nfs3TReadDirPlus Nfs3TReadDirPlus;
typedef struct Nfs3EntryPlus Nfs3EntryPlus;
typedef struct Nfs3RReadDirPlus Nfs3RReadDirPlus;
typedef struct Nfs3TFsStat Nfs3TFsStat;
typedef struct Nfs3RFsStat Nfs3RFsStat;
typedef struct Nfs3TFsInfo Nfs3TFsInfo;
typedef struct Nfs3RFsInfo Nfs3RFsInfo;
typedef struct Nfs3TPathconf Nfs3TPathconf;
typedef struct Nfs3RPathconf Nfs3RPathconf;
typedef struct Nfs3TCommit Nfs3TCommit;
typedef struct Nfs3RCommit Nfs3RCommit;

struct Nfs3Handle {
	uchar h[Nfs3MaxHandleSize];
	u32int len;
};

struct Nfs3Time {
	u32int sec;
	u32int nsec;
};

struct Nfs3Attr {
	Nfs3FileType type;
	u32int mode;
	u32int nlink;
	u32int uid;
	u32int gid;
	u64int size;
	u64int used;
	u32int major;
	u32int minor;
	u64int fsid;
	u64int fileid;
	Nfs3Time atime;
	Nfs3Time mtime;
	Nfs3Time ctime;
};

struct Nfs3WccAttr {
	u64int size;
	Nfs3Time mtime;
	Nfs3Time ctime;
};

struct Nfs3Wcc {
	u1int haveWccAttr;
	Nfs3WccAttr wccAttr;
	u1int haveAttr;
	Nfs3Attr attr;
};

struct Nfs3SetAttr {
	u1int setMode;
	u32int mode;
	u1int setUid;
	u32int uid;
	u1int setGid;
	u32int gid;
	u1int setSize;
	u64int size;
	Nfs3SetTime setAtime;
	Nfs3Time atime;
	Nfs3SetTime setMtime;
	Nfs3Time mtime;
};

struct Nfs3TNull {
	SunCall call;
};

struct Nfs3RNull {
	SunCall call;
};

struct Nfs3TGetattr {
	SunCall call;
	Nfs3Handle handle;
};

struct Nfs3RGetattr {
	SunCall call;
	Nfs3Status status;
	Nfs3Attr attr;
};

struct Nfs3TSetattr {
	SunCall call;
	Nfs3Handle handle;
	Nfs3SetAttr attr;
	u1int checkCtime;
	Nfs3Time ctime;
};

struct Nfs3RSetattr {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc wcc;
};

struct Nfs3TLookup {
	SunCall call;
	Nfs3Handle handle;
	char *name;
};

struct Nfs3RLookup {
	SunCall call;
	Nfs3Status status;
	Nfs3Handle handle;
	u1int haveAttr;
	Nfs3Attr attr;
	u1int haveDirAttr;
	Nfs3Attr dirAttr;
};

struct Nfs3TAccess {
	SunCall call;
	Nfs3Handle handle;
	u32int access;
};

struct Nfs3RAccess {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	u32int access;
};

struct Nfs3TReadlink {
	SunCall call;
	Nfs3Handle handle;
};

struct Nfs3RReadlink {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	char *data;
};

struct Nfs3TRead {
	SunCall call;
	Nfs3Handle handle;
	u64int offset;
	u32int count;
};

struct Nfs3RRead {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	u32int count;
	u1int eof;
	uchar *data;
	u32int ndata;
};

struct Nfs3TWrite {
	SunCall call;
	Nfs3Handle handle;
	u64int offset;
	u32int count;
	Nfs3Sync stable;
	uchar *data;
	u32int ndata;
};

struct Nfs3RWrite {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc wcc;
	u32int count;
	Nfs3Sync committed;
	uchar verf[Nfs3WriteVerfSize];
};

struct Nfs3TCreate {
	SunCall call;
	Nfs3Handle handle;
	char *name;
	Nfs3Create mode;
	Nfs3SetAttr attr;
	uchar verf[Nfs3CreateVerfSize];
};

struct Nfs3RCreate {
	SunCall call;
	Nfs3Status status;
	u1int haveHandle;
	Nfs3Handle handle;
	u1int haveAttr;
	Nfs3Attr attr;
	Nfs3Wcc dirWcc;
};

struct Nfs3TMkdir {
	SunCall call;
	Nfs3Handle handle;
	char *name;
	Nfs3SetAttr attr;
};

struct Nfs3RMkdir {
	SunCall call;
	Nfs3Status status;
	u1int haveHandle;
	Nfs3Handle handle;
	u1int haveAttr;
	Nfs3Attr attr;
	Nfs3Wcc dirWcc;
};

struct Nfs3TSymlink {
	SunCall call;
	Nfs3Handle handle;
	char *name;
	Nfs3SetAttr attr;
	char *data;
};

struct Nfs3RSymlink {
	SunCall call;
	Nfs3Status status;
	u1int haveHandle;
	Nfs3Handle handle;
	u1int haveAttr;
	Nfs3Attr attr;
	Nfs3Wcc dirWcc;
};

struct Nfs3TMknod {
	SunCall call;
	Nfs3Handle handle;
	char *name;
	Nfs3FileType type;
	Nfs3SetAttr attr;
	u32int major;
	u32int minor;
};

struct Nfs3RMknod {
	SunCall call;
	Nfs3Status status;
	u1int haveHandle;
	Nfs3Handle handle;
	u1int haveAttr;
	Nfs3Attr attr;
	Nfs3Wcc dirWcc;
};

struct Nfs3TRemove {
	SunCall call;
	Nfs3Handle handle;
	char *name;
};

struct Nfs3RRemove {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc wcc;
};

struct Nfs3TRmdir {
	SunCall call;
	Nfs3Handle handle;
	char *name;
};

struct Nfs3RRmdir {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc wcc;
};

struct Nfs3TRename {
	SunCall call;
	struct {
		Nfs3Handle handle;
		char *name;
	} from;
	struct {
		Nfs3Handle handle;
		char *name;
	} to;
};

struct Nfs3RRename {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc fromWcc;
	Nfs3Wcc toWcc;
};

struct Nfs3TLink {
	SunCall call;
	Nfs3Handle handle;
	struct {
		Nfs3Handle handle;
		char *name;
	} link;
};

struct Nfs3RLink {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	Nfs3Wcc dirWcc;
};

struct Nfs3TReadDir {
	SunCall call;
	Nfs3Handle handle;
	u64int cookie;
	uchar verf[Nfs3CookieVerfSize];
	u32int count;
};

struct Nfs3RReadDir {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	uchar verf[Nfs3CookieVerfSize];
	uchar *data;
	u32int count;
	u1int eof;
};

struct Nfs3TReadDirPlus {
	SunCall call;
	Nfs3Handle handle;
	u64int cookie;
	uchar verf[Nfs3CookieVerfSize];
	u32int dirCount;
	u32int maxCount;
};

struct Nfs3Entry {
	u64int fileid;
	char *name;
	uint namelen;
	u64int cookie;
	u1int haveAttr;
	Nfs3Attr attr;
	u1int haveHandle;
	Nfs3Handle handle;
};

struct Nfs3RReadDirPlus {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	uchar verf[Nfs3CookieVerfSize];
	uchar *data;
	u32int count;
	u1int eof;
};

struct Nfs3TFsStat {
	SunCall call;
	Nfs3Handle handle;
};

struct Nfs3RFsStat {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	u64int totalBytes;
	u64int freeBytes;
	u64int availBytes;
	u64int totalFiles;
	u64int freeFiles;
	u64int availFiles;
	u32int invarSec;
};

struct Nfs3TFsInfo {
	SunCall call;
	Nfs3Handle handle;
};

struct Nfs3RFsInfo {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	u32int readMax;
	u32int readPref;
	u32int readMult;
	u32int writeMax;
	u32int writePref;
	u32int writeMult;
	u32int readDirPref;
	u64int maxFileSize;
	Nfs3Time timePrec;
	u32int flags;
};

struct Nfs3TPathconf {
	SunCall call;
	Nfs3Handle handle;
};

struct Nfs3RPathconf {
	SunCall call;
	Nfs3Status status;
	u1int haveAttr;
	Nfs3Attr attr;
	u32int maxLink;
	u32int maxName;
	u1int noTrunc;
	u1int chownRestricted;
	u1int caseInsensitive;
	u1int casePreserving;
};

struct Nfs3TCommit {
	SunCall call;
	Nfs3Handle handle;
	u64int offset;
	u32int count;
};

struct Nfs3RCommit {
	SunCall call;
	Nfs3Status status;
	Nfs3Wcc wcc;
	uchar verf[Nfs3WriteVerfSize];
};

char *nfs3statusstr(Nfs3Status);
char *nfs3typestr(SunCallType);
char *nfs3settimestr(Nfs3SetTime);
char *nfs3syncstr(Nfs3Sync);

void nfs3handleprint(Fmt*, Nfs3Handle*);
u32int nfs3handlesize(Nfs3Handle*);
int nfs3handlepack(uchar*, uchar*, uchar**, Nfs3Handle*);
int nfs3handleunpack(uchar*, uchar*, uchar**, Nfs3Handle*);

void nfs3timeprint(Fmt*, Nfs3Time*);
u32int nfs3timesize(Nfs3Time*);
int nfs3timepack(uchar*, uchar*, uchar**, Nfs3Time*);
int nfs3timeunpack(uchar*, uchar*, uchar**, Nfs3Time*);

void nfs3attrprint(Fmt*, Nfs3Attr*);
u32int nfs3attrsize(Nfs3Attr*);
int nfs3attrpack(uchar*, uchar*, uchar**, Nfs3Attr*);
int nfs3attrunpack(uchar*, uchar*, uchar**, Nfs3Attr*);

void nfs3wccattrprint(Fmt*, Nfs3WccAttr*);
u32int nfs3wccattrsize(Nfs3WccAttr*);
int nfs3wccattrpack(uchar*, uchar*, uchar**, Nfs3WccAttr*);
int nfs3wccattrunpack(uchar*, uchar*, uchar**, Nfs3WccAttr*);

void nfs3wccprint(Fmt*, Nfs3Wcc*);
u32int nfs3wccsize(Nfs3Wcc*);
int nfs3wccpack(uchar*, uchar*, uchar**, Nfs3Wcc*);
int nfs3wccunpack(uchar*, uchar*, uchar**, Nfs3Wcc*);

void nfs3setattrprint(Fmt*, Nfs3SetAttr*);
u32int nfs3setattrsize(Nfs3SetAttr*);
int nfs3setattrpack(uchar*, uchar*, uchar**, Nfs3SetAttr*);
int nfs3setattrunpack(uchar*, uchar*, uchar**, Nfs3SetAttr*);

extern SunProg nfs3prog;

void nfs3entryprint(Fmt*, Nfs3Entry*);
u32int nfs3entrysize(Nfs3Entry*);
int nfs3entrypack(uchar*, uchar*, uchar**, Nfs3Entry*);
int nfs3entryunpack(uchar*, uchar*, uchar**, Nfs3Entry*);

void nfs3entryplusprint(Fmt*, Nfs3Entry*);
u32int nfs3entryplussize(Nfs3Entry*);
int nfs3entrypluspack(uchar*, uchar*, uchar**, Nfs3Entry*);
int nfs3entryplusunpack(uchar*, uchar*, uchar**, Nfs3Entry*);

