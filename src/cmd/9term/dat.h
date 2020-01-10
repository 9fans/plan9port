#define STACK 32768
#undef Borderwidth
#define Borderwidth 0

#undef TRUE	/* OS X */
#undef FALSE

typedef	struct	Consreadmesg Consreadmesg;
typedef	struct	Conswritemesg Conswritemesg;
typedef	struct	Stringpair Stringpair;
typedef	struct	Dirtab Dirtab;
typedef	struct	Mouseinfo	Mouseinfo;
typedef	struct	Mousereadmesg Mousereadmesg;
typedef	struct	Mousestate	Mousestate;
typedef	struct	Timer Timer;
typedef	struct	Wctlmesg Wctlmesg;
typedef	struct	Window Window;

enum
{
	Selborder		= 0,		/* border of selected window */
	Unselborder	= 0,		/* border of unselected window */
	Scrollwid 		= 12,		/* width of scroll bar */
	Scrollgap 		= 4,		/* gap right of scroll bar */
	BIG			= 3,		/* factor by which window dimension can exceed screen */
	TRUE		= 1,
	FALSE		= 0
};

enum
{
	Kscrolloneup = KF|0x20,
	Kscrollonedown = KF|0x21
};

enum	/* control messages */
{
	Wakeup,
	Reshaped,
	Moved,
	Refresh,
	Movemouse,
	Rawon,
	Rawoff,
	Holdon,
	Holdoff,
	Deleted,
	Exited
};

struct Wctlmesg
{
	int		type;
	Rectangle	r;
	Image	*image;
};

struct Conswritemesg
{
	Channel	*cw;		/* chan(Stringpair) */
};

struct Consreadmesg
{
	Channel	*c1;		/* chan(tuple(char*, int) == Stringpair) */
	Channel	*c2;		/* chan(tuple(char*, int) == Stringpair) */
};

struct Mousereadmesg
{
	Channel	*cm;		/* chan(Mouse) */
};

struct Stringpair	/* rune and nrune or byte and nbyte */
{
	void		*s;
	int		ns;
};

struct Mousestate
{
	Mouse	m;
	ulong	counter;	/* serial no. of mouse event */
};

struct Mouseinfo
{
	Mousestate	queue[16];
	int	ri;	/* read index into queue */
	int	wi;	/* write index */
	ulong	counter;	/* serial no. of last mouse event we received */
	ulong	lastcounter;	/* serial no. of last mouse event sent to client */
	int	lastb;	/* last button state we received */
	uchar	qfull;	/* filled the queue; no more recording until client comes back */
};

struct Window
{
	Ref	ref;
	QLock	lk;
	Frame	f;
	Image		*i;
	Mousectl		mc;
	Mouseinfo	mouse;
	Channel		*ck;			/* chan(Rune[10]) */
	Channel		*cctl;		/* chan(Wctlmesg)[20] */
	Channel		*conswrite;	/* chan(Conswritemesg) */
	Channel		*consread;	/* chan(Consreadmesg) */
	Channel		*mouseread;	/* chan(Mousereadmesg) */
	Channel		*wctlread;		/* chan(Consreadmesg) */
	uint			nr;			/* number of runes in window */
	uint			maxr;		/* number of runes allocated in r */
	Rune			*r;
	uint			nraw;
	Rune			*raw;
	uint			org;
	uint			q0;
	uint			q1;
	uint			qh;
	uint			iq1;			/* last input position */
	int			id;
	char			name[32];
	uint			namecount;
	Rectangle		scrollr;
	/*
	 * Rio once used originwindow, so screenr could be different from i->r.
	 * Now they're always the same but the code doesn't assume so.
	*/
	Rectangle		screenr;	/* screen coordinates of window */
	int			resized;
	int			wctlready;
	Rectangle		lastsr;
	int			topped;
	int			notefd;
	uchar		scrolling;
	Cursor		cursor;
	Cursor		*cursorp;
	uchar		holding;
	uchar		rawing;
	uchar		ctlopen;
	uchar		wctlopen;
	uchar		deleted;
	uchar		mouseopen;
	char			*label;
	int			pid;
	char			*dir;
};

int		winborder(Window*, Point);
void		winctl(void*);
void		winshell(void*);
Window*	wlookid(int);
Window*	wmk(Image*, Mousectl*, Channel*, Channel*, int);
Window*	wpointto(Point);
Window*	wtop(Point);
void		wtopme(Window*);
void		wbottomme(Window*);
char*	wcontents(Window*, int*);
int		wbswidth(Window*, Rune);
int		wclickmatch(Window*, int, int, int, uint*);
int		wclose(Window*);
int		wctlmesg(Window*, int, Rectangle, Image*);
int		wctlmesg(Window*, int, Rectangle, Image*);
uint		wbacknl(Window*, uint, uint);
uint		winsert(Window*, Rune*, int, uint);
void		waddraw(Window*, Rune*, int);
void		wborder(Window*, int);
void		wclosewin(Window*);
void		wcurrent(Window*);
void		wcut(Window*);
void		wdelete(Window*, uint, uint);
void		wdoubleclick(Window*, uint*, uint*);
void		wfill(Window*);
void		wframescroll(Window*, int);
void		wkeyctl(Window*, Rune);
void		wmousectl(Window*);
void		wmovemouse(Window*, Point);
void		wpaste(Window*);
void		wplumb(Window*);
void		wlook(Window*);
void		wrefresh(Window*, Rectangle);
void		wrepaint(Window*);
void		wresize(Window*, Image*, int);
void		wscrdraw(Window*);
void		wscroll(Window*, int);
void		wselect(Window*);
void		wsendctlmesg(Window*, int, Rectangle, Image*);
void		wsetcursor(Window*, int);
void		wsetname(Window*);
void		wsetorigin(Window*, uint, int);
void		wsetpid(Window*, int, int);
void		wsetselect(Window*, uint, uint);
void		wshow(Window*, uint);
void		wsnarf(Window*);
void 		wscrsleep(Window*, uint);
void		wsetcols(Window*);

void		deletetimeoutproc(void*);

struct Timer
{
	int		dt;
	int		cancel;
	Channel	*c;	/* chan(int) */
	Timer	*next;
};

#ifndef Extern
#define Extern extern
#endif

Extern	Font		*font;
Extern	Mousectl	*mousectl;
Extern	Mouse	*mouse;
Extern	Keyboardctl	*keyboardctl;
Extern	Display	*display;
Extern	Image	*view;
Extern	Screen	*wscreen;
Extern	Cursor	boxcursor;
Extern	Cursor	crosscursor;
Extern	Cursor	sightcursor;
Extern	Cursor	whitearrow;
Extern	Cursor	query;
Extern	Cursor	*corners[9];
Extern	Image	*background;
Extern	Image	*lightgrey;
Extern	Image	*red;
Extern	Window	**window;
Extern	Window	*wkeyboard;	/* window of simulated keyboard */
Extern	int		nwindow;
Extern	int		snarffd;
Extern	Window	*input;
Extern	QLock	all;			/* BUG */
Extern	Window	*hidden[100];
Extern	int		nhidden;
Extern	int		nsnarf;
Extern	Rune*	snarf;
Extern	int		scrolling;
Extern	int		maxtab;
Extern	Channel*	winclosechan;
Extern	Channel*	deletechan;
Extern	char		*startdir;
Extern	int		sweeping;
Extern	int		wctlfd;
Extern	int		errorshouldabort;
Extern	int		menuing;		/* menu action is pending; waiting for window to be indicated */
Extern	int		snarfversion;	/* updated each time it is written */
Extern	int		messagesize;		/* negotiated in 9P version setup */
