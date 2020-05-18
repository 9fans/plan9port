
#define NHASH (1<<5)
#define HASHMASK (NHASH-1)

typedef struct Kbdbuf Kbdbuf;
typedef struct Mousebuf Mousebuf;
typedef struct Tagbuf Tagbuf;

typedef struct Client Client;
typedef struct ClientImpl ClientImpl;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct CScreen CScreen;
typedef struct FChar FChar;
typedef struct Refresh Refresh;
typedef struct Refx Refx;
typedef struct DName DName;

struct Kbdbuf
{
	Rune r[256];
	int ri;
	int wi;
	int stall;
	int alting;
	Rune k[10];
	int nk;
};

struct Mousebuf
{
	Mouse m[256];
	Mouse last;
	int ri;
	int wi;
	int stall;
	int resized;
};

struct Tagbuf
{
	int t[256];
	int ri;
	int wi;
};

struct ClientImpl
{
	void (*rpc_resizeimg)(Client*);
	void (*rpc_resizewindow)(Client*, Rectangle);
	void (*rpc_setcursor)(Client*, Cursor*, Cursor2*);
	void (*rpc_setlabel)(Client*, char*);
	void (*rpc_setmouse)(Client*, Point);
	void (*rpc_topwin)(Client*);
	void (*rpc_bouncemouse)(Client*, Mouse);
	void (*rpc_flush)(Client*, Rectangle);
};

extern QLock drawlk;

struct Client
{
	int		rfd;

	// wfdlk protects writes to wfd, which can be issued from either
	// the RPC thread or the graphics thread.
	QLock	wfdlk;
	int		wfd;
	uchar*	mbuf;
	int		nmbuf;

	char*	wsysid;

	// drawlk protects the draw data structures for all clients.
	// It can be acquired by an RPC thread or a graphics thread
	// but must not be held on one thread while waiting for the other.
	/*Ref		r;*/
	DImage*		dimage[NHASH];
	CScreen*	cscreen;
	Refresh*	refresh;
	Rendez		refrend;
	uchar*		readdata;
	int		nreaddata;
	int		busy;
	int		clientid;
	int		slot;
	int		refreshme;
	int		infoid;
	int		op;
	int		displaydpi;
	int		forcedpi;
	int		waste;
	Rectangle	flushrect;
	Memimage	*screenimage;
	DScreen*	dscreen;
	int		nname;
	DName*		name;
	int		namevers;
	ClientImpl*	impl;

	// Only accessed/modified by the graphics thread.
	const void*		view;

	// eventlk protects the keyboard and mouse events.
	QLock eventlk;
	Kbdbuf kbd;
	Mousebuf mouse;
	Tagbuf kbdtags;
	Tagbuf mousetags;
	Rectangle mouserect;
};

struct Refresh
{
	DImage*		dimage;
	Rectangle	r;
	Refresh*	next;
};

struct Refx
{
	Client*		client;
	DImage*		dimage;
};

struct DName
{
	char			*name;
	Client	*client;
	DImage*		dimage;
	int			vers;
};

struct FChar
{
	int		minx;	/* left edge of bits */
	int		maxx;	/* right edge of bits */
	uchar		miny;	/* first non-zero scan-line */
	uchar		maxy;	/* last non-zero scan-line + 1 */
	schar		left;	/* offset of baseline */
	uchar		width;	/* width of baseline */
};

/*
 * Reference counts in DImages:
 *	one per open by original client
 *	one per screen image or fill
 * 	one per image derived from this one by name
 */
struct DImage
{
	int		id;
	int		ref;
	char		*name;
	int		vers;
	Memimage*	image;
	int		ascent;
	int		nfchar;
	FChar*		fchar;
	DScreen*	dscreen;	/* 0 if not a window */
	DImage*	fromname;	/* image this one is derived from, by name */
	DImage*		next;
};

struct CScreen
{
	DScreen*	dscreen;
	CScreen*	next;
};

struct DScreen
{
	int		id;
	int		public;
	int		ref;
	DImage	*dimage;
	DImage	*dfill;
	Memscreen*	screen;
	Client*		owner;
	DScreen*	next;
};

// For the most part, the graphics driver-specific code in files
// like mac-screen.m runs in the graphics library's main thread,
// while the RPC service code in srv.c runs on the RPC service thread.
// The exceptions in each file, which are called by the other,
// are marked with special prefixes: gfx_* indicates code that
// is in srv.c but nonetheless runs on the main graphics thread,
// while rpc_* indicates code that is in, say, mac-screen.m but
// nonetheless runs on the RPC service thread.
//
// The gfx_* and rpc_* calls typically synchronize with the other
// code in the file by acquiring a lock (or running a callback on the
// target thread, which amounts to the same thing).
// To avoid deadlock, callers of those routines must not hold any locks.

// gfx_* routines are called on the graphics thread,
// invoked from graphics driver callbacks to do RPC work.
// No locks are held on entry.
void	gfx_abortcompose(Client*);
void	gfx_keystroke(Client*, int);
void	gfx_main(void);
void	gfx_mousetrack(Client*, int, int, int, uint);
void	gfx_replacescreenimage(Client*, Memimage*);
void	gfx_mouseresized(Client*);
void	gfx_started(void);

// rpc_* routines are called on the RPC thread,
// invoked by the RPC server code to do graphics work.
// No locks are held on entry.
Memimage *rpc_attach(Client*, char*, char*);
char*	rpc_getsnarf(void);
void	rpc_putsnarf(char*);
void	rpc_shutdown(void);
void	rpc_main(void);

// rpc_gfxdrawlock and rpc_gfxdrawunlock
// are called around drawing operations to lock and unlock
// access to the graphics display, for systems where the
// individual memdraw operations use the graphics display (X11, not macOS).
void rpc_gfxdrawlock(void);
void rpc_gfxdrawunlock(void);

// draw* routines are called on the RPC thread,
// invoked by the RPC server to do pixel pushing.
// No locks are held on entry.
int draw_dataread(Client*, void*, int);
int draw_datawrite(Client*, void*, int);
void draw_initdisplaymemimage(Client*, Memimage*);

// utility routines
int latin1(Rune*, int);
int mouseswap(int);
int parsewinsize(char*, Rectangle*, int*);

extern Client *client0; // set in single-client mode
