/*
 * Structure pointed to by X field of Memimage
 */

typedef struct Xmem Xmem;
typedef struct Xprivate Xprivate;

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
	u32int		black;
	u32int		chan;
	XColormap	cmap;
	XCursor		cursor;
	XDisplay	*display;
	int		depth;				/* of screen */
	XDrawable	drawable;
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
	XDisplay	*kbdcon;
	XDisplay	*mousecon;
	Rectangle	newscreenr;
	Memimage*	screenimage;
	QLock		screenlock;
	XDrawable	screenpm;
	XDrawable	nextscreenpm;
	Rectangle	screenr;
	XDisplay	*snarfcon;
	int		toplan9[256];
	int		tox11[256];
	int		usetable;
	XVisual		*vis;
	u32int		white;
	Atom		clipboard;
	Atom		utf8string;
	Atom		targets;
	Atom		text;
	Atom		compoundtext;
	uint		putsnarf;
	uint		assertsnarf;
	int		destroyed;
};

extern Xprivate _x;

extern Memimage *xallocmemimage(Rectangle, u32int, int);
extern XImage	*xallocxdata(Memimage*, Rectangle);
extern void	xdirtyxdata(Memimage*, Rectangle);
extern void	xfillcolor(Memimage*, Rectangle, u32int);
extern void	xfreexdata(Memimage*);
extern XImage	*xgetxdata(Memimage*, Rectangle);
extern void	xputxdata(Memimage*, Rectangle);
extern void	_initdisplaymemimage(Display*, Memimage*);

struct Mouse;
extern int	xtoplan9mouse(XDisplay*, XEvent*, struct Mouse*);
extern int	xtoplan9kbd(XEvent*);
extern void	xexpose(XEvent*, XDisplay*);
extern int	xselect(XEvent*, XDisplay*);
extern int	xconfigure(XEvent*, XDisplay*);
extern int	xdestroy(XEvent*, XDisplay*);
extern void	flushmemscreen(Rectangle);
extern void	xmoveto(Point);
struct Cursor;
extern void	xsetcursor(struct Cursor*);

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

