#include <u.h>
#include "x11-inc.h"
#include "x11-keysym2ucs.h"
#include <errno.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <thread.h>
#include "x11-memdraw.h"
#include "devdraw.h"

#undef time

static void	plan9cmap(void);
static int	setupcmap(XWindow);
static XGC	xgc(XDrawable, int, int);
#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|KeyReleaseMask|EnterWindowMask|LeaveWindowMask|FocusChangeMask

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

Xprivate _x;

static void runxevent(XEvent *xev);
static int	_xconfigure(Xwin *w, XEvent *e);
static int	_xdestroy(Xwin *w, XEvent *e);
static void	_xexpose(Xwin *w, XEvent *e);
static int _xreplacescreenimage(Client *client);
static int _xtoplan9mouse(Xwin *w, XEvent *e, Mouse *m);
static void _xmovewindow(Xwin *w, Rectangle r);
static int _xtoplan9kbd(XEvent *e);
static int _xselect(XEvent *e);

static void	rpc_resizeimg(Client*);
static void	rpc_resizewindow(Client*, Rectangle);
static void	rpc_setcursor(Client*, Cursor*, Cursor2*);
static void	rpc_setlabel(Client*, char*);
static void	rpc_setmouse(Client*, Point);
static void	rpc_topwin(Client*);
static void	rpc_bouncemouse(Client*, Mouse);
static void	rpc_flush(Client*, Rectangle);

static ClientImpl x11impl = {
	rpc_resizeimg,
	rpc_resizewindow,
	rpc_setcursor,
	rpc_setlabel,
	rpc_setmouse,
	rpc_topwin,
	rpc_bouncemouse,
	rpc_flush
};

static Xwin*
newxwin(Client *c)
{
	Xwin *w;

	w = mallocz(sizeof *w, 1);
	if(w == nil)
		sysfatal("out of memory");
	w->client = c;
	w->next = _x.windows;
	_x.windows = w;
	c->impl = &x11impl;
	c->view = w;
	return w;
}

static Xwin*
findxwin(XDrawable d)
{
	Xwin *w, **l;

	for(l=&_x.windows; (w=*l) != nil; l=&w->next) {
		if(w->drawable == d) {
			/* move to front */
			*l = w->next;
			w->next = _x.windows;
			_x.windows = w;
			return w;
		}
	}
	return nil;
}

static int
xerror(XDisplay *d, XErrorEvent *e)
{
	char buf[200];

	if(e->request_code == 42) /* XSetInputFocus */
		return 0;
	if(e->request_code == 18) /* XChangeProperty */
		return 0;
	/*
	 * BadDrawable happens in apps that get resized a LOT,
	 * e.g. when KDE is configured to resize continuously
	 * during a window drag.
	 */
	if(e->error_code == 9) /* BadDrawable */
		return 0;

	fprint(2, "X error: error_code=%d, request_code=%d, minor=%d disp=%p\n",
		e->error_code, e->request_code, e->minor_code, d);
	XGetErrorText(d, e->error_code, buf, sizeof buf);
	fprint(2, "%s\n", buf);
	return 0;
}

static int
xioerror(XDisplay *d)
{
	/*print("X I/O error\n"); */
	exit(0);
	/*sysfatal("X I/O error\n");*/
	abort();
	return -1;
}

static void xloop(void);

static QLock xlk;

void
xlock(void)
{
	qlock(&xlk);
}

void
xunlock(void)
{
	qunlock(&xlk);
}

void
gfx_main(void)
{
	char *disp;
	int i, n, xrootid;
	XPixmapFormatValues *pfmt;
	XScreen *xscreen;
	XVisualInfo xvi;
	XWindow xrootwin;

	/*
	if(XInitThreads() == 0)
		sysfatal("XInitThread: %r");
	*/

	/*
	 * Connect to X server.
	 */
	_x.display = XOpenDisplay(NULL);
	if(_x.display == nil){
		disp = getenv("DISPLAY");
		werrstr("XOpenDisplay %s: %r", disp ? disp : ":0");
		free(disp);
		sysfatal("%r");
	}
	_x.fd = ConnectionNumber(_x.display);
	XSetErrorHandler(xerror);
	XSetIOErrorHandler(xioerror);
	xrootid = DefaultScreen(_x.display);
	xrootwin = DefaultRootWindow(_x.display);

	/*
	 * Figure out underlying screen format.
	 */
	if(XMatchVisualInfo(_x.display, xrootid, 24, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 24, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 24;
	}
	else
	if(XMatchVisualInfo(_x.display, xrootid, 16, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 16, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 16;
	}
	else
	if(XMatchVisualInfo(_x.display, xrootid, 15, TrueColor, &xvi)
	|| XMatchVisualInfo(_x.display, xrootid, 15, DirectColor, &xvi)){
		_x.vis = xvi.visual;
		_x.depth = 15;
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
		_x.depth = DefaultDepth(_x.display, xrootid);
		if(_x.depth != 8){
			werrstr("can't understand depth %d screen", _x.depth);
			goto err0;
		}
		_x.vis = DefaultVisual(_x.display, xrootid);
	}

	if(DefaultDepth(_x.display, xrootid) == _x.depth)
		_x.usetable = 1;

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
	XFree(pfmt);
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
	gfx_started();
	xloop();

err0:
	XCloseDisplay(_x.display);
	sysfatal("%r");
}

static void
xloop(void)
{
	fd_set rd, wr, xx;
	XEvent event;

	xlock();
	_x.fd = ConnectionNumber(_x.display);
	for(;;) {
		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&xx);
		FD_SET(_x.fd, &rd);
		FD_SET(_x.fd, &xx);
		if(_x.windows != nil)
			XSelectInput(_x.display, _x.windows->drawable, Mask); // TODO: when is this needed?
		XFlush(_x.display);
		xunlock();

	again:
		if(select(_x.fd+1, &rd, &wr, &xx, nil) < 0) {
			if(errno == EINTR)
				goto again;
			sysfatal("select: %r"); // TODO: quiet exit?
		}

		xlock();
		while(XPending(_x.display)) {
			XNextEvent(_x.display, &event);
			runxevent(&event);
		}
	}
}

/*
 * Handle an incoming X event.
 */
static void
runxevent(XEvent *xev)
{
	int c;
	KeySym k;
	static Mouse m;
	XButtonEvent *be;
	XKeyEvent *ke;
	Xwin *w;

#ifdef SHOWEVENT
	static int first = 1;
	if(first){
		dup(create("/tmp/devdraw.out", OWRITE, 0666), 1);
		setbuf(stdout, 0);
		first = 0;
	}
#endif

	if(xev == 0)
		return;

#ifdef SHOWEVENT
	print("\n");
	ShowEvent(xev);
#endif

	w = nil;
	switch(xev->type){
	case Expose:
		w = findxwin(((XExposeEvent*)xev)->window);
		break;
	case DestroyNotify:
		w = findxwin(((XDestroyWindowEvent*)xev)->window);
		break;
	case ConfigureNotify:
		w = findxwin(((XConfigureEvent*)xev)->window);
		break;
	case ButtonPress:
	case ButtonRelease:
		w = findxwin(((XButtonEvent*)xev)->window);
		break;
	case MotionNotify:
		w = findxwin(((XMotionEvent*)xev)->window);
		break;
	case KeyRelease:
	case KeyPress:
		w = findxwin(((XKeyEvent*)xev)->window);
		break;
	case FocusOut:
		w = findxwin(((XFocusChangeEvent*)xev)->window);
		break;
	}
	if(w == nil)
		w = _x.windows;

	switch(xev->type){
	case Expose:
		_xexpose(w, xev);
		break;

	case DestroyNotify:
		if(_xdestroy(w, xev))
			threadexitsall(nil);
		break;

	case ConfigureNotify:
		if(_xconfigure(w, xev))
			_xreplacescreenimage(w->client);
		break;

	case ButtonPress:
		be = (XButtonEvent*)xev;
		if(be->button == 1) {
			if(_x.kstate & ControlMask)
				be->button = 2;
			else if(_x.kstate & Mod1Mask)
				be->button = 3;
		}
		// fall through
	case ButtonRelease:
		_x.altdown = 0;
		// fall through
	case MotionNotify:
		if(_xtoplan9mouse(w, xev, &m) < 0)
			return;
		gfx_mousetrack(w->client, m.xy.x, m.xy.y, m.buttons|_x.kbuttons, m.msec);
		break;

	case KeyRelease:
	case KeyPress:
		ke = (XKeyEvent*)xev;
		XLookupString(ke, NULL, 0, &k, NULL);
		c = ke->state;
		switch(k) {
		case XK_Alt_L:
		case XK_Meta_L:	/* Shift Alt on PCs */
		case XK_Alt_R:
		case XK_Meta_R:	/* Shift Alt on PCs */
		case XK_Multi_key:
			if(xev->type == KeyPress)
				_x.altdown = 1;
			else if(_x.altdown) {
				_x.altdown = 0;
				gfx_keystroke(w->client, Kalt);
			}
			break;
		}

		switch(k) {
		case XK_Control_L:
			if(xev->type == KeyPress)
				c |= ControlMask;
			else
				c &= ~ControlMask;
			goto kbutton;
		case XK_Alt_L:
		case XK_Shift_L:
			if(xev->type == KeyPress)
				c |= Mod1Mask;
			else
				c &= ~Mod1Mask;
		kbutton:
			_x.kstate = c;
			if(m.buttons || _x.kbuttons) {
				_x.altdown = 0; // used alt
				_x.kbuttons = 0;
				if(c & ControlMask)
					_x.kbuttons |= 2;
				if(c & Mod1Mask)
					_x.kbuttons |= 4;
				gfx_mousetrack(w->client, m.xy.x, m.xy.y, m.buttons|_x.kbuttons, m.msec);
				break;
			}
		}

		if(xev->type != KeyPress)
			break;
		if(k == XK_F11){
			w->fullscreen = !w->fullscreen;
			_xmovewindow(w, w->fullscreen ? w->screenrect : w->windowrect);
			return;
		}
		if((c = _xtoplan9kbd(xev)) < 0)
			return;
		gfx_keystroke(w->client, c);
		break;

	case FocusOut:
		/*
		 * Some key combinations (e.g. Alt-Tab) can cause us
		 * to see the key down event without the key up event,
		 * so clear out the keyboard state when we lose the focus.
		 */
		_x.kstate = 0;
		_x.altdown = 0;
		gfx_abortcompose(w->client);
		break;

	case SelectionRequest:
		_xselect(xev);
		break;
	}
}


static Memimage*
xattach(Client *client, char *label, char *winsize)
{
	char *argv[2];
	int havemin, height, mask, width, x, y;
	Rectangle r;
	XClassHint classhint;
	XDrawable pmid;
	XScreen *xscreen;
	XSetWindowAttributes attr;
	XSizeHints normalhint;
	XTextProperty name;
	XWindow xrootwin;
	XWindowAttributes wattr;
	XWMHints hint;
	Atom atoms[2];
	Xwin *w;

	USED(client);
	xscreen = DefaultScreenOfDisplay(_x.display);
	xrootwin = DefaultRootWindow(_x.display);

	/*
	 * We get to choose the initial rectangle size.
	 * This is arbitrary.  In theory we should read the
	 * command line and allow the traditional X options.
	 */
	mask = 0;
	x = 0;
	y = 0;
	if(winsize && winsize[0]){
		if(parsewinsize(winsize, &r, &havemin) < 0)
			sysfatal("%r");
	}else{
		/*
		 * Parse the various X resources.  Thanks to Peter Canning.
		 */
		char *screen_resources, *display_resources, *geom,
			*geomrestype, *home, *file, *dpitype;
		XrmDatabase database;
		XrmValue geomres, dpires;

		database = XrmGetDatabase(_x.display);
		screen_resources = XScreenResourceString(xscreen);
		if(screen_resources != nil){
			XrmCombineDatabase(XrmGetStringDatabase(screen_resources), &database, False);
			XFree(screen_resources);
		}

		display_resources = XResourceManagerString(_x.display);
		if(display_resources == nil){
			home = getenv("HOME");
			if(home!=nil && (file=smprint("%s/.Xdefaults", home)) != nil){
				XrmCombineFileDatabase(file, &database, False);
				free(file);
			}
			free(home);
		}else
			XrmCombineDatabase(XrmGetStringDatabase(display_resources), &database, False);

		if (XrmGetResource(database, "Xft.dpi", "String", &dpitype, &dpires) == True) {
			if (dpires.addr) {
				client->displaydpi = atoi(dpires.addr);
			}
		}
		geom = smprint("%s.geometry", label);
		if(geom && XrmGetResource(database, geom, nil, &geomrestype, &geomres))
			mask = XParseGeometry(geomres.addr, &x, &y, (uint*)&width, (uint*)&height);
		XrmDestroyDatabase(database);
		free(geom);

		if((mask & WidthValue) && (mask & HeightValue)){
			r = Rect(0, 0, width, height);
		}else{
			r = Rect(0, 0, WidthOfScreen(xscreen)*3/4,
					HeightOfScreen(xscreen)*3/4);
			if(Dx(r) > Dy(r)*3/2)
				r.max.x = r.min.x + Dy(r)*3/2;
			if(Dy(r) > Dx(r)*3/2)
				r.max.y = r.min.y + Dx(r)*3/2;
		}
		if(mask & XNegative){
			x += WidthOfScreen(xscreen);
		}
		if(mask & YNegative){
			y += HeightOfScreen(xscreen);
		}
		havemin = 0;
	}
	w = newxwin(client);

	memset(&attr, 0, sizeof attr);
	attr.colormap = _x.cmap;
	attr.background_pixel = ~0;
	attr.border_pixel = 0;
	w->drawable = XCreateWindow(
		_x.display,	/* display */
		xrootwin,	/* parent */
		x,		/* x */
		y,		/* y */
		Dx(r),		/* width */
	 	Dy(r),		/* height */
		0,		/* border width */
		_x.depth,	/* depth */
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
	name.nitems = strlen((char*)name.value);

	memset(&normalhint, 0, sizeof normalhint);
	normalhint.flags = PSize|PMaxSize;
	if(winsize && winsize[0]){
		normalhint.flags &= ~PSize;
		normalhint.flags |= USSize;
		normalhint.width = Dx(r);
		normalhint.height = Dy(r);
	}else{
		if((mask & WidthValue) && (mask & HeightValue)){
			normalhint.flags &= ~PSize;
			normalhint.flags |= USSize;
			normalhint.width = width;
			normalhint.height = height;
		}
		if((mask & WidthValue) && (mask & HeightValue)){
			normalhint.flags |= USPosition;
			normalhint.x = x;
			normalhint.y = y;
		}
	}

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
		w->drawable,	/* window */
		&name,		/* XA_WM_NAME property */
		&name,		/* XA_WM_ICON_NAME property */
		argv,		/* XA_WM_COMMAND */
		1,		/* argc */
		&normalhint,	/* XA_WM_NORMAL_HINTS */
		&hint,		/* XA_WM_HINTS */
		&classhint	/* XA_WM_CLASSHINTS */
	);
	XFlush(_x.display);

	if(havemin){
		XWindowChanges ch;

		memset(&ch, 0, sizeof ch);
		ch.x = r.min.x;
		ch.y = r.min.y;
		XConfigureWindow(_x.display, w->drawable, CWX|CWY, &ch);
		/*
		 * Must pretend origin is 0,0 for X.
		 */
		r = Rect(0,0,Dx(r),Dy(r));
	}
	/*
	 * Look up clipboard atom.
	 */
	 if(_x.clipboard == 0) {
		_x.clipboard = XInternAtom(_x.display, "CLIPBOARD", False);
		_x.utf8string = XInternAtom(_x.display, "UTF8_STRING", False);
		_x.targets = XInternAtom(_x.display, "TARGETS", False);
		_x.text = XInternAtom(_x.display, "TEXT", False);
		_x.compoundtext = XInternAtom(_x.display, "COMPOUND_TEXT", False);
		_x.takefocus = XInternAtom(_x.display, "WM_TAKE_FOCUS", False);
		_x.losefocus = XInternAtom(_x.display, "_9WM_LOSE_FOCUS", False);
		_x.wmprotos = XInternAtom(_x.display, "WM_PROTOCOLS", False);
	}

	atoms[0] = _x.takefocus;
	atoms[1] = _x.losefocus;
	XChangeProperty(_x.display, w->drawable, _x.wmprotos, XA_ATOM, 32,
		PropModeReplace, (uchar*)atoms, 2);

	/*
	 * Put the window on the screen, check to see what size we actually got.
	 */
	XMapWindow(_x.display, w->drawable);
	XSync(_x.display, False);

	if(!XGetWindowAttributes(_x.display, w->drawable, &wattr))
		fprint(2, "XGetWindowAttributes failed\n");
	else if(wattr.width && wattr.height){
		if(wattr.width != Dx(r) || wattr.height != Dy(r)){
			r.max.x = wattr.width;
			r.max.y = wattr.height;
		}
	}else
		fprint(2, "XGetWindowAttributes: bad attrs\n");
	w->screenrect = Rect(0, 0, WidthOfScreen(xscreen), HeightOfScreen(xscreen));
	w->windowrect = r;

	/*
	 * Allocate our local backing store.
	 */
	w->screenr = r;
	w->screenpm = XCreatePixmap(_x.display, w->drawable, Dx(r), Dy(r), _x.depth);
	w->nextscreenpm = w->screenpm;
	w->screenimage = _xallocmemimage(r, _x.chan, w->screenpm);
	client->mouserect = r;

	/*
	 * Allocate some useful graphics contexts for the future.
	 * These can be used with any drawable matching w->drawable's
	 * pixel format (which is all the drawables we create).
	 */
	 if(_x.gcfill == 0) {
		_x.gcfill	= xgc(w->screenpm, FillSolid, -1);
		_x.gccopy	= xgc(w->screenpm, -1, -1);
		_x.gcsimplesrc 	= xgc(w->screenpm, FillStippled, -1);
		_x.gczero	= xgc(w->screenpm, -1, -1);
		_x.gcreplsrc	= xgc(w->screenpm, FillTiled, -1);

		pmid = XCreatePixmap(_x.display, w->drawable, 1, 1, 1);
		_x.gcfill0	= xgc(pmid, FillSolid, 0);
		_x.gccopy0	= xgc(pmid, -1, -1);
		_x.gcsimplesrc0	= xgc(pmid, FillStippled, -1);
		_x.gczero0	= xgc(pmid, -1, -1);
		_x.gcreplsrc0	= xgc(pmid, FillTiled, -1);
		XFreePixmap(_x.display, pmid);
	}

	return w->screenimage;
}

Memimage*
rpc_attach(Client *client, char *label, char *winsize)
{
	Memimage *m;

	xlock();
	m = xattach(client, label, winsize);
	xunlock();
	return m;
}

void
rpc_setlabel(Client *client, char *label)
{
	Xwin *w = (Xwin*)client->view;
	XTextProperty name;

	/*
	 * Label and other properties required by ICCCCM.
	 */
	xlock();
	memset(&name, 0, sizeof name);
	if(label == nil)
		label = "pjw-face-here";
	name.value = (uchar*)label;
	name.encoding = XA_STRING;
	name.format = 8;
	name.nitems = strlen((char*)name.value);

	XSetWMProperties(
		_x.display,	/* display */
		w->drawable,	/* window */
		&name,		/* XA_WM_NAME property */
		&name,		/* XA_WM_ICON_NAME property */
		nil,		/* XA_WM_COMMAND */
		0,		/* argc */
		nil,		/* XA_WM_NORMAL_HINTS */
		nil,		/* XA_WM_HINTS */
		nil	/* XA_WM_CLASSHINTS */
	);
	XFlush(_x.display);
	xunlock();
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
 *
 * We always choose the best depth possible, but that might not
 * be the default depth.  On such "suboptimal" systems, we have to allocate an
 * empty color map anyway, according to Axel Belinfante.
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
		if(_x.usetable == 0)
			_x.cmap = XCreateColormap(_x.display, w, _x.vis, AllocNone);

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
		if(_x.usetable == 0)
			_x.cmap = XCreateColormap(_x.display, w, _x.vis, AllocNone);
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
rpc_shutdown(void)
{
}

void
rpc_flush(Client *client, Rectangle r)
{
	Xwin *w = (Xwin*)client->view;

	xlock();
	if(w->nextscreenpm != w->screenpm){
		XSync(_x.display, False);
		XFreePixmap(_x.display, w->screenpm);
		w->screenpm = w->nextscreenpm;
	}

	if(r.min.x >= r.max.x || r.min.y >= r.max.y) {
		xunlock();
		return;
	}

	XCopyArea(_x.display, w->screenpm, w->drawable, _x.gccopy, r.min.x, r.min.y,
		Dx(r), Dy(r), r.min.x, r.min.y);
	XFlush(_x.display);
	xunlock();
}

static void
_xexpose(Xwin *w, XEvent *e)
{
	XExposeEvent *xe;
	Rectangle r;

	if(w->screenpm != w->nextscreenpm)
		return;
	xe = (XExposeEvent*)e;
	r.min.x = xe->x;
	r.min.y = xe->y;
	r.max.x = xe->x+xe->width;
	r.max.y = xe->y+xe->height;
	XCopyArea(_x.display, w->screenpm, w->drawable, _x.gccopy, r.min.x, r.min.y,
		Dx(r), Dy(r), r.min.x, r.min.y);
	XSync(_x.display, False);
}

static int
_xdestroy(Xwin *w, XEvent *e)
{
	XDestroyWindowEvent *xe;

	xe = (XDestroyWindowEvent*)e;
	if(xe->window == w->drawable){
		w->destroyed = 1;
		return 1;
	}
	return 0;
}

static int
_xconfigure(Xwin *w, XEvent *e)
{
	Rectangle r;
	XConfigureEvent *xe = (XConfigureEvent*)e;

	if(!w->fullscreen){
		int rx, ry;
		XWindow xw;
		if(XTranslateCoordinates(_x.display, w->drawable, DefaultRootWindow(_x.display), 0, 0, &rx, &ry, &xw))
			w->windowrect = Rect(rx, ry, rx+xe->width, ry+xe->height);
	}

	if(xe->width == Dx(w->screenr) && xe->height == Dy(w->screenr))
		return 0;
	r = Rect(0, 0, xe->width, xe->height);

	if(w->screenpm != w->nextscreenpm){
		XCopyArea(_x.display, w->screenpm, w->drawable, _x.gccopy, r.min.x, r.min.y,
			Dx(r), Dy(r), r.min.x, r.min.y);
		XSync(_x.display, False);
	}
	w->newscreenr = r;
	return 1;
}

static int
_xreplacescreenimage(Client *client)
{
	Memimage *m;
	XDrawable pixmap;
	Rectangle r;
	Xwin *w;

	w = (Xwin*)client->view;
	r = w->newscreenr;
	pixmap = XCreatePixmap(_x.display, w->drawable, Dx(r), Dy(r), _x.depth);
	m = _xallocmemimage(r, _x.chan, pixmap);
	if(w->nextscreenpm != w->screenpm)
		XFreePixmap(_x.display, w->nextscreenpm);
	w->nextscreenpm = pixmap;
	w->screenr = r;
	client->mouserect = r;
	xunlock();
	gfx_replacescreenimage(client, m);
	xlock();
	return 1;
}

void
rpc_resizeimg(Client *client)
{
	xlock();
	_xreplacescreenimage(client);
	xunlock();
}

void
rpc_gfxdrawlock(void)
{
	xlock();
}

void
rpc_gfxdrawunlock(void)
{
	xunlock();
}
void
rpc_topwin(Client *client)
{
	Xwin *w = (Xwin*)client->view;

	xlock();
	XMapRaised(_x.display, w->drawable);
	XSetInputFocus(_x.display, w->drawable, RevertToPointerRoot,
		CurrentTime);
	XFlush(_x.display);
	xunlock();
}

void
rpc_resizewindow(Client *client, Rectangle r)
{
	Xwin *w = (Xwin*)client->view;
	XWindowChanges e;
	int value_mask;

	xlock();
	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(_x.display, w->drawable, value_mask, &e);
	XFlush(_x.display);
	xunlock();
}

static void
_xmovewindow(Xwin *w, Rectangle r)
{
	XWindowChanges e;
	int value_mask;

	memset(&e, 0, sizeof e);
	value_mask = CWX|CWY|CWWidth|CWHeight;
	e.x = r.min.x;
	e.y = r.min.y;
	e.width = Dx(r);
	e.height = Dy(r);
	XConfigureWindow(_x.display, w->drawable, value_mask, &e);
	XFlush(_x.display);
}

static int
_xtoplan9kbd(XEvent *e)
{
	KeySym k;

	if(e->xany.type != KeyPress)
		return -1;
	needstack(64*1024);	/* X has some *huge* buffers in openobject */
		/* and they're even bigger on SuSE */
	XLookupString((XKeyEvent*)e,NULL,0,&k,NULL);
	if(k == NoSymbol)
		return -1;

	if(k&0xFF00){
		switch(k){
		case XK_BackSpace:
		case XK_Tab:
		case XK_Escape:
		case XK_Delete:
		case XK_KP_0:
		case XK_KP_1:
		case XK_KP_2:
		case XK_KP_3:
		case XK_KP_4:
		case XK_KP_5:
		case XK_KP_6:
		case XK_KP_7:
		case XK_KP_8:
		case XK_KP_9:
		case XK_KP_Divide:
		case XK_KP_Multiply:
		case XK_KP_Subtract:
		case XK_KP_Add:
		case XK_KP_Decimal:
			k &= 0x7F;
			break;
		case XK_Linefeed:
			k = '\r';
			break;
		case XK_KP_Space:
			k = ' ';
			break;
		case XK_Home:
		case XK_KP_Home:
			k = Khome;
			break;
		case XK_Left:
		case XK_KP_Left:
			k = Kleft;
			break;
		case XK_Up:
		case XK_KP_Up:
			k = Kup;
			break;
		case XK_Down:
		case XK_KP_Down:
			k = Kdown;
			break;
		case XK_Right:
		case XK_KP_Right:
			k = Kright;
			break;
		case XK_Page_Down:
		case XK_KP_Page_Down:
			k = Kpgdown;
			break;
		case XK_End:
		case XK_KP_End:
			k = Kend;
			break;
		case XK_Page_Up:
		case XK_KP_Page_Up:
			k = Kpgup;
			break;
		case XK_Insert:
		case XK_KP_Insert:
			k = Kins;
			break;
		case XK_KP_Enter:
		case XK_Return:
			k = '\n';
			break;
		case XK_Alt_L:
		case XK_Meta_L:	/* Shift Alt on PCs */
		case XK_Alt_R:
		case XK_Meta_R:	/* Shift Alt on PCs */
		case XK_Multi_key:
			return -1;
		default:		/* not ISO-1 or tty control */
			if(k>0xff) {
				k = _p9keysym2ucs(k);
				if(k==-1) return -1;
			}
		}
	}

	/* Compensate for servers that call a minus a hyphen */
	if(k == XK_hyphen)
		k = XK_minus;
	/* Do control mapping ourselves if translator doesn't */
	if(e->xkey.state&ControlMask)
		k &= 0x9f;
	if(k == NoSymbol) {
		return -1;
	}

	return k+0;
}

static int
_xtoplan9mouse(Xwin *w, XEvent *e, Mouse *m)
{
	int s;
	XButtonEvent *be;
	XMotionEvent *me;

	if(_x.putsnarf != _x.assertsnarf){
		_x.assertsnarf = _x.putsnarf;
		XSetSelectionOwner(_x.display, XA_PRIMARY, w->drawable, CurrentTime);
		if(_x.clipboard != None)
			XSetSelectionOwner(_x.display, _x.clipboard, w->drawable, CurrentTime);
		XFlush(_x.display);
	}

	switch(e->type){
	case ButtonPress:
		be = (XButtonEvent*)e;

		/*
		 * Fake message, just sent to make us announce snarf.
		 * Apparently state and button are 16 and 8 bits on
		 * the wire, since they are truncated by the time they
		 * get to us.
		 */
		if(be->send_event
		&& (~be->state&0xFFFF)==0
		&& (~be->button&0xFF)==0)
			return -1;
		/* BUG? on mac need to inherit these from elsewhere? */
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s |= Button1Mask;
			break;
		case 2:
			s |= Button2Mask;
			break;
		case 3:
			s |= Button3Mask;
			break;
		case 4:
			s |= Button4Mask;
			break;
		case 5:
			s |= Button5Mask;
			break;
		}
		break;
	case ButtonRelease:
		be = (XButtonEvent*)e;
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s &= ~Button1Mask;
			break;
		case 2:
			s &= ~Button2Mask;
			break;
		case 3:
			s &= ~Button3Mask;
			break;
		case 4:
			s &= ~Button4Mask;
			break;
		case 5:
			s &= ~Button5Mask;
			break;
		}
		break;

	case MotionNotify:
		me = (XMotionEvent*)e;
		s = me->state;
		m->xy.x = me->x;
		m->xy.y = me->y;
		m->msec = me->time;
		return 0; // do not set buttons

	default:
		return -1;
	}

	m->buttons = 0;
	if(s & Button1Mask)
		m->buttons |= 1;
	if(s & Button2Mask)
		m->buttons |= 2;
	if(s & Button3Mask)
		m->buttons |= 4;
	if(s & Button4Mask)
		m->buttons |= 8;
	if(s & Button5Mask)
		m->buttons |= 16;
	return 0;
}

void
rpc_setmouse(Client *client, Point p)
{
	Xwin *w = (Xwin*)client->view;

	xlock();
	XWarpPointer(_x.display, None, w->drawable, 0, 0, 0, 0, p.x, p.y);
	XFlush(_x.display);
	xunlock();
}

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static void
xcursorarrow(Xwin *w)
{
	if(_x.cursor != 0){
		XFreeCursor(_x.display, _x.cursor);
		_x.cursor = 0;
	}
	XUndefineCursor(_x.display, w->drawable);
	XFlush(_x.display);
}


void
rpc_setcursor(Client *client, Cursor *c, Cursor2 *c2)
{
	Xwin *w = (Xwin*)client->view;
	XColor fg, bg;
	XCursor xc;
	Pixmap xsrc, xmask;
	int i;
	uchar src[2*16], mask[2*16];

	USED(c2);

	xlock();
	if(c == nil){
		xcursorarrow(w);
		xunlock();
		return;
	}
	for(i=0; i<2*16; i++){
		src[i] = revbyte(c->set[i]);
		mask[i] = revbyte(c->set[i] | c->clr[i]);
	}

	fg = _x.map[0];
	bg = _x.map[255];
	xsrc = XCreateBitmapFromData(_x.display, w->drawable, (char*)src, 16, 16);
	xmask = XCreateBitmapFromData(_x.display, w->drawable, (char*)mask, 16, 16);
	xc = XCreatePixmapCursor(_x.display, xsrc, xmask, &fg, &bg, -c->offset.x, -c->offset.y);
	if(xc != 0) {
		XDefineCursor(_x.display, w->drawable, xc);
		if(_x.cursor != 0)
			XFreeCursor(_x.display, _x.cursor);
		_x.cursor = xc;
	}
	XFreePixmap(_x.display, xsrc);
	XFreePixmap(_x.display, xmask);
	XFlush(_x.display);
	xunlock();
}

struct {
	QLock lk;
	char buf[SnarfSize];
#ifdef APPLESNARF
	Rune rbuf[SnarfSize];
	PasteboardRef apple;
#endif
} clip;

static uchar*
_xgetsnarffrom(Xwin *w, XWindow xw, Atom clipboard, Atom target, int timeout0, int timeout)
{
	Atom prop, type;
	ulong len, lastlen, dummy;
	int fmt, i;
	uchar *data, *xdata;

	/*
	 * We should be waiting for SelectionNotify here, but it might never
	 * come, and we have no way to time out.  Instead, we will clear
	 * local property #1, request our buddy to fill it in for us, and poll
	 * until he's done or we get tired of waiting.
	 */
	prop = 1;
	XChangeProperty(_x.display, w->drawable, prop, target, 8, PropModeReplace, (uchar*)"", 0);
	XConvertSelection(_x.display, clipboard, target, prop, w->drawable, CurrentTime);
	XFlush(_x.display);
	lastlen = 0;
	timeout0 = (timeout0 + 9)/10;
	timeout = (timeout + 9)/10;
	for(i=0; i<timeout0 || (lastlen!=0 && i<timeout); i++){
		usleep(10*1000);
		XGetWindowProperty(_x.display, w->drawable, prop, 0, 0, 0, AnyPropertyType,
			&type, &fmt, &dummy, &len, &xdata);
		if(lastlen == len && len > 0){
			XFree(xdata);
			break;
		}
		lastlen = len;
		XFree(xdata);
	}
	if(len == 0)
		return nil;

	/* get the property */
	xdata = nil;
	XGetWindowProperty(_x.display, w->drawable, prop, 0, SnarfSize/sizeof(ulong), 0,
		AnyPropertyType, &type, &fmt, &len, &dummy, &xdata);
	if((type != target && type != XA_STRING && type != _x.utf8string) || len == 0){
		if(xdata)
			XFree(xdata);
		return nil;
	}
	if(xdata){
		data = (uchar*)strdup((char*)xdata);
		XFree(xdata);
		return data;
	}
	return nil;
}

char*
rpc_getsnarf(void)
{
	uchar *data;
	Atom clipboard;
	XWindow xw;
	Xwin *w;

	qlock(&clip.lk);
	xlock();
	w = _x.windows;
	/*
	 * Have we snarfed recently and the X server hasn't caught up?
	 */
	if(_x.putsnarf != _x.assertsnarf)
		goto mine;

	/*
	 * Is there a primary selection (highlighted text in an xterm)?
	 */
	clipboard = XA_PRIMARY;
	xw = XGetSelectionOwner(_x.display, XA_PRIMARY);
	// TODO check more
	if(xw == w->drawable){
	mine:
		data = (uchar*)strdup(clip.buf);
		goto out;
	}

	/*
	 * If not, is there a clipboard selection?
	 */
	if(xw == None && _x.clipboard != None){
		clipboard = _x.clipboard;
		xw = XGetSelectionOwner(_x.display, _x.clipboard);
		if(xw == w->drawable)
			goto mine;
	}

	/*
	 * If not, give up.
	 */
	if(xw == None){
		data = nil;
		goto out;
	}

	if((data = _xgetsnarffrom(w, xw, clipboard, _x.utf8string, 10, 100)) == nil)
	if((data = _xgetsnarffrom(w, xw, clipboard, XA_STRING, 10, 100)) == nil){
		/* nothing left to do */
	}

out:
	xunlock();
	qunlock(&clip.lk);
	return (char*)data;
}

void
__xputsnarf(char *data)
{
	XButtonEvent e;
	Xwin *w;

	if(strlen(data) >= SnarfSize)
		return;
	qlock(&clip.lk);
	xlock();
	w = _x.windows;
	strcpy(clip.buf, data);
	/* leave note for mouse proc to assert selection ownership */
	_x.putsnarf++;

	/* send mouse a fake event so snarf is announced */
	memset(&e, 0, sizeof e);
	e.type = ButtonPress;
	e.window = w->drawable;
	e.state = ~0;
	e.button = ~0;
	XSendEvent(_x.display, w->drawable, True, ButtonPressMask, (XEvent*)&e);
	XFlush(_x.display);
	xunlock();
	qunlock(&clip.lk);
}

static int
_xselect(XEvent *e)
{
	char *name;
	XEvent r;
	XSelectionRequestEvent *xe;
	Atom a[4];

	memset(&r, 0, sizeof r);
	xe = (XSelectionRequestEvent*)e;
if(0) fprint(2, "xselect target=%d requestor=%d property=%d selection=%d (sizeof atom=%d)\n",
	xe->target, xe->requestor, xe->property, xe->selection, sizeof a[0]);
	r.xselection.property = xe->property;
	if(xe->target == _x.targets){
		a[0] = _x.utf8string;
		a[1] = XA_STRING;
		a[2] = _x.text;
		a[3] = _x.compoundtext;
		XChangeProperty(_x.display, xe->requestor, xe->property, XA_ATOM,
			32, PropModeReplace, (uchar*)a, nelem(a));
	}else if(xe->target == XA_STRING
	|| xe->target == _x.utf8string
	|| xe->target == _x.text
	|| xe->target == _x.compoundtext
	|| ((name = XGetAtomName(_x.display, xe->target)) && strcmp(name, "text/plain;charset=UTF-8") == 0)){
		/* text/plain;charset=UTF-8 seems nonstandard but is used by Synergy */
		/* if the target is STRING we're supposed to reply with Latin1 XXX */
		qlock(&clip.lk);
		XChangeProperty(_x.display, xe->requestor, xe->property, xe->target,
			8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf));
		qunlock(&clip.lk);
	}else{
		if(strcmp(name, "TIMESTAMP") != 0)
			fprint(2, "%s: cannot handle selection request for '%s' (%d)\n", argv0, name, (int)xe->target);
		r.xselection.property = None;
	}

	r.xselection.display = xe->display;
	/* r.xselection.property filled above */
	r.xselection.target = xe->target;
	r.xselection.type = SelectionNotify;
	r.xselection.requestor = xe->requestor;
	r.xselection.time = xe->time;
	r.xselection.send_event = True;
	r.xselection.selection = xe->selection;
	XSendEvent(_x.display, xe->requestor, False, 0, &r);
	XFlush(_x.display);
	return 0;
}

#ifdef APPLESNARF
char*
_applegetsnarf(void)
{
	char *s, *t;
	CFArrayRef flavors;
	CFDataRef data;
	CFIndex nflavor, ndata, j;
	CFStringRef type;
	ItemCount nitem;
	PasteboardItemID id;
	PasteboardSyncFlags flags;
	UInt32 i;

/*	fprint(2, "applegetsnarf\n"); */
	qlock(&clip.lk);
	if(clip.apple == nil){
		if(PasteboardCreate(kPasteboardClipboard, &clip.apple) != noErr){
			fprint(2, "apple pasteboard create failed\n");
			qunlock(&clip.lk);
			return nil;
		}
	}
	flags = PasteboardSynchronize(clip.apple);
	if(flags&kPasteboardClientIsOwner){
		s = strdup(clip.buf);
		qunlock(&clip.lk);
		return s;
	}
	if(PasteboardGetItemCount(clip.apple, &nitem) != noErr){
		fprint(2, "apple pasteboard get item count failed\n");
		qunlock(&clip.lk);
		return nil;
	}
	for(i=1; i<=nitem; i++){
		if(PasteboardGetItemIdentifier(clip.apple, i, &id) != noErr)
			continue;
		if(PasteboardCopyItemFlavors(clip.apple, id, &flavors) != noErr)
			continue;
		nflavor = CFArrayGetCount(flavors);
		for(j=0; j<nflavor; j++){
			type = (CFStringRef)CFArrayGetValueAtIndex(flavors, j);
			if(!UTTypeConformsTo(type, CFSTR("public.utf16-plain-text")))
				continue;
			if(PasteboardCopyItemFlavorData(clip.apple, id, type, &data) != noErr)
				continue;
			ndata = CFDataGetLength(data);
			qunlock(&clip.lk);
			s = smprint("%.*S", ndata/2, (Rune*)CFDataGetBytePtr(data));
			CFRelease(flavors);
			CFRelease(data);
			for(t=s; *t; t++)
				if(*t == '\r')
					*t = '\n';
			return s;
		}
		CFRelease(flavors);
	}
	qunlock(&clip.lk);
	return nil;
}

void
_appleputsnarf(char *s)
{
	CFDataRef cfdata;
	PasteboardSyncFlags flags;

/*	fprint(2, "appleputsnarf\n"); */

	if(strlen(s) >= SnarfSize)
		return;
	qlock(&clip.lk);
	strcpy(clip.buf, s);
	runesnprint(clip.rbuf, nelem(clip.rbuf), "%s", s);
	if(clip.apple == nil){
		if(PasteboardCreate(kPasteboardClipboard, &clip.apple) != noErr){
			fprint(2, "apple pasteboard create failed\n");
			qunlock(&clip.lk);
			return;
		}
	}
	if(PasteboardClear(clip.apple) != noErr){
		fprint(2, "apple pasteboard clear failed\n");
		qunlock(&clip.lk);
		return;
	}
	flags = PasteboardSynchronize(clip.apple);
	if((flags&kPasteboardModified) || !(flags&kPasteboardClientIsOwner)){
		fprint(2, "apple pasteboard cannot assert ownership\n");
		qunlock(&clip.lk);
		return;
	}
	cfdata = CFDataCreate(kCFAllocatorDefault,
		(uchar*)clip.rbuf, runestrlen(clip.rbuf)*2);
	if(cfdata == nil){
		fprint(2, "apple pasteboard cfdatacreate failed\n");
		qunlock(&clip.lk);
		return;
	}
	if(PasteboardPutItemFlavor(clip.apple, (PasteboardItemID)1,
		CFSTR("public.utf16-plain-text"), cfdata, 0) != noErr){
		fprint(2, "apple pasteboard putitem failed\n");
		CFRelease(cfdata);
		qunlock(&clip.lk);
		return;
	}
	/* CFRelease(cfdata); ??? */
	qunlock(&clip.lk);
}
#endif	/* APPLESNARF */

void
rpc_putsnarf(char *data)
{
#ifdef APPLESNARF
	_appleputsnarf(data);
#endif
	__xputsnarf(data);
}

/*
 * Send the mouse event back to the window manager.
 * So that 9term can tell rio to pop up its button3 menu.
 */
void
rpc_bouncemouse(Client *c, Mouse m)
{
	Xwin *w = (Xwin*)c->view;
	XButtonEvent e;
	XWindow dw;

	xlock();
	e.type = ButtonPress;
	e.state = 0;
	e.button = 0;
	if(m.buttons&1)
		e.button = 1;
	else if(m.buttons&2)
		e.button = 2;
	else if(m.buttons&4)
		e.button = 3;
	e.same_screen = 1;
	XTranslateCoordinates(_x.display, w->drawable,
		DefaultRootWindow(_x.display),
		m.xy.x, m.xy.y, &e.x_root, &e.y_root, &dw);
	e.root = DefaultRootWindow(_x.display);
	e.window = e.root;
	e.subwindow = None;
	e.x = e.x_root;
	e.y = e.y_root;
#undef time
	e.time = CurrentTime;
	XUngrabPointer(_x.display, m.msec);
	XSendEvent(_x.display, e.root, True, ButtonPressMask, (XEvent*)&e);
	XFlush(_x.display);
	xunlock();
}
