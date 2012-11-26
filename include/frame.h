#ifndef _FRAME_H_
#define _FRAME_H_ 1
#if defined(__cplusplus)
extern "C" {
#endif

AUTOLIB(frame)

typedef struct Frbox Frbox;
typedef struct Frame Frame;

enum{
	BACK,
	HIGH,
	BORD,
	TEXT,
	HTEXT,
	NCOL
};

#define	FRTICKW	3
struct Frbox
{
	long		wid;		/* in pixels */
	long		nrune;		/* <0 ==> negate and treat as break char */
	uchar	*ptr;
	short	bc;	/* break char */
	short	minwid;
};

struct Frame
{
	Font		*font;		/* of chars in the frame */
	Display		*display;	/* on which frame appears */
	Image		*b;		/* on which frame appears */
	Image		*cols[NCOL];	/* text and background colors */
	Rectangle	r;		/* in which text appears */
	Rectangle	entire;		/* of full frame */
	void			(*scroll)(Frame*, int);	/* scroll function provided by application */
	Frbox		*box;
	ulong		p0, p1;		/* selection */
	ushort		nbox, nalloc;
	ushort		maxtab;		/* max size of tab, in pixels */
	ushort		nchars;		/* # runes in frame */
	ushort		nlines;		/* # lines with text */
	ushort		maxlines;	/* total # lines in frame */
	ushort		lastlinefull;	/* last line fills frame */
	ushort		modified;	/* changed since frselect() */
	Image		*tick;	/* typing tick */
	Image		*tickback;	/* saved image under tick */
	int			ticked;	/* flag: is tick onscreen? */
	int			noredraw;	/* don't draw on the screen */
	int			tickscale;	/* tick scaling factor */
};

ulong	frcharofpt(Frame*, Point);
Point	frptofchar(Frame*, ulong);
int	frdelete(Frame*, ulong, ulong);
void	frinsert(Frame*, Rune*, Rune*, ulong);
void	frselect(Frame*, Mousectl*);
void	frselectpaint(Frame*, Point, Point, Image*);
void	frdrawsel(Frame*, Point, ulong, ulong, int);
Point	frdrawsel0(Frame*, Point, ulong, ulong, Image*, Image*);
void	frinit(Frame*, Rectangle, Font*, Image*, Image**);
void	frsetrects(Frame*, Rectangle, Image*);
void	frclear(Frame*, int);
void	frredraw(Frame*);

uchar	*_frallocstr(Frame*, unsigned);
void	_frinsure(Frame*, int, unsigned);
Point	_frdraw(Frame*, Point);
void	_frgrowbox(Frame*, int);
void	_frfreebox(Frame*, int, int);
void	_frmergebox(Frame*, int);
void	_frdelbox(Frame*, int, int);
void	_frsplitbox(Frame*, int, int);
int	_frfindbox(Frame*, int, ulong, ulong);
void	_frclosebox(Frame*, int, int);
int	_frcanfit(Frame*, Point, Frbox*);
void	_frcklinewrap(Frame*, Point*, Frbox*);
void	_frcklinewrap0(Frame*, Point*, Frbox*);
void	_fradvance(Frame*, Point*, Frbox*);
int	_frnewwid(Frame*, Point, Frbox*);
int	_frnewwid0(Frame*, Point, Frbox*);
void	_frclean(Frame*, Point, int, int);
void	_frdrawtext(Frame*, Point, Image*, Image*);
void	_fraddbox(Frame*, int, int);
Point	_frptofcharptb(Frame*, ulong, Point, int);
Point	_frptofcharnb(Frame*, ulong, int);
int	_frstrlen(Frame*, int);
void	frtick(Frame*, Point, int);
void	frinittick(Frame*);

#define	NRUNE(b)	((b)->nrune<0? 1 : (b)->nrune)
#define	NBYTE(b)	strlen((char*)(b)->ptr)
#if defined(__cplusplus)
}
#endif
#endif
