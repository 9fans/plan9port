/*
 * Some of the stuff in this file is not X-dependent and should be elsewhere.
 */
#include "x11-inc.h"
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include "x11-memdraw.h"

static Memimage	*xattach(char*);
static void	plan9cmap(void);
static int	setupcmap(XWindow);
static int	xreplacescreenimage(void);
static XGC	xgc(XDrawable, int, int);
static Image	*getimage0(Display*);

Xprivate _x;

Display*
_initdisplay(void (*error)(Display*, char*), char *label)
{
	Display *d;
	Memimage *m;

	/*
	 * This rfork(RFNOTEG) isn't exactly right,
	 * but we need some way to signal window 
	 * closes.  Right now we post a hangup
	 * note to the note group, which kills a whole
	 * lot more than just the current program
	 * if we don't do this.
	 */
	rfork(RFNOTEG);
	memimageinit();

	d = mallocz(sizeof(Display), 1);
	if(d == nil)
		return nil;

	d->buf = malloc(16000+5);
	d->obuf = malloc(16000);
	if(d->buf == nil || d->obuf == nil){
		free(d->buf);
		free(d->obuf);
		free(d);
		return nil;
	}
	d->bufsize = 16000;
	d->obufsize = 16000;
	d->bufp = d->buf;
	d->obufp = d->obuf;

	m = xattach(label);
	if(m == nil){
		free(d);
		return nil;
	}

	d->error = error;
	_initdisplaymemimage(d, m);
	d->screenimage = getimage0(d);
	return d;
}

static Image*
getimage0(Display *d)
{
	char *a, info[12*12+1];
	int n;
	Image *image;

	a = bufimage(d, 2);
	a[0] = 'J';
	a[1] = 'I';
	if(flushimage(d, 0) < 0){
		fprint(2, "cannot read screen info: %r\n");
		abort();
	}

	n = _drawmsgread(d, info, sizeof info);
	if(n != 12*12){
		fprint(2, "short screen info\n");
		abort();
	}

	image = mallocz(sizeof(Image), 1);
	image->display = d;
	image->id = 0;
	image->chan = strtochan(info+2*12);
	image->depth = chantodepth(image->chan);
	image->repl = atoi(info+3*12);
	image->r.min.x = atoi(info+4*12);
	image->r.min.y = atoi(info+5*12);
	image->r.max.x = atoi(info+6*12);
	image->r.max.y = atoi(info+7*12);
	image->clipr.min.x = atoi(info+8*12);
	image->clipr.min.y = atoi(info+9*12);
	image->clipr.max.x = atoi(info+10*12);
	image->clipr.max.y = atoi(info+11*12);
	return image;
}

int
getwindow(Display *d, int ref)
{
	Image *i;

	if(_x.destroyed)
		postnote(PNGROUP, getpgrp(), "hangup");
	if(xreplacescreenimage() == 0)
		return 0;
	freeimage(d->screenimage);
	i = getimage0(d);
	screen = d->screenimage = d->image = i;
	return 0;
}

static int
xerror(XDisplay *d, XErrorEvent *e)
{
	char buf[200];

	print("X error: error_code=%d, request_code=%d, minor=%d disp=%p\n",
		e->error_code, e->request_code, e->minor_code, d);
	XGetErrorText(d, e->error_code, buf, sizeof buf);
	print("%s\n", buf);
	return 0;
}

static int
xioerror(XDisplay *d)
{
	print("X I/O error\n");
	abort();
	return -1;
}


static Memimage*
xattach(char *label)
{
	char *argv[2], *disp;
	int i, n, xrootid;
	Rectangle r;
	XClassHint classhint;
	XDrawable pmid;
	XPixmapFormatValues *pfmt;
	XScreen *xscreen;
	XSetWindowAttributes attr;
	XSizeHints normalhint;
	XTextProperty name;
	XVisualInfo xvi;
	XWindow xrootwin;
	XWMHints hint;

	/*
	if(XInitThreads() == 0){
		fprint(2, "XInitThreads failed\n");
		abort();
	}
	*/

	/*
	 * Connect to X server.
	 */
	_x.display = XOpenDisplay(NULL);
	if(_x.display == nil){
		disp = getenv("DISPLAY");
		werrstr("XOpenDisplay %s: %r", disp ? disp : ":0");
		free(disp);
		return nil;
	}
	XSetErrorHandler(xerror);
	XSetIOErrorHandler(xioerror);
	xrootid = DefaultScreen(_x.display);
	xrootwin = DefaultRootWindow(_x.display);

	/* 
	 * Figure out underlying screen format.
	 */
	_x.depth = DefaultDepth(_x.display, xrootid);
	if(XMatchVisualInfo(_x.display, xrootid, 16, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 16, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 16;
		_x.usetable = 1;
	}
	else
	if(XMatchVisualInfo(_x.display, xrootid, 15, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 15, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 15;
		_x.usetable = 1;
	}
	else
	if(XMatchVisualInfo(_x.display, xrootid, 24, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 24, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 24;
		_x.usetable = 1;
	}
	else
	if(XMatchVisualInfo(_x.display, xrootid, 8, PseudoColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 8, StaticColor, &xvi)){
		if(_x.depth > 8){
			werrstr("can't deal with colormapped depth %d screens",
				_x.depth);
			goto err0;
		}
		_x.vis = xvi.visual;
		_x.depth = 8;
	}
	else{
		if(_x.depth != 8){
			werrstr("can't understand depth %d screen", _x.depth);
			goto err0;
		}
		_x.vis = DefaultVisual(_x.display, xrootid);
	}

	/*
	 * _x.depth is only the number of significant pixel bits,
	 * not the total number of pixel bits.  We need to walk the
	 * display list to find how many actual bits are used
	 * per pixel.
	 */
	_x.chan = 0;
	pfmt = XListPixmapFormats(_x.display, &n);
	for(i=0; i<n; i++){
		if(pfmt[i].depth == _x.depth){
			switch(pfmt[i].bits_per_pixel){
			case 1:	/* untested */
				_x.chan = GREY1;
				break;
			case 2:	/* untested */
				_x.chan = GREY2;
				break;
			case 4:	/* untested */
				_x.chan = GREY4;
				break;
			case 8:
				_x.chan = CMAP8;
				break;
			case 15:
				_x.chan = RGB15;
				break;
			case 16: /* how to tell RGB15? */
				_x.chan = RGB16;
				break;
			case 24: /* untested (impossible?) */
				_x.chan = RGB24;
				break;
			case 32:
				_x.chan = XRGB32;
				break;
			}
		}
	}
	if(_x.chan == 0){
		werrstr("could not determine screen pixel format");
		goto err0;
	}

	/*
	 * Set up color map if necessary.
	 */
	xscreen = DefaultScreenOfDisplay(_x.display);
	_x.cmap = DefaultColormapOfScreen(xscreen);
	if(_x.vis->class != StaticColor){
		plan9cmap();
		setupcmap(xrootwin);
	}

	/*
	 * We get to choose the initial rectangle size.
	 * This is arbitrary.  In theory we should read the
	 * command line and allow the traditional X options.
	 */
	r = Rect(0, 0, WidthOfScreen(xscreen)*3/4,
			HeightOfScreen(xscreen)*3/4);

	memset(&attr, 0, sizeof attr);
	attr.colormap = _x.cmap;
	attr.background_pixel = ~0;
	attr.border_pixel = 0;
	_x.drawable = XCreateWindow(
		_x.display,	/* display */
		xrootwin,	/* parent */
		0,		/* x */
		0,		/* y */
		Dx(r),		/* width */
	 	Dy(r),		/* height */
		0,		/* border width */
		DefaultDepthOfScreen(xscreen),	/* depth */
		InputOutput,	/* class */
		_x.vis,		/* visual */
				/* valuemask */
		CWBackPixel|CWBorderPixel|CWColormap,
		&attr		/* attributes (the above aren't?!) */
	);

	/*
	 * Label and other properties required by ICCCCM.
	 */
	memset(&name, 0, sizeof name);
	if(label == nil)
		label = "pjw-face-here";
	name.value = (uchar*)label;
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen(name.value);

	memset(&normalhint, 0, sizeof normalhint);
	normalhint.flags = USSize|PMaxSize;
	normalhint.max_width = WidthOfScreen(xscreen);
	normalhint.max_height = HeightOfScreen(xscreen);

	memset(&hint, 0, sizeof hint);
	hint.flags = InputHint|StateHint;
	hint.input = 1;
	hint.initial_state = NormalState;

	memset(&classhint, 0, sizeof classhint);
	classhint.res_name = label;
	classhint.res_class = label;

	argv[0] = label;
	argv[1] = nil;

	XSetWMProperties(
		_x.display,	/* display */
		_x.drawable,	/* window */
		&name,		/* XA_WM_NAME property */
		&name,		/* XA_WM_ICON_NAME property */
		argv,		/* XA_WM_COMMAND */
		1,		/* argc */
		&normalhint,	/* XA_WM_NORMAL_HINTS */
		&hint,		/* XA_WM_HINTS */
		&classhint	/* XA_WM_CLASSHINTS */
	);
	XFlush(_x.display);

	/*
	 * Allocate our local backing store.
	 */
	_x.screenr = r;
	_x.screenpm = XCreatePixmap(_x.display, _x.drawable, Dx(r), Dy(r), _x.depth);
	_x.nextscreenpm = _x.screenpm;
	_x.screenimage = xallocmemimage(r, _x.chan, _x.screenpm);

	/*
	 * Allocate some useful graphics contexts for the future.
	 */
	_x.gcfill	= xgc(_x.screenpm, FillSolid, -1);
	_x.gccopy	= xgc(_x.screenpm, -1, -1);
	_x.gcsimplesrc 	= xgc(_x.screenpm, FillStippled, -1);
	_x.gczero	= xgc(_x.screenpm, -1, -1);
	_x.gcreplsrc	= xgc(_x.screenpm, FillTiled, -1);

	pmid = XCreatePixmap(_x.display, _x.drawable, 1, 1, 1);
	_x.gcfill0	= xgc(pmid, FillSolid, 0);
	_x.gccopy0	= xgc(pmid, -1, -1);
	_x.gcsimplesrc0	= xgc(pmid, FillStippled, -1);
	_x.gczero0	= xgc(pmid, -1, -1);
	_x.gcreplsrc0	= xgc(pmid, FillTiled, -1);
	XFreePixmap(_x.display, pmid);

	/*
	 * Put the window on the screen.
	 */
	XMapWindow(_x.display, _x.drawable);
	XFlush(_x.display);

	/*
	 * Look up clipboard atom.
	 */
	_x.clipboard = XInternAtom(_x.display, "CLIPBOARD", False);

	/*
	 * Lots of display connections for various procs.
	 */
	_x.kbdcon	= XOpenDisplay(NULL);
	_x.mousecon	= XOpenDisplay(NULL);
	_x.snarfcon	= XOpenDisplay(NULL);

	if(0) fprint(2, "x: display=%p kbd=%p mouse=%p snarf=%p\n",
		_x.display, _x.kbdcon, _x.mousecon, _x.snarfcon);

	_x.black	= xscreen->black_pixel;
	_x.white	= xscreen->white_pixel;

	return _x.screenimage;

err0:
fprint(2, "%r\n");
	/*
	 * Should do a better job of cleaning up here.
	 */
	XCloseDisplay(_x.display);
	return nil;
}

int
drawsetlabel(Display *d, char *label)
{
	char *argv[2];
	XClassHint classhint;
	XTextProperty name;

	/*
	 * Label and other properties required by ICCCCM.
	 */
	memset(&name, 0, sizeof name);
	if(label == nil)
		label = "pjw-face-here";
	name.value = (uchar*)label;
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen(name.value);

	memset(&classhint, 0, sizeof classhint);
	classhint.res_name = label;
	classhint.res_class = label;

	argv[0] = label;
	argv[1] = nil;

	XSetWMProperties(
		_x.display,	/* display */
		_x.drawable,	/* window */
		&name,		/* XA_WM_NAME property */
		&name,		/* XA_WM_ICON_NAME property */
		argv,		/* XA_WM_COMMAND */
		1,		/* argc */
		nil,		/* XA_WM_NORMAL_HINTS */
		nil,		/* XA_WM_HINTS */
		&classhint	/* XA_WM_CLASSHINTS */
	);
	XFlush(_x.display);
	return 0;
}

/*
 * Create a GC with a particular fill style and XXX.
 * Disable generation of GraphicsExpose/NoExpose events in the GC.
 */
static XGC
xgc(XDrawable d, int fillstyle, int foreground)
{
	XGC gc;
	XGCValues v;

	memset(&v, 0, sizeof v);
	v.function = GXcopy;
	v.graphics_exposures = False;
	gc = XCreateGC(_x.display, d, GCFunction|GCGraphicsExposures, &v);
	if(fillstyle != -1)
		XSetFillStyle(_x.display, gc, fillstyle);
	if(foreground != -1)
		XSetForeground(_x.display, gc, 0);
	return gc;
}


/*
 * Initialize map with the Plan 9 rgbv color map.
 */
static void
plan9cmap(void)
{
	int r, g, b, cr, cg, cb, v, num, den, idx, v7, idx7;
	static int once;

	if(once)
		return;
	once = 1;

	for(r=0; r!=4; r++)
	for(g = 0; g != 4; g++)
	for(b = 0; b!=4; b++)
	for(v = 0; v!=4; v++){
		den=r;
		if(g > den)
			den=g;
		if(b > den)
			den=b;
		/* divide check -- pick grey shades */
		if(den==0)
			cr=cg=cb=v*17;
		else {
			num=17*(4*den+v);
			cr=r*num/den;
			cg=g*num/den;
			cb=b*num/den;
		}
		idx = r*64 + v*16 + ((g*4 + b + v - r) & 15);
		_x.map[idx].red = cr*0x0101;
		_x.map[idx].green = cg*0x0101;
		_x.map[idx].blue = cb*0x0101;
		_x.map[idx].pixel = idx;
		_x.map[idx].flags = DoRed|DoGreen|DoBlue;

		v7 = v >> 1;
		idx7 = r*32 + v7*16 + g*4 + b;
		if((v & 1) == v7){
			_x.map7to8[idx7][0] = idx;
			if(den == 0) { 		/* divide check -- pick grey shades */
				cr = ((255.0/7.0)*v7)+0.5;
				cg = cr;
				cb = cr;
			}
			else {
				num=17*15*(4*den+v7*2)/14;
				cr=r*num/den;
				cg=g*num/den;
				cb=b*num/den;
			}
			_x.map7[idx7].red = cr*0x0101;
			_x.map7[idx7].green = cg*0x0101;
			_x.map7[idx7].blue = cb*0x0101;
			_x.map7[idx7].pixel = idx7;
			_x.map7[idx7].flags = DoRed|DoGreen|DoBlue;
		}
		else
			_x.map7to8[idx7][1] = idx;
	}
}

/*
 * Initialize and install the rgbv color map as a private color map
 * for this application.  It gets the best colors when it has the
 * cursor focus.
 */
static int 
setupcmap(XWindow w)
{
	char buf[30];
	int i;
	u32int p, pp;
	XColor c;

	if(_x.depth <= 1)
		return 0;

	if(_x.depth >= 24) {
		/*
		 * The pixel value returned from XGetPixel needs to
		 * be converted to RGB so we can call rgb2cmap()
		 * to translate between 24 bit X and our color. Unfortunately,
		 * the return value appears to be display server endian 
		 * dependant. Therefore, we run some heuristics to later
		 * determine how to mask the int value correctly.
		 * Yeah, I know we can look at _x.vis->byte_order but 
		 * some displays say MSB even though they run on LSB.
		 * Besides, this is more anal.
		 */

		c = _x.map[19];	/* known to have different R, G, B values */
		if(!XAllocColor(_x.display, _x.cmap, &c)){
			werrstr("XAllocColor: %r");
			return -1;
		}
		p  = c.pixel;
		pp = rgb2cmap((p>>16)&0xff,(p>>8)&0xff,p&0xff);
		if(pp != _x.map[19].pixel) {
			/* check if endian is other way */
			pp = rgb2cmap(p&0xff,(p>>8)&0xff,(p>>16)&0xff);
			if(pp != _x.map[19].pixel){
				werrstr("cannot detect X server byte order");
				return -1;
			}

			switch(_x.chan){
			case RGB24:
				_x.chan = BGR24;
				break;
			case XRGB32:
				_x.chan = XBGR32;
				break;
			default:
				werrstr("cannot byteswap channel %s",
					chantostr(buf, _x.chan));
				break;
			}
		}
	}else if(_x.vis->class == TrueColor || _x.vis->class == DirectColor){
		/*
		 * Do nothing.  We have no way to express a
		 * mixed-endian 16-bit screen, so pretend they don't exist.
		 */
	}else if(_x.vis->class == PseudoColor){
		if(_x.usetable == 0){
			_x.cmap = XCreateColormap(_x.display, w, _x.vis, AllocAll); 
			XStoreColors(_x.display, _x.cmap, _x.map, 256);
			for(i = 0; i < 256; i++){
				_x.tox11[i] = i;
				_x.toplan9[i] = i;
			}
		}else{
			for(i = 0; i < 128; i++){
				c = _x.map7[i];
				if(!XAllocColor(_x.display, _x.cmap, &c)){
					werrstr("can't allocate colors in 7-bit map");
					return -1;
				}
				_x.tox11[_x.map7to8[i][0]] = c.pixel;
				_x.tox11[_x.map7to8[i][1]] = c.pixel;
				_x.toplan9[c.pixel] = _x.map7to8[i][0];
			}
		}
	}else{
		werrstr("unsupported visual class %d", _x.vis->class);
		return -1;
	}
	return 0;
}

void
flushmemscreen(Rectangle r)
{
	if(_x.nextscreenpm != _x.screenpm){
		qlock(&_x.screenlock);
		XSync(_x.display, False);
		XFreePixmap(_x.display, _x.screenpm);
		_x.screenpm = _x.nextscreenpm;
		qunlock(&_x.screenlock);
	}

	if(r.min.x >= r.max.x || r.min.y >= r.max.y)
		return;
	XCopyArea(_x.display, _x.screenpm, _x.drawable, _x.gccopy, r.min.x, r.min.y,
		Dx(r), Dy(r), r.min.x, r.min.y);
	XFlush(_x.display);
}

void
xexpose(XEvent *e, XDisplay *xd)
{
	XExposeEvent *xe;
	Rectangle r;

	qlock(&_x.screenlock);
	if(_x.screenpm != _x.nextscreenpm){
		qunlock(&_x.screenlock);
		return;
	}
	xe = (XExposeEvent*)e;
	r.min.x = xe->x;
	r.min.y = xe->y;
	r.max.x = xe->x+xe->width;
	r.max.y = xe->y+xe->height;
	XCopyArea(xd, _x.screenpm, _x.drawable, _x.gccopy, r.min.x, r.min.y,
		Dx(r), Dy(r), r.min.x, r.min.y);
	XSync(xd, False);
	qunlock(&_x.screenlock);
}

int
xdestroy(XEvent *e, XDisplay *xd)
{
	XDestroyWindowEvent *xe;

	xe = (XDestroyWindowEvent*)e;
	if(xe->window == _x.drawable){
		_x.destroyed = 1;
		return 1;
	}
	return 0;
}

int
xconfigure(XEvent *e, XDisplay *xd)
{
	Rectangle r;
	XConfigureEvent *xe = (XConfigureEvent*)e;

	if(xe->width == Dx(_x.screenr) && xe->height == Dy(_x.screenr))
		return 0;
	if(xe->width==0 || xe->height==0)
		fprint(2, "ignoring resize to %dx%d\n", xe->width, xe->height);
	r = Rect(0, 0, xe->width, xe->height);
	qlock(&_x.screenlock);
	if(_x.screenpm != _x.nextscreenpm){
		XCopyArea(xd, _x.screenpm, _x.drawable, _x.gccopy, r.min.x, r.min.y,
			Dx(r), Dy(r), r.min.x, r.min.y);
		XSync(xd, False);
	}
	qunlock(&_x.screenlock);
	_x.newscreenr = r;
	return 1;
}

static int
xreplacescreenimage(void)
{
	Memimage *m;
	XDrawable pixmap;
	Rectangle r;

	r = _x.newscreenr;
	if(eqrect(_x.screenr, r))
		return 0;

	pixmap = XCreatePixmap(_x.display, _x.drawable, Dx(r), Dy(r), _x.depth);
	m = xallocmemimage(r, _x.chan, pixmap);
	if(_x.nextscreenpm != _x.screenpm)
		XFreePixmap(_x.display, _x.nextscreenpm);
	_x.nextscreenpm = pixmap;
	_x.screenr = r;
	_drawreplacescreenimage(m);
	return 1;
}

