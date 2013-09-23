#include <auth.h>
#include <fcall.h>

enum {
	NFidHash	= 503,
};

typedef struct Con Con;
typedef struct DirBuf DirBuf;
typedef struct Excl Excl;
typedef struct Fid Fid;
typedef struct Fsys Fsys;
typedef struct Msg Msg;

#pragma incomplete DirBuf
#pragma incomplete Excl
#pragma incomplete Fsys

struct Msg {
	uchar*	data;
	u32int	msize;			/* actual size of data */
	Fcall	t;
	Fcall	r;
	Con*	con;

	Msg*	anext;			/* allocation free list */

	Msg*	mnext;			/* all active messsages on this Con */
	Msg* 	mprev;

	int	state;			/* */

	Msg*	flush;			/* flushes waiting for this Msg */

	Msg*	rwnext;			/* read/write queue */
	int	nowq;			/* do not place on write queue */
};

enum {
	MsgN		= 0,
	MsgR		= 1,
	Msg9		= 2,
	MsgW		= 3,
	MsgF		= 4,
};

enum {
	ConNoneAllow	= 1<<0,
	ConNoAuthCheck	= 1<<1,
	ConNoPermCheck	= 1<<2,
	ConWstatAllow	= 1<<3,
	ConIPCheck	= 1<<4,
};
struct Con {
	char*	name;
	uchar*	data;			/* max, not negotiated */
	int	isconsole;		/* immutable */
	int	flags;			/* immutable */
	char	remote[128];		/* immutable */
	VtLock*	lock;
	int	state;
	int	fd;
	Msg*	version;
	u32int	msize;			/* negotiated with Tversion */
	VtRendez* rendez;

	Con*	anext;			/* alloc */
	Con*	cnext;			/* in use */
	Con*	cprev;

	VtLock*	alock;
	int	aok;			/* authentication done */

	VtLock*	mlock;
	Msg*	mhead;			/* all Msgs on this connection */
	Msg*	mtail;
	VtRendez* mrendez;

	VtLock*	wlock;
	Msg*	whead;			/* write queue */
	Msg*	wtail;
	VtRendez* wrendez;

	VtLock*	fidlock;		/* */
	Fid*	fidhash[NFidHash];
	Fid*	fhead;
	Fid*	ftail;
	int	nfid;
};

enum {
	ConDead		= 0,
	ConNew		= 1,
	ConDown		= 2,
	ConInit		= 3,
	ConUp		= 4,
	ConMoribund	= 5,
};

struct Fid {
	VtLock*	lock;
	Con*	con;
	u32int	fidno;
	int	ref;			/* inc/dec under Con.fidlock */
	int	flags;

	int	open;
	Fsys*	fsys;
	File*	file;
	Qid	qid;
	char*	uid;
	char*	uname;
	DirBuf*	db;
	Excl*	excl;

	VtLock*	alock;			/* Tauth/Tattach */
	AuthRpc* rpc;
	char*	cuname;

	Fid*	sort;			/* sorted by uname in cmdWho */
	Fid*	hash;			/* lookup by fidno */
	Fid*	next;			/* clunk session with Tversion */
	Fid*	prev;
};

enum {					/* Fid.flags and fidGet(..., flags) */
	FidFCreate	= 0x01,
	FidFWlock	= 0x02,
};

enum {					/* Fid.open */
	FidOCreate	= 0x01,
	FidORead	= 0x02,
	FidOWrite	= 0x04,
	FidORclose	= 0x08,
};

/*
 * 9p.c
 */
extern int (*rFcall[Tmax])(Msg*);
extern int validFileName(char*);

/*
 * 9auth.c
 */
extern int authCheck(Fcall*, Fid*, Fsys*);
extern int authRead(Fid*, void*, int);
extern int authWrite(Fid*, void*, int);

/*
 * 9dir.c
 */
extern void dirBufFree(DirBuf*);
extern int dirDe2M(DirEntry*, uchar*, int);
extern int dirRead(Fid*, uchar*, int, vlong);

/*
 * 9excl.c
 */
extern int exclAlloc(Fid*);
extern void exclFree(Fid*);
extern void exclInit(void);
extern int exclUpdate(Fid*);

/*
 * 9fid.c
 */
extern void fidClunk(Fid*);
extern void fidClunkAll(Con*);
extern Fid* fidGet(Con*, u32int, int);
extern void fidInit(void);
extern void fidPut(Fid*);

/*
 * 9fsys.c
 */
extern void fsysFsRlock(Fsys*);
extern void fsysFsRUnlock(Fsys*);
extern Fs* fsysGetFs(Fsys*);
extern Fsys* fsysGet(char*);
extern char* fsysGetName(Fsys*);
extern File* fsysGetRoot(Fsys*, char*);
extern Fsys* fsysIncRef(Fsys*);
extern int fsysInit(void);
extern int fsysNoAuthCheck(Fsys*);
extern int fsysNoPermCheck(Fsys*);
extern void fsysPut(Fsys*);
extern int fsysWstatAllow(Fsys*);

/*
 * 9lstn.c
 */
extern int lstnInit(void);

/*
 * 9proc.c
 */
extern Con* conAlloc(int, char*, int);
extern void conInit(void);
extern void msgFlush(Msg*);
extern void msgInit(void);

/*
 * 9srv.c
 */
extern int srvInit(void);

/*
 * 9user.c
 */
extern int groupLeader(char*, char*);
extern int groupMember(char*, char*);
extern int groupWriteMember(char*);
extern char* unameByUid(char*);
extern char* uidByUname(char*);
extern int usersInit(void);
extern int usersFileRead(char*);
extern int validUserName(char*);

extern char* uidadm;
extern char* unamenone;
extern char* uidnoworld;

/*
 * Ccli.c
 */
extern int cliAddCmd(char*, int (*)(int, char*[]));
extern int cliError(char*, ...);
extern int cliInit(void);
extern int cliExec(char*);
#pragma	varargck	argpos	cliError	1

/*
 * Ccmd.c
 */
extern int cmdInit(void);

/*
 * Ccons.c
 */
extern int consPrompt(char*);
extern int consInit(void);
extern int consOpen(int, int, int);
extern int consTTY(void);
extern int consWrite(char*, int);

/*
 * Clog.c
 */
extern int consPrint(char*, ...);
extern int consVPrint(char*, va_list);
#pragma	varargck	argpos	consPrint	1

/*
 * fossil.c
 */
extern int Dflag;
