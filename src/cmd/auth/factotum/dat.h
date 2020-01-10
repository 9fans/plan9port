enum
{
	MaxRpc = 2048,	/* max size of any protocol message */

	/* keep in sync with rpc.c:/rpcname */
	RpcUnknown = 0,		/* Rpc.op */
	RpcAuthinfo,
	RpcAttr,
	RpcRead,
	RpcStart,
	RpcWrite,
	RpcReadHex,
	RpcWriteHex,

	/* thread stack size - big buffers for printing */
	STACK = 65536
};

typedef struct Conv Conv;
typedef struct Key Key;
typedef struct Logbuf Logbuf;
typedef struct Proto Proto;
typedef struct Ring Ring;
typedef struct Role Role;
typedef struct Rpc Rpc;

struct Rpc
{
	int op;
	void *data;
	int count;
	int hex;	/* should result of read be turned into hex? */
};

struct Conv
{
	int ref;			/* ref count */
	int hangup;		/* flag: please hang up */
	int active;			/* flag: there is an active thread */
	int done;			/* flag: conversation finished successfully */
	ulong tag;			/* identifying tag */
	Conv *next;		/* in linked list */
	char *sysuser;		/* system name for user speaking to us */
	char *state;		/* for debugging */
	char statebuf[128];	/* for formatted states */
	char err[ERRMAX];	/* last error */

	Attr *attr;			/* current attributes */
	Proto *proto;		/* protocol */

	Channel *rpcwait;		/* wait here for an rpc */
	Rpc rpc;				/* current rpc. op==RpcUnknown means none */
	char rpcbuf[MaxRpc];	/* buffer for rpc */
	char reply[MaxRpc];		/* buffer for response */
	int nreply;				/* count of response */
	void (*kickreply)(Conv*);	/* call to send response */
	Req *req;				/* 9P call to read response */

	Channel *keywait;	/* wait here for key confirmation */

};

struct Key
{
	int ref;			/* ref count */
	ulong tag;			/* identifying tag: sequence number */
	Attr *attr;			/* public attributes */
	Attr *privattr;		/* private attributes, like !password */
	Proto *proto;		/* protocol owner of key */
	void *priv;		/* protocol-specific storage */
};

struct Logbuf
{
	Req *wait;
	Req **waitlast;
	int rp;
	int wp;
	char *msg[128];
};

struct Ring
{
	Key **key;
	int nkey;
};

struct Proto
{
	char *name;		/* name of protocol */
	Role *roles;		/* list of roles and service functions */
	char *keyprompt;	/* required attributes for key proto=name */
	int (*checkkey)(Key*);	/* initialize k->priv or reject key */
	void (*closekey)(Key*);	/* free k->priv */
};

struct Role
{
	char *name;		/* name of role */
	int (*fn)(Conv*);	/* service function */
};

extern char	*authaddr;	/* plan9.c */
extern int		*confirminuse;	/* fs.c */
extern Conv*	conv;		/* conv.c */
extern int		debug;		/* main.c */
extern char	*factname;	/* main.c */
extern Srv		fs;			/* fs.c */
extern int		*needkeyinuse;	/* fs.c */
extern char	*owner;		/* main.c */
extern Proto	*prototab[];	/* main.c */
extern Ring	ring;			/* key.c */
extern char	*rpcname[];	/* rpc.c */

extern char	Easproto[];	/* err.c */

void fsinit0(void);

/* provided by lib9p */
#define emalloc	emalloc9p
#define erealloc	erealloc9p
#define estrdup	estrdup9p

/* hidden in libauth */
#define attrfmt		_attrfmt
#define copyattr	_copyattr
#define delattr		_delattr
#define findattr		_findattr
#define freeattr		_freeattr
#define mkattr		_mkattr
#define parseattr	_parseattr
#define strfindattr	_strfindattr

extern Attr*	addattr(Attr*, char*, ...);
/* #pragma varargck argpos addattr 2 */
extern Attr*	addattrs(Attr*, Attr*);
extern Attr*	sortattr(Attr*);
extern int		attrnamefmt(Fmt*);
/* #pragma varargck type "N" Attr* */
extern int		matchattr(Attr*, Attr*, Attr*);
extern Attr*	parseattrfmt(char*, ...);
/* #pragma varargck argpos parseattrfmt 1 */
extern Attr*	parseattrfmtv(char*, va_list);

extern void	confirmflush(Req*);
extern void	confirmread(Req*);
extern int		confirmwrite(char*);
extern int		needkey(Conv*, Attr*);
extern int		badkey(Conv*, Key*, char*, Attr*);
extern int		confirmkey(Conv*, Key*);

extern Conv*	convalloc(char*);
extern void	convclose(Conv*);
extern void	convhangup(Conv*);
extern int		convneedkey(Conv*, Attr*);
extern int		convbadkey(Conv*, Key*, char*, Attr*);
extern int		convread(Conv*, void*, int);
extern int		convreadm(Conv*, char**);
extern int		convprint(Conv*, char*, ...);
/* #pragma varargck argpos convprint 2 */
extern int		convreadfn(Conv*, int(*)(void*, int), char**);
extern void	convreset(Conv*);
extern int		convwrite(Conv*, void*, int);

extern int		ctlwrite(char*);

extern char*	estrappend(char*, char*, ...);
/* #pragma varargck argpos estrappend 2 */
extern int		hexparse(char*, uchar*, int);

extern void	keyadd(Key*);
extern Key*	keylookup(char*, ...);
extern Key*	keyiterate(int, char*, ...);
/* #pragma varargck argpos keylookup 1 */
extern Key*	keyfetch(Conv*, char*, ...);
/* #pragma varargck argpos keyfetch 2 */
extern void	keyclose(Key*);
extern void	keyevict(Conv*, Key*, char*, ...);
/* #pragma varargck argpos keyevict 3 */
extern Key*	keyreplace(Conv*, Key*, char*, ...);
/* #pragma varargck argpos keyreplace 3 */

extern void	lbkick(Logbuf*);
extern void	lbappend(Logbuf*, char*, ...);
extern void	lbvappend(Logbuf*, char*, va_list);
/* #pragma varargck argpos lbappend 2 */
extern void	lbread(Logbuf*, Req*);
extern void	lbflush(Logbuf*, Req*);
extern void	flog(char*, ...);
/* #pragma varargck argpos flog 1 */

extern void	logflush(Req*);
extern void	logread(Req*);
extern void	logwrite(Req*);

extern void	needkeyread(Req*);
extern void	needkeyflush(Req*);
extern int		needkeywrite(char*);
extern int		needkeyqueue(void);

extern Attr*	addcap(Attr*, char*, Ticket*);
extern Key*	plan9authkey(Attr*);
extern int		_authdial(char*, char*);

extern int		memrandom(void*, int);

extern Proto*	protolookup(char*);

extern int		rpcwrite(Conv*, void*, int);
extern void	rpcrespond(Conv*, char*, ...);
/* #pragma varargck argpos rpcrespond 2 */
extern void	rpcrespondn(Conv*, char*, void*, int);
extern void	rpcexec(Conv*);

extern int		xioauthdial(char*, char*);
extern void	xioclose(int);
extern int		xiodial(char*, char*, char*, int*);
extern int		xiowrite(int, void*, int);
extern int		xioasrdresp(int, void*, int);
extern int		xioasgetticket(int, char*, char*);

/* pkcs1.c - maybe should be in libsec */
typedef DigestState *DigestAlg(uchar*, ulong, uchar*, DigestState*);
int	rsasign(RSApriv*, DigestAlg*, uchar*, uint, uchar*, uint);
int	rsaverify(RSApub*, DigestAlg*, uchar*, uint, uchar*, uint);
void	mptoberjust(mpint*, uchar*, uint);


extern int		extrafactotumdir;

int		havesecstore(void);
int		secstorefetch(void);
