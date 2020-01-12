/*
 * Structure pointed to by X field of Memimage
 */

typedef struct Xmem Xmem;
typedef struct Xprivate Xprivate;
typedef struct Xwin Xwin;

enum
{
	PMundef = ~0
};

struct Xmem
{
	int		pixmap;	/* pixmap id */
	XImage		*xi;	/* local image */
	int		dirty;	/* is the X server ahead of us?  */
	Rectangle	dirtyr;	/* which pixels? */
	Rectangle	r;	/* size of image */
};

struct Xprivate {
	u32int		chan;
	XColormap	cmap;
	XCursor		cursor;
	XDisplay	*display;
	int		fd;	/* of display */
	int		depth;				/* of screen */
	XColor		map[256];
	XColor		map7[128];
	uchar		map7to8[128][2];
	XGC		gccopy;
	XGC		gccopy0;
	XGC		gcfill;
	u32int		gcfillcolor;
	XGC		gcfill0;
	u32int		gcfill0color;
	XGC		gcreplsrc;
	u32int		gcreplsrctile;
	XGC		gcreplsrc0;
	u32int		gcreplsrc0tile;
	XGC		gcsimplesrc;
	u32int		gcsimplesrccolor;
	u32int		gcsimplesrcpixmap;
	XGC		gcsimplesrc0;
	u32int		gcsimplesrc0color;
	u32int		gcsimplesrc0pixmap;
	XGC		gczero;
	u32int		gczeropixmap;
	XGC		gczero0;
	u32int		gczero0pixmap;
	int		toplan9[256];
	int		tox11[256];
	int		usetable;
	XVisual		*vis;
	Atom		clipboard;
	Atom		utf8string;
	Atom		targets;
	Atom		text;
	Atom		compoundtext;
	Atom		takefocus;
	Atom		losefocus;
	Atom		wmprotos;
	uint		putsnarf;
	uint		assertsnarf;
	int		kbuttons;
	int		kstate;
	int		altdown;

	Xwin*	windows;
};

struct Client;

struct Xwin
{
	XDrawable	drawable;
	struct Client*	client;
	
	Rectangle	newscreenr;
	Memimage*	screenimage;
	XDrawable	screenpm;
	XDrawable	nextscreenpm;
	Rectangle	screenr;
	Rectangle	screenrect;
	Rectangle	windowrect;
	int		fullscreen;
	int		destroyed;

	Xwin*	next;
};

void xlock(void);
void xunlock(void);
extern Xprivate _x;

extern Memimage *_xallocmemimage(Rectangle, u32int, int);
extern XImage	*_xallocxdata(Memimage*, Rectangle);
extern void	_xdirtyxdata(Memimage*, Rectangle);
extern void	_xfillcolor(Memimage*, Rectangle, u32int);
extern void	_xfreexdata(Memimage*);
extern XImage	*_xgetxdata(Memimage*, Rectangle);
extern void	_xputxdata(Memimage*, Rectangle);

