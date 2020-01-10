
#define NHASH (1<<5)
#define HASHMASK (NHASH-1)

typedef struct Kbdbuf Kbdbuf;
typedef struct Mousebuf Mousebuf;
typedef struct Tagbuf Tagbuf;

typedef struct Client Client;
typedef struct Draw Draw;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct CScreen CScreen;
typedef struct FChar FChar;
typedef struct Refresh Refresh;
typedef struct Refx Refx;
typedef struct DName DName;

struct Draw
{
	QLock		lk;
};

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

struct Client
{
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

	int		rfd;
	int		wfd;
	const void*		view;
	
	QLock inputlk;
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

int _drawmsgread(Client*, void*, int);
int _drawmsgwrite(Client*, void*, int);
void _initdisplaymemimage(Client*, Memimage*);
void	_drawreplacescreenimage(Client*, Memimage*);

int _latin1(Rune*, int);
int parsewinsize(char*, Rectangle*, int*);
int mouseswap(int);

void	gfx_abortcompose(Client*);
void	gfx_keystroke(Client*, int);
void	gfx_mousetrack(Client*, int, int, int, uint);

void	rpc_setmouse(Client*, Point);
void	rpc_setcursor(Client*, Cursor*, Cursor2*);
void	rpc_setlabel(Client*, char*);
void	rpc_resizeimg(Client*);
void	rpc_resizewindow(Client*, Rectangle);
void	rpc_topwin(Client*);
char*	rpc_getsnarf(void);
void	rpc_putsnarf(char*);
Memimage *rpc_attachscreen(Client*, char*, char*);
void	rpc_flushmemscreen(Client*, Rectangle);

extern Client *client0;

void	servep9p(Client*);
