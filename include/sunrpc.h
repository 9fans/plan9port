/*
 * Sun RPC; see RFC 1057
 */

/*
#pragma lib "libsunrpc.a"
#pragma src "/sys/src/libsunrpc"
*/
AUTOLIB(sunrpc)

typedef uchar u1int;

typedef struct SunAuthInfo SunAuthInfo;
typedef struct SunAuthUnix SunAuthUnix;
typedef struct SunRpc SunRpc;
typedef struct SunCall SunCall;

enum
{
	/* Authinfo.flavor */
	SunAuthNone = 0,
	SunAuthSys,
	SunAuthShort,
	SunAuthDes
};

typedef enum {
	SunAcceptError = 0x10000,
	SunRejectError = 0x20000,
	SunAuthError = 0x40000,

	/* Reply.status */
	SunSuccess = 0,

	SunProgUnavail = SunAcceptError | 1,
	SunProgMismatch,
	SunProcUnavail,
	SunGarbageArgs,
	SunSystemErr,

	SunRpcMismatch = SunRejectError | 0,

	SunAuthBadCred = SunAuthError | 1,
	SunAuthRejectedCred,
	SunAuthBadVerf,
	SunAuthRejectedVerf,
	SunAuthTooWeak,
	SunAuthInvalidResp,
	SunAuthFailed
} SunStatus;

struct SunAuthInfo
{
	uint flavor;
	uchar *data;
	uint ndata;
};

struct SunAuthUnix
{
	u32int stamp;
	char *sysname;
	u32int uid;
	u32int gid;
	u32int g[16];
	u32int ng;
};

struct SunRpc
{
	u32int xid;
	uint iscall;

	/*
	 * only sent on wire in call
	 * caller fills in for the reply unpackers.
	 */
	u32int proc;

	/* call */
	/* uint proc; */
	u32int prog, vers;
	SunAuthInfo cred;
	SunAuthInfo verf;
	uchar *data;
	uint ndata;

	/* reply */
	u32int status;
	/* SunAuthInfo verf; */
	u32int low, high;
	/* uchar *data; */
	/* uint ndata; */
};

typedef enum
{
	SunCallTypeTNull,
	SunCallTypeRNull
} SunCallType;

struct SunCall
{
	SunRpc rpc;
	SunCallType type;
};

void sunerrstr(SunStatus);

void sunrpcprint(Fmt*, SunRpc*);
uint sunrpcsize(SunRpc*);
SunStatus sunrpcpack(uchar*, uchar*, uchar**, SunRpc*);
SunStatus sunrpcunpack(uchar*, uchar*, uchar**, SunRpc*);

void sunauthinfoprint(Fmt*, SunAuthInfo*);
uint sunauthinfosize(SunAuthInfo*);
int sunauthinfopack(uchar*, uchar*, uchar**, SunAuthInfo*);
int sunauthinfounpack(uchar*, uchar*, uchar**, SunAuthInfo*);

void sunauthunixprint(Fmt*, SunAuthUnix*);
uint sunauthunixsize(SunAuthUnix*);
int sunauthunixpack(uchar*, uchar*, uchar**, SunAuthUnix*);
int sunauthunixunpack(uchar*, uchar*, uchar**, SunAuthUnix*);

int sunenumpack(uchar*, uchar*, uchar**, int*);
int sunenumunpack(uchar*, uchar*, uchar**, int*);
int sunuint1pack(uchar*, uchar*, uchar**, u1int*);
int sunuint1unpack(uchar*, uchar*, uchar**, u1int*);

int sunstringpack(uchar*, uchar*, uchar**, char**, u32int);
int sunstringunpack(uchar*, uchar*, uchar**, char**, u32int);
uint sunstringsize(char*);

int sunuint32pack(uchar*, uchar*, uchar**, u32int*);
int sunuint32unpack(uchar*, uchar*, uchar**, u32int*);
int sunuint64pack(uchar*, uchar*, uchar**, u64int*);
int sunuint64unpack(uchar*, uchar*, uchar**, u64int*);

int sunvaropaquepack(uchar*, uchar*, uchar**, uchar**, u32int*, u32int);
int sunvaropaqueunpack(uchar*, uchar*, uchar**, uchar**, u32int*, u32int);
uint sunvaropaquesize(u32int);

int sunfixedopaquepack(uchar*, uchar*, uchar**, uchar*, u32int);
int sunfixedopaqueunpack(uchar*, uchar*, uchar**, uchar*, u32int);
uint sunfixedopaquesize(u32int);

/*
 * Sun RPC Program
 */
typedef struct SunProc SunProc;
typedef struct SunProg SunProg;
struct SunProg
{
	uint prog;
	uint vers;
	SunProc *proc;
	int nproc;
};

struct SunProc
{
	int (*pack)(uchar*, uchar*, uchar**, SunCall*);
	int (*unpack)(uchar*, uchar*, uchar**, SunCall*);
	uint (*size)(SunCall*);
	void (*fmt)(Fmt*, SunCall*);
	uint sizeoftype;
};

SunStatus suncallpack(SunProg*, uchar*, uchar*, uchar**, SunCall*);
SunStatus suncallunpack(SunProg*, uchar*, uchar*, uchar**, SunCall*);
SunStatus suncallunpackalloc(SunProg*, SunCallType, uchar*, uchar*, uchar**, SunCall**);
void suncallsetup(SunCall*, SunProg*, uint);
uint suncallsize(SunProg*, SunCall*);

/*
 * Formatting
#pragma varargck type "B" SunRpc*
#pragma varargck type "C" SunCall*
 */

int	sunrpcfmt(Fmt*);
int	suncallfmt(Fmt*);
void	sunfmtinstall(SunProg*);


/*
 * Sun RPC Server
 */
typedef struct SunMsg SunMsg;
typedef struct SunSrv SunSrv;

enum
{
	SunStackSize = 32768
};

struct SunMsg
{
	uchar *data;
	int count;
	SunSrv *srv;
	SunRpc rpc;
	SunProg *pg;
	SunCall *call;
	Channel *creply;	/* chan(SunMsg*) */
};

struct SunSrv
{
	int chatty;
	int cachereplies;
	int alwaysreject;
	int localonly;
	int localparanoia;
	SunProg **map;
	Channel *crequest;
	int (*ipokay)(uchar*, ushort);

/* implementation use only */
	Channel **cdispatch;
	SunProg **prog;
	int nprog;
	void *cache;
	Channel *creply;
	Channel *cthread;
};

SunSrv *sunsrv(void);

void	sunsrvprog(SunSrv *srv, SunProg *prog, Channel *c);
int	sunsrvannounce(SunSrv *srv, char *address);
int	sunsrvudp(SunSrv *srv, char *address);
int	sunsrvnet(SunSrv *srv, char *address);
int	sunsrvfd(SunSrv *srv, int fd);
void	sunsrvthreadcreate(SunSrv *srv, void (*fn)(void*), void*);
void	sunsrvclose(SunSrv*);

int	sunmsgreply(SunMsg*, SunCall*);
int	sunmsgdrop(SunMsg*);
int	sunmsgreplyerror(SunMsg*, SunStatus);

/*
 * Sun RPC Client
 */
typedef struct SunClient SunClient;

struct SunClient
{
	int		fd;
	int		chatty;
	int		needcount;
	ulong	maxwait;
	ulong	xidgen;
	int		nsend;
	int		nresend;
	struct {
		ulong min;
		ulong max;
		ulong avg;
	} rtt;
	Channel	*dying;
	Channel	*rpcchan;
	Channel	*timerchan;
	Channel	*flushchan;
	Channel	*readchan;
	SunProg	**prog;
	int		nprog;
	int 		timertid;
	int 		nettid;
};

SunClient	*sundial(char*);

int	sunclientrpc(SunClient*, ulong, SunCall*, SunCall*, uchar**);
void	sunclientclose(SunClient*);
void	sunclientflushrpc(SunClient*, ulong);
void	sunclientprog(SunClient*, SunProg*);


/*
 * Provided by callers.
 * Should remove dependence on this, but hard.
 */
void	*emalloc(ulong);
void *erealloc(void*, ulong);


/*
 * Sun RPC port mapper; see RFC 1057 Appendix A
 */

typedef struct PortMap PortMap;
typedef struct PortTNull PortTNull;
typedef struct PortRNull PortRNull;
typedef struct PortTSet PortTSet;
typedef struct PortRSet PortRSet;
typedef struct PortTUnset PortTUnset;
typedef struct PortRUnset PortRUnset;
typedef struct PortTGetport PortTGetport;
typedef struct PortRGetport PortRGetport;
typedef struct PortTDump PortTDump;
typedef struct PortRDump PortRDump;
typedef struct PortTCallit PortTCallit;
typedef struct PortRCallit PortRCallit;

typedef enum
{
	PortCallTNull,
	PortCallRNull,
	PortCallTSet,
	PortCallRSet,
	PortCallTUnset,
	PortCallRUnset,
	PortCallTGetport,
	PortCallRGetport,
	PortCallTDump,
	PortCallRDump,
	PortCallTCallit,
	PortCallRCallit
} PortCallType;

enum
{
	PortProgram	= 100000,
	PortVersion	= 2,

	PortProtoTcp	= 6,	/* protocol number for TCP/IP */
	PortProtoUdp	= 17	/* protocol number for UDP/IP */
};

struct PortMap {
	u32int prog;
	u32int vers;
	u32int prot;
	u32int port;
};

struct PortTNull {
	SunCall call;
};

struct PortRNull {
	SunCall call;
};

struct PortTSet {
	SunCall call;
	PortMap map;
};

struct PortRSet {
	SunCall call;
	u1int b;
};

struct PortTUnset {
	SunCall call;
	PortMap map;
};

struct PortRUnset {
	SunCall call;
	u1int b;
};

struct PortTGetport {
	SunCall call;
	PortMap map;
};

struct PortRGetport {
	SunCall call;
	u32int port;
};

struct PortTDump {
	SunCall call;
};

struct PortRDump {
	SunCall call;
	PortMap *map;
	int nmap;
};

struct PortTCallit {
	SunCall call;
	u32int prog;
	u32int vers;
	u32int proc;
	uchar *data;
	u32int count;
};

struct PortRCallit {
	SunCall call;
	u32int port;
	uchar *data;
	u32int count;
};

extern SunProg portprog;
