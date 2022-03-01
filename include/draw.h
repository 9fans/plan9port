#ifndef _DRAW_H_
#define _DRAW_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(draw)
#ifdef __APPLE__
AUTOFRAMEWORK(Carbon)
#endif

typedef struct	Cachefont Cachefont;
typedef struct	Cacheinfo Cacheinfo;
typedef struct	Cachesubf Cachesubf;
typedef struct	Display Display;
typedef struct	Font Font;
typedef struct	Fontchar Fontchar;
typedef struct	Image Image;
typedef struct	Mouse Mouse;
typedef struct	Point Point;
typedef struct	Rectangle Rectangle;
typedef struct	RGB RGB;
typedef struct	Screen Screen;
typedef struct	Subfont Subfont;

struct Mux;

extern	int	Rfmt(Fmt*);
extern	int	Pfmt(Fmt*);

#define 	DOpaque		0xFFFFFFFF
#define 	DTransparent	0x00000000		/* only useful for allocimage memfillcolor */
#define 	DBlack		0x000000FF
#define 	DWhite		0xFFFFFFFF
#define 	DRed		0xFF0000FF
#define 	DGreen		0x00FF00FF
#define 	DBlue		0x0000FFFF
#define 	DCyan		0x00FFFFFF
#define 	DMagenta		0xFF00FFFF
#define 	DYellow		0xFFFF00FF
#define 	DPaleyellow	0xFFFFAAFF
#define 	DDarkyellow	0xEEEE9EFF
#define 	DDarkgreen	0x448844FF
#define 	DPalegreen	0xAAFFAAFF
#define 	DMedgreen	0x88CC88FF
#define 	DDarkblue	0x000055FF
#define 	DPalebluegreen 0xAAFFFFFF
#define 	DPaleblue		0x0000BBFF
#define 	DBluegreen	0x008888FF
#define 	DGreygreen	0x55AAAAFF
#define 	DPalegreygreen	0x9EEEEEFF
#define 	DYellowgreen	0x99994CFF
#define 	DMedblue		0x000099FF
#define 	DGreyblue	0x005DBBFF
#define 	DPalegreyblue	0x4993DDFF
#define 	DPurpleblue	0x8888CCFF

#define 	DNotacolor	0xFFFFFF00
#define 	DNofill		DNotacolor

enum
{
	Displaybufsize	= 8000,
	ICOSSCALE	= 1024,
	Borderwidth	= 4,
	DefaultDPI	= 133
};

enum
{
	/* refresh methods */
	Refbackup	= 0,
	Refnone		= 1,
	Refmesg		= 2
};
#define	NOREFRESH	((void*)-1)

enum
{
	/* line ends */
	Endsquare	= 0,
	Enddisc		= 1,
	Endarrow	= 2,
	Endmask		= 0x1F
};

#define	ARROW(a, b, c)	(Endarrow|((a)<<5)|((b)<<14)|((c)<<23))

typedef enum
{
	/* Porter-Duff compositing operators */
	Clear	= 0,

	SinD	= 8,
	DinS	= 4,
	SoutD	= 2,
	DoutS	= 1,

	S		= SinD|SoutD,
	SoverD	= SinD|SoutD|DoutS,
	SatopD	= SinD|DoutS,
	SxorD	= SoutD|DoutS,

	D		= DinS|DoutS,
	DoverS	= DinS|DoutS|SoutD,
	DatopS	= DinS|SoutD,
	DxorS	= DoutS|SoutD,	/* == SxorD */

	Ncomp = 12
} Drawop;

/*
 * image channel descriptors 
 */
enum {
	CRed = 0,
	CGreen,
	CBlue,
	CGrey,
	CAlpha,
	CMap,
	CIgnore,
	NChan
};

#define __DC(type, nbits)	((((type)&15)<<4)|((nbits)&15))
#define CHAN1(a,b)	__DC(a,b)
#define CHAN2(a,b,c,d)	(CHAN1((a),(b))<<8|__DC((c),(d)))
#define CHAN3(a,b,c,d,e,f)	(CHAN2((a),(b),(c),(d))<<8|__DC((e),(f)))
#define CHAN4(a,b,c,d,e,f,g,h)	(CHAN3((a),(b),(c),(d),(e),(f))<<8|__DC((g),(h)))

#define NBITS(c) ((c)&15)
#define TYPE(c) (((c)>>4)&15)

enum {
	GREY1	= CHAN1(CGrey, 1),
	GREY2	= CHAN1(CGrey, 2),
	GREY4	= CHAN1(CGrey, 4),
	GREY8	= CHAN1(CGrey, 8),
	CMAP8	= CHAN1(CMap, 8),
	RGB15	= CHAN4(CIgnore, 1, CRed, 5, CGreen, 5, CBlue, 5),
	RGB16	= CHAN3(CRed, 5, CGreen, 6, CBlue, 5),
	RGB24	= CHAN3(CRed, 8, CGreen, 8, CBlue, 8),
	BGR24	= CHAN3(CBlue, 8, CGreen, 8, CRed, 8),
	RGBA32	= CHAN4(CRed, 8, CGreen, 8, CBlue, 8, CAlpha, 8),
	ARGB32	= CHAN4(CAlpha, 8, CRed, 8, CGreen, 8, CBlue, 8),	/* stupid VGAs */
	ABGR32	= CHAN4(CAlpha, 8, CBlue, 8, CGreen, 8, CRed, 8),
	XRGB32  = CHAN4(CIgnore, 8, CRed, 8, CGreen, 8, CBlue, 8),
	XBGR32  = CHAN4(CIgnore, 8, CBlue, 8, CGreen, 8, CRed, 8)
};

extern	char*	chantostr(char*, u32int);
extern	u32int	strtochan(char*);
extern	int		chantodepth(u32int);

struct	Point
{
	int	x;
	int	y;
};

struct Rectangle
{
	Point	min;
	Point	max;
};

typedef void	(*Reffn)(Image*, Rectangle, void*);

struct Screen
{
	Display	*display;	/* display holding data */
	int	id;		/* id of system-held Screen */
	Image	*image;		/* unused; for reference only */
	Image	*fill;		/* color to paint behind windows */
};

struct Display
{
	QLock		qlock;
	int		locking;	/*program is using lockdisplay */
	int		dirno;
	int		imageid;
	int		local;
	void		(*error)(Display*, char*);
	char		*devdir;
	char		*windir;
	char		oldlabel[64];
	u32int		dataqid;
	Image		*image;
	Image		*white;
	Image		*black;
	Image		*opaque;
	Image		*transparent;
	uchar		*buf;
	int		bufsize;
	uchar		*bufp;
	uchar		*obuf;
	int		obufsize;
	uchar		*obufp;
	Font		*defaultfont;
	Image		*windows;
	Image		*screenimage;
	int		_isnewdisplay;
	struct Mux	*mux;
	int		srvfd;
	int		dpi;
	
	Font	*firstfont;
	Font	*lastfont;
};

struct Image
{
	Display		*display;	/* display holding data */
	int		id;		/* id of system-held Image */
	Rectangle	r;		/* rectangle in data area, local coords */
	Rectangle 	clipr;		/* clipping region */
	int		depth;		/* number of bits per pixel */
	u32int	chan;
	int		repl;		/* flag: data replicates to tile clipr */
	Screen		*screen;	/* 0 if not a window */
	Image		*next;	/* next in list of windows */
};

struct RGB
{
	u32int	red;
	u32int	green;
	u32int	blue;
};

/*
 * Subfonts
 *
 * given char c, Subfont *f, Fontchar *i, and Point p, one says
 *	i = f->info+c;
 *	void(b, Rect(p.x+i->left, p.y+i->top,
 *		p.x+i->left+((i+1)->x-i->x), p.y+i->bottom),
 *		color, f->bits, Pt(i->x, i->top));
 *	p.x += i->width;
 * to draw characters in the specified color (itself an Image) in Image b.
 */

struct	Fontchar
{
	int		x;		/* left edge of bits */
	uchar		top;		/* first non-zero scan-line */
	uchar		bottom;		/* last non-zero scan-line + 1 */
	char		left;		/* offset of baseline */
	uchar		width;		/* width of baseline */
};

struct	Subfont
{
	char		*name;
	short		n;		/* number of chars in font */
	uchar		height;		/* height of image */
	char		ascent;		/* top of image to baseline */
	Fontchar 	*info;		/* n+1 character descriptors */
	Image		*bits;		/* of font */
	int		ref;
};

enum
{
	/* starting values */
	LOG2NFCACHE =	6,
	NFCACHE =	(1<<LOG2NFCACHE),	/* #chars cached */
	NFLOOK =	5,			/* #chars to scan in cache */
	NFSUBF =	2,			/* #subfonts to cache */
	/* max value */
	MAXFCACHE =	1024+NFLOOK,		/* upper limit */
	MAXSUBF =	50,			/* generous upper limit */
	/* deltas */
	DSUBF = 	4,
	/* expiry ages */
	SUBFAGE	=	10000,
	CACHEAGE =	10000
};

struct Cachefont
{
	Rune		min;	/* lowest rune value to be taken from subfont */
	Rune		max;	/* highest rune value+1 to be taken from subfont */
	int		offset;	/* position in subfont of character at min */
	char		*name;			/* stored in font */
	char		*subfontname;		/* to access subfont */
};

struct Cacheinfo
{
	int		x;		/* left edge of bits */
	uchar		width;		/* width of baseline */
	schar		left;		/* offset of baseline */
	Rune		value;	/* value of character at this slot in cache */
	ushort		age;
};

struct Cachesubf
{
	u32int		age;	/* for replacement */
	Cachefont	*cf;	/* font info that owns us */
	Subfont		*f;	/* attached subfont */
};

struct Font
{
	char		*name;
	char		*namespec;
	Display		*display;
	short		height;	/* max height of image, interline spacing */
	short		ascent;	/* top of image to baseline */
	short		width;	/* widest so far; used in caching only */
	int		nsub;	/* number of subfonts */
	u32int		age;	/* increasing counter; used for LRU */
	int		maxdepth;	/* maximum depth of all loaded subfonts */
	int		ncache;	/* size of cache */
	int		nsubf;	/* size of subfont list */
	int		scale;	/* pixel scaling to apply */
	Cacheinfo	*cache;
	Cachesubf	*subf;
	Cachefont	**sub;	/* as read from file */
	Image		*cacheimage;
	
	/* doubly linked list of fonts known to display */
	int ondisplaylist;
	Font *next;
	Font *prev;
	
	/* on hi-dpi systems, one of these is set to f and the other is the other-dpi version of f */
	Font	*lodpi;
	Font	*hidpi;
};

#define	Dx(r)	((r).max.x-(r).min.x)
#define	Dy(r)	((r).max.y-(r).min.y)

/*
 * Image management
 */
extern Image*	_allocimage(Image*, Display*, Rectangle, u32int, int, u32int, int, int);
extern Image*	allocimage(Display*, Rectangle, u32int, int, u32int);
extern uchar*	bufimage(Display*, int);
extern int	bytesperline(Rectangle, int);
extern void	closedisplay(Display*);
extern void	drawerror(Display*, char*);
extern int	flushimage(Display*, int);
extern int	freeimage(Image*);
extern int	_freeimage1(Image*);
extern int	geninitdraw(char*, void(*)(Display*, char*), char*, char*, char*, int);
extern int	initdraw(void(*)(Display*, char*), char*, char*);
extern int	newwindow(char*);
extern int	loadimage(Image*, Rectangle, uchar*, int);
extern int	cloadimage(Image*, Rectangle, uchar*, int);
extern int	getwindow(Display*, int);
extern int	gengetwindow(Display*, char*, Image**, Screen**, int);
extern Image* readimage(Display*, int, int);
extern Image* creadimage(Display*, int, int);
extern int	unloadimage(Image*, Rectangle, uchar*, int);
extern int	wordsperline(Rectangle, int);
extern int	writeimage(int, Image*, int);
extern Image*	namedimage(Display*, char*);
extern int	nameimage(Image*, char*, int);
extern Image* allocimagemix(Display*, u32int, u32int);
extern int	drawsetlabel(char*);
extern int	scalesize(Display*, int);

/*
 * Colors
 */
extern	void	readcolmap(Display*, RGB*);
extern	void	writecolmap(Display*, RGB*);
extern	u32int	setalpha(u32int, uchar);

/*
 * Windows
 */
extern Screen*	allocscreen(Image*, Image*, int);
extern Image*	_allocwindow(Image*, Screen*, Rectangle, int, u32int);
extern Image*	allocwindow(Screen*, Rectangle, int, u32int);
extern void	bottomnwindows(Image**, int);
extern void	bottomwindow(Image*);
extern int	freescreen(Screen*);
extern Screen*	publicscreen(Display*, int, u32int);
extern void	topnwindows(Image**, int);
extern void	topwindow(Image*);
extern int	originwindow(Image*, Point, Point);

/*
 * Geometry
 */
extern Point		Pt(int, int);
extern Rectangle	Rect(int, int, int, int);
extern Rectangle	Rpt(Point, Point);
extern Point		addpt(Point, Point);
extern Point		subpt(Point, Point);
extern Point		divpt(Point, int);
extern Point		mulpt(Point, int);
extern int		eqpt(Point, Point);
extern int		eqrect(Rectangle, Rectangle);
extern Rectangle	insetrect(Rectangle, int);
extern Rectangle	rectaddpt(Rectangle, Point);
extern Rectangle	rectsubpt(Rectangle, Point);
extern Rectangle	canonrect(Rectangle);
extern int		rectXrect(Rectangle, Rectangle);
extern int		rectinrect(Rectangle, Rectangle);
extern void		combinerect(Rectangle*, Rectangle);
extern int		rectclip(Rectangle*, Rectangle);
extern int		ptinrect(Point, Rectangle);
extern void		replclipr(Image*, int, Rectangle);
extern int		drawreplxy(int, int, int);	/* used to be drawsetxy */
extern Point	drawrepl(Rectangle, Point);
extern int		rgb2cmap(int, int, int);
extern int		cmap2rgb(int);
extern int		cmap2rgba(int);
extern void		icossin(int, int*, int*);
extern void		icossin2(int, int, int*, int*);

/*
 * Graphics
 */
extern void	draw(Image*, Rectangle, Image*, Image*, Point);
extern void	drawop(Image*, Rectangle, Image*, Image*, Point, Drawop);
extern void	gendraw(Image*, Rectangle, Image*, Point, Image*, Point);
extern void	gendrawop(Image*, Rectangle, Image*, Point, Image*, Point, Drawop);
extern void	line(Image*, Point, Point, int, int, int, Image*, Point);
extern void	lineop(Image*, Point, Point, int, int, int, Image*, Point, Drawop);
extern void	poly(Image*, Point*, int, int, int, int, Image*, Point);
extern void	polyop(Image*, Point*, int, int, int, int, Image*, Point, Drawop);
extern void	fillpoly(Image*, Point*, int, int, Image*, Point);
extern void	fillpolyop(Image*, Point*, int, int, Image*, Point, Drawop);
extern Point	string(Image*, Point, Image*, Point, Font*, char*);
extern Point	stringop(Image*, Point, Image*, Point, Font*, char*, Drawop);
extern Point	stringn(Image*, Point, Image*, Point, Font*, char*, int);
extern Point	stringnop(Image*, Point, Image*, Point, Font*, char*, int, Drawop);
extern Point	runestring(Image*, Point, Image*, Point, Font*, Rune*);
extern Point	runestringop(Image*, Point, Image*, Point, Font*, Rune*, Drawop);
extern Point	runestringn(Image*, Point, Image*, Point, Font*, Rune*, int);
extern Point	runestringnop(Image*, Point, Image*, Point, Font*, Rune*, int, Drawop);
extern Point	stringbg(Image*, Point, Image*, Point, Font*, char*, Image*, Point);
extern Point	stringbgop(Image*, Point, Image*, Point, Font*, char*, Image*, Point, Drawop);
extern Point	stringnbg(Image*, Point, Image*, Point, Font*, char*, int, Image*, Point);
extern Point	stringnbgop(Image*, Point, Image*, Point, Font*, char*, int, Image*, Point, Drawop);
extern Point	runestringbg(Image*, Point, Image*, Point, Font*, Rune*, Image*, Point);
extern Point	runestringbgop(Image*, Point, Image*, Point, Font*, Rune*, Image*, Point, Drawop);
extern Point	runestringnbg(Image*, Point, Image*, Point, Font*, Rune*, int, Image*, Point);
extern Point	runestringnbgop(Image*, Point, Image*, Point, Font*, Rune*, int, Image*, Point, Drawop);
extern Point	_string(Image*, Point, Image*, Point, Font*, char*, Rune*, int, Rectangle, Image*, Point, Drawop);
extern Point	stringsubfont(Image*, Point, Image*, Subfont*, char*);
extern int		bezier(Image*, Point, Point, Point, Point, int, int, int, Image*, Point);
extern int		bezierop(Image*, Point, Point, Point, Point, int, int, int, Image*, Point, Drawop);
extern int		bezspline(Image*, Point*, int, int, int, int, Image*, Point);
extern int		bezsplineop(Image*, Point*, int, int, int, int, Image*, Point, Drawop);
extern int		bezsplinepts(Point*, int, Point**);
extern int		fillbezier(Image*, Point, Point, Point, Point, int, Image*, Point);
extern int		fillbezierop(Image*, Point, Point, Point, Point, int, Image*, Point, Drawop);
extern int		fillbezspline(Image*, Point*, int, int, Image*, Point);
extern int		fillbezsplineop(Image*, Point*, int, int, Image*, Point, Drawop);
extern void	ellipse(Image*, Point, int, int, int, Image*, Point);
extern void	ellipseop(Image*, Point, int, int, int, Image*, Point, Drawop);
extern void	fillellipse(Image*, Point, int, int, Image*, Point);
extern void	fillellipseop(Image*, Point, int, int, Image*, Point, Drawop);
extern void	arc(Image*, Point, int, int, int, Image*, Point, int, int);
extern void	arcop(Image*, Point, int, int, int, Image*, Point, int, int, Drawop);
extern void	fillarc(Image*, Point, int, int, Image*, Point, int, int);
extern void	fillarcop(Image*, Point, int, int, Image*, Point, int, int, Drawop);
extern void	border(Image*, Rectangle, int, Image*, Point);
extern void	borderop(Image*, Rectangle, int, Image*, Point, Drawop);

/*
 * Font management
 */
extern Font*	openfont(Display*, char*);
extern int	parsefontscale(char*, char**);
extern Font*	buildfont(Display*, char*, char*);
extern void	freefont(Font*);
extern Font*	mkfont(Subfont*, Rune);
extern int	cachechars(Font*, char**, Rune**, ushort*, int, int*, char**);
extern void	agefont(Font*);
extern Subfont*	allocsubfont(char*, int, int, int, Fontchar*, Image*);
extern Subfont*	lookupsubfont(Display*, char*);
extern void	installsubfont(char*, Subfont*);
extern void	uninstallsubfont(Subfont*);
extern void	freesubfont(Subfont*);
extern Subfont*	readsubfont(Display*, char*, int, int);
extern Subfont*	readsubfonti(Display*, char*, int, Image*, int);
extern int	writesubfont(int, Subfont*);
extern void	_unpackinfo(Fontchar*, uchar*, int);
extern Point	stringsize(Font*, char*);
extern int	stringwidth(Font*, char*);
extern int	stringnwidth(Font*, char*, int);
extern Point	runestringsize(Font*, Rune*);
extern int	runestringwidth(Font*, Rune*);
extern int	runestringnwidth(Font*, Rune*, int);
extern Point	strsubfontwidth(Subfont*, char*);
extern int	loadchar(Font*, Rune, Cacheinfo*, int, int, char**);
extern char*	subfontname(char*, char*, int);
extern Subfont*	_getsubfont(Display*, char*);
extern void		lockdisplay(Display*);
extern void	unlockdisplay(Display*);
extern int		drawlsetrefresh(u32int, int, void*, void*);
extern void	loadhidpi(Font*);
extern void	swapfont(Font*, Font**, Font**);

/*
 * Predefined 
 */
extern	Point		ZP;
extern	Rectangle	ZR;

/*
 * Set up by initdraw()
 */
extern	Display	*display;
extern	Font		*font;
extern	Image	*screen;
extern	Screen	*_screen;
extern	int	drawmousemask; /* set bits to disable receiving those buttons */
extern	int	_cursorfd;
extern	int	_drawdebug;	/* set to 1 to see errors from flushimage */
extern	void	_setdrawop(Display*, Drawop);
extern	Display	*_initdisplay(void(*)(Display*,char*), char*);

extern	void	needdisplay(void); /* call instead of initdraw to get (null) variable linked in */

#define	BGSHORT(p)		(((p)[0]<<0) | ((p)[1]<<8))
#define	BGLONG(p)		((BGSHORT(p)<<0) | (BGSHORT(p+2)<<16))
#define	BPSHORT(p, v)		((p)[0]=(v), (p)[1]=((v)>>8))
#define	BPLONG(p, v)		(BPSHORT(p, (v)), BPSHORT(p+2, (v)>>16))

/*
 * Compressed image file parameters and helper routines
 */
#define	NMATCH	3		/* shortest match possible */
#define	NRUN	(NMATCH+31)	/* longest match possible */
#define	NMEM	1024		/* window size */
#define	NDUMP	128		/* maximum length of dump */
#define	NCBLOCK	6000		/* size of compressed blocks */
extern	void	_twiddlecompressed(uchar*, int);
extern	int	_compblocksize(Rectangle, int);

/* XXX backwards helps; should go */
extern	u32int	drawld2chan[];
extern	void		drawsetdebug(int);

/*
 * Snarf buffer
 */
enum
{
	SnarfSize = 64*1024
};
char *getsnarf(void);
void putsnarf(char*);

void drawtopwindow(void);
void drawresizewindow(Rectangle);
extern char *winsize;

int	mousescrollsize(int);

/*
 * RPC interface to draw server.
 */
struct Mouse;
struct Cursor;
struct Cursor2;
int		_displaybouncemouse(Display *d, struct Mouse *m);
int		_displayconnect(Display *d);
int		_displaycursor(Display *d, struct Cursor *c, struct Cursor2 *c2);
int		_displayinit(Display *d, char *label, char *winsize);
int		_displaylabel(Display *d, char *label);
int		_displaymoveto(Display *d, Point p);
int		_displaymux(Display *d);
int		_displayrddraw(Display *d, void *v, int n);
int		_displayrdkbd(Display *d, Rune *r);
int		_displayrdmouse(Display *d, struct Mouse *m, int *resized);
char*		_displayrdsnarf(Display *d);
int		_displaywrdraw(Display *d, void *v, int n);
int		_displaywrsnarf(Display *d, char *snarf);
int		_displaytop(Display *d);
int		_displayresize(Display *d, Rectangle rect);

#if defined(__cplusplus)
}
#endif
#endif
