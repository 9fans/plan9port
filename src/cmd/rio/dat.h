/* Copyright (c) 1994-1996 David Hogan, see README for licence details */

#define BORDER		_border
#define CORNER		_corner
#define	INSET		_inset
#define MAXHIDDEN	128
#define B3FIXED 	5

#define AllButtonMask	(Button1Mask|Button2Mask|Button3Mask \
			|Button4Mask|Button5Mask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MenuMask	(ButtonMask|ButtonMotionMask|ExposureMask)
#define MenuGrabMask	(ButtonMask|ButtonMotionMask|StructureNotifyMask)

#ifdef	Plan9
#define DEFSHELL	"/bin/rc"
#else
#define DEFSHELL	"/bin/sh"
#endif

typedef struct Client Client;
typedef struct Menu Menu;
typedef struct ScreenInfo ScreenInfo;
typedef enum BorderOrient BorderOrient;

struct Client {
	Window		window;
	Window		parent;
	Window		trans;
	Client		*next;
	Client		*revert;

	int 		x;
	int 		y;
	int 		dx;
	int 		dy;
	int 		border;

	XSizeHints	size;
	int 		min_dx;
	int 		min_dy;

	int 		state;
	int 		init;
	int 		reparenting;
	int 		is9term;
	int 		hold;
	int 		proto;

	char		*label;
	char		*instance;
	char		*class;
	char		*name;
	char		*iconname;

	Colormap	cmap;
	int 		ncmapwins;
	Window		*cmapwins;
	Colormap	*wmcmaps;
	ScreenInfo	*screen;
};

#define hidden(c)	((c)->state == IconicState)
#define withdrawn(c)	((c)->state == WithdrawnState)
#define normal(c)	((c)->state == NormalState)

/* c->proto */
#define Pdelete 	1
#define Ptakefocus	2
#define Plosefocus	4

struct Menu {
	char	**item;
	char	*(*gen)();
	int	lasthit;
};

enum BorderOrient {
	BorderUnknown = 0, /* we depend on this!*/
	BorderN,
	BorderNNE,
	BorderENE,
	BorderE,
	BorderESE,
	BorderSSE,
	BorderS,
	BorderSSW,
	BorderWSW,
	BorderW,
	BorderWNW,
	BorderNNW,
	NBorder,
};

struct ScreenInfo {
	int			num;
	int			depth;
	Visual		*vis;
	int			width;
	int			height;
	Window		root;
	Window		menuwin;
	Window		sweepwin;
	Colormap		def_cmap;
	GC			gc;
	GC			gccopy;
	GC			gcred;
	GC			gcsweep;
	GC			gcmenubg;
	GC			gcmenubgs;
	GC			gcmenufg;
	GC			gcmenufgs;
	unsigned long	black;
	unsigned long	white;
	unsigned long	activeholdborder;
	unsigned long	inactiveholdborder;
	unsigned long	activeborder;
	unsigned long	inactiveborder;
	unsigned long	red;
	Pixmap		bkup[2];
	int			min_cmaps;
	Cursor		target;
	Cursor		sweep0;
	Cursor		boxcurs;
	Cursor		arrow;
	Cursor		bordcurs[NBorder];
	Pixmap		root_pixmap;
	char			display[256];	/* arbitrary limit */
};

/* main.c */
extern Display		*dpy;
extern ScreenInfo	*screens;
extern int			num_screens;
extern int			initting;
extern XFontStruct	*font;
extern int			nostalgia;
extern char		**myargv;
extern Bool 		shape;
extern char 		*termprog;
extern char 		*shell;
extern char 		*version[];
extern int			_border;
extern int			_corner;
extern int			_inset;
extern int			curtime;
extern int			debug;
extern int			solidsweep;

extern Atom		exit_rio;
extern Atom		restart_rio;
extern Atom 		wm_state;
extern Atom		wm_change_state;
extern Atom 		_rio_hold_mode;
extern Atom 		wm_protocols;
extern Atom 		wm_delete;
extern Atom 		wm_take_focus;
extern Atom		wm_lose_focus;
extern Atom 		wm_colormaps;

/* client.c */
extern Client		*clients;
extern Client		*current;

/* menu.c */
extern Client		*hiddenc[];
extern int 			numhidden;
extern char 		*b3items[];
extern Menu 		b3menu;

/* manage.c */
extern int			isNew;

/* error.c */
extern int 			ignore_badwindow;
