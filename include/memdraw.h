#ifndef _MEMDRAW_H_
#define _MEMDRAW_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif

AUTOLIB(memdraw)
AUTOLIB(memlayer)

typedef struct	Memimage Memimage;
typedef struct	Memdata Memdata;
typedef struct	Memsubfont Memsubfont;
typedef struct	Memlayer Memlayer;
typedef struct	Memcmap Memcmap;
typedef struct	Memdrawparam	Memdrawparam;

/*
 * Memdata is allocated from main pool, but .data from the image pool.
 * Memdata is allocated separately to permit patching its pointer after
 * compaction when windows share the image data.
 * The first word of data is a back pointer to the Memdata, to find
 * The word to patch.
 */

struct Memdata
{
	u32int	*base;	/* allocated data pointer */
	uchar	*bdata;	/* pointer to first byte of actual data; word-aligned */
	int	ref;	/* number of Memimages using this data */
	void*	imref;
	int	allocd;	/* is this malloc'd? */
};

enum {
	Frepl	= 1<<0,	/* is replicated */
	Fsimple	= 1<<1,	/* is 1x1 */
	Fgrey	= 1<<2,	/* is grey */
	Falpha	= 1<<3,	/* has explicit alpha */
	Fcmap	= 1<<4,	/* has cmap channel */
	Fbytes	= 1<<5	/* has only 8-bit channels */
};

struct Memimage
{
	Rectangle	r;	/* rectangle in data area, local coords */
	Rectangle	clipr;	/* clipping region */
	int		depth;	/* number of bits of storage per pixel */
	int		nchan;	/* number of channels */
	u32int		chan;	/* channel descriptions */
	Memcmap		*cmap;

	Memdata		*data;	/* pointer to data; shared by windows in this image */
	int		zero;	/* data->bdata+zero==&byte containing (0,0) */
	u32int		width;	/* width in words of a single scan line */
	Memlayer	*layer;	/* nil if not a layer*/
	u32int		flags;
	void		*X;
	int		screenref;	/* reference count if this is a screen */

	int		shift[NChan];
	int		mask[NChan];
	int		nbits[NChan];
};

struct Memcmap
{
	uchar	cmap2rgb[3*256];
	uchar	rgb2cmap[16*16*16];
};

/*
 * Subfonts
 *
 * given char c, Subfont *f, Fontchar *i, and Point p, one says
 *	i = f->info+c;
 *	draw(b, Rect(p.x+i->left, p.y+i->top,
 *		p.x+i->left+((i+1)->x-i->x), p.y+i->bottom),
 *		color, f->bits, Pt(i->x, i->top));
 *	p.x += i->width;
 * to draw characters in the specified color (itself a Memimage) in Memimage b.
 */

struct	Memsubfont
{
	char		*name;
	short		n;		/* number of chars in font */
	uchar		height;		/* height of bitmap */
	char		ascent;		/* top of bitmap to baseline */
	Fontchar	*info;		/* n+1 character descriptors */
	Memimage	*bits;		/* of font */
};

/*
 * Encapsulated parameters and information for sub-draw routines.
 */
enum {
	Simplesrc=1<<0,
	Simplemask=1<<1,
	Replsrc=1<<2,
	Replmask=1<<3,
	Fullsrc=1<<4,
	Fullmask=1<<5
};
struct	Memdrawparam
{
	Memimage *dst;
	Rectangle	r;
	Memimage *src;
	Rectangle sr;
	Memimage *mask;
	Rectangle mr;
	int op;

	u32int state;
	u32int mval;	/* if Simplemask, the mask pixel in mask format */
	u32int mrgba;	/* mval in rgba */
	u32int sval;	/* if Simplesrc, the source pixel in src format */
	u32int srgba;	/* sval in rgba */
	u32int sdval;	/* sval in dst format */
};

/*
 * Memimage management
 */

extern Memimage*	allocmemimage(Rectangle, u32int);
extern Memimage*	allocmemimaged(Rectangle, u32int, Memdata*, void*);
extern Memimage*	readmemimage(int);
extern Memimage*	creadmemimage(int);
extern int		writememimage(int, Memimage*);
extern void		freememimage(Memimage*);
extern int		loadmemimage(Memimage*, Rectangle, uchar*, int);
extern int		cloadmemimage(Memimage*, Rectangle, uchar*, int);
extern int		unloadmemimage(Memimage*, Rectangle, uchar*, int);
extern u32int*		wordaddr(Memimage*, Point);
extern uchar*		byteaddr(Memimage*, Point);
extern int		drawclip(Memimage*, Rectangle*, Memimage*, Point*,
				Memimage*, Point*, Rectangle*, Rectangle*);
extern void		memfillcolor(Memimage*, u32int);
extern int		memsetchan(Memimage*, u32int);
extern u32int		pixelbits(Memimage*, Point);

/*
 * Graphics
 */
extern void	memdraw(Memimage*, Rectangle, Memimage*, Point, 
			Memimage*, Point, int);
extern void	memline(Memimage*, Point, Point, int, int, int,
			Memimage*, Point, int);
extern void	mempoly(Memimage*, Point*, int, int, int, int,
			Memimage*, Point, int);
extern void	memfillpoly(Memimage*, Point*, int, int,
			Memimage*, Point, int);
extern void	_memfillpolysc(Memimage*, Point*, int, int,
			Memimage*, Point, int, int, int, int);
extern void	memimagedraw(Memimage*, Rectangle, Memimage*, Point,
			Memimage*, Point, int);
extern int	hwdraw(Memdrawparam*);
extern void	memimageline(Memimage*, Point, Point, int, int, int,
			Memimage*, Point, int);
extern void	_memimageline(Memimage*, Point, Point, int, int, int,
			Memimage*, Point, Rectangle, int);
extern Point	memimagestring(Memimage*, Point, Memimage*, Point,
			Memsubfont*, char*);
extern void	memellipse(Memimage*, Point, int, int, int,
			Memimage*, Point, int);
extern void	memarc(Memimage*, Point, int, int, int, Memimage*,
			Point, int, int, int);
extern Rectangle memlinebbox(Point, Point, int, int, int);
extern int	memlineendsize(int);
extern void	_memmkcmap(void);
extern void	memimageinit(void);

/*
 * Subfont management
 */
extern Memsubfont*	allocmemsubfont(char*, int, int, int, Fontchar*, Memimage*);
extern Memsubfont*	openmemsubfont(char*);
extern void		freememsubfont(Memsubfont*);
extern Point		memsubfontwidth(Memsubfont*, char*);

/*
 * Predefined 
 */
extern	Memimage*	memwhite;
extern	Memimage*	memblack;
extern	Memimage*	memopaque;
extern	Memimage*	memtransparent;
extern	Memcmap*	memdefcmap;

/*
 * Kernel interface
 */
void			memimagemove(void*, void*);

/*
 * Kernel cruft
 */
extern void		rdb(void);
extern int		iprint(char*, ...);
extern int		drawdebug;

/*
 * For other implementations, like x11.
 */
extern void		_memfillcolor(Memimage*, u32int);
extern Memimage*	_allocmemimage(Rectangle, u32int);
extern int		_cloadmemimage(Memimage*, Rectangle, uchar*, int);
extern int		_loadmemimage(Memimage*, Rectangle, uchar*, int);
extern void		_freememimage(Memimage*);
extern u32int		_rgbatoimg(Memimage*, u32int);
extern u32int		_imgtorgba(Memimage*, u32int);
extern u32int		_pixelbits(Memimage*, Point);
extern int		_unloadmemimage(Memimage*, Rectangle, uchar*, int);
extern Memdrawparam*	_memimagedrawsetup(Memimage*,
				Rectangle, Memimage*, Point, Memimage*,
				Point, int);
extern void		_memimagedraw(Memdrawparam*);

#if defined(__cplusplus)
}
#endif
#endif
