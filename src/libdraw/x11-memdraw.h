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
	Atom		takefocus;
	Atom		losefocus;
	Atom		wmprotos;
	uint		putsnarf;
	uint		assertsnarf;
	int		destroyed;
};

extern Xprivate _x;

extern Memimage *_xallocmemimage(Rectangle, u32int, int);
extern XImage	*_xallocxdata(Memimage*, Rectangle);
extern void	_xdirtyxdata(Memimage*, Rectangle);
extern void	_xfillcolor(Memimage*, Rectangle, u32int);
extern void	_xfreexdata(Memimage*);
extern XImage	*_xgetxdata(Memimage*, Rectangle);
extern void	_xputxdata(Memimage*, Rectangle);
extern void	_initdisplaymemimage(Display*, Memimage*);

struct Mouse;
extern int	_xtoplan9mouse(XDisplay*, XEvent*, struct Mouse*);
extern int	_xtoplan9kbd(XEvent*);
extern void	_xexpose(XEvent*, XDisplay*);
extern int	_xselect(XEvent*, XDisplay*);
extern int	_xconfigure(XEvent*, XDisplay*);
extern int	_xdestroy(XEvent*, XDisplay*);
extern void	_flushmemscreen(Rectangle);
extern void	_xmoveto(Point);
struct Cursor;
extern void	_xsetcursor(struct Cursor*);

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

