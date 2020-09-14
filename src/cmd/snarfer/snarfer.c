/*
 * This program is only intended for OS X, but the
 * ifdef __APPLE__ below lets us build it on all systems.
 * On non-OS X systems, you can use it to hold the snarf
 * buffer around after a program exits.
 */

#include <u.h>
#define Colormap	XColormap
#define Cursor		XCursor
#define Display		XDisplay
#define Drawable	XDrawable
#define Font		XFont
#define GC		XGC
#define Point		XPoint
#define Rectangle	XRectangle
#define Screen		XScreen
#define Visual		XVisual
#define Window		XWindow
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#undef Colormap
#undef Cursor
#undef Display
#undef Drawable
#undef Font
#undef GC
#undef Point
#undef Rectangle
#undef Screen
#undef Visual
#undef Window
AUTOLIB(X11);
#ifdef __APPLE__
#define APPLESNARF
#define Boolean AppleBoolean
#define Rect AppleRect
#define EventMask AppleEventMask
#define Point ApplePoint
#define Cursor AppleCursor
#include <Carbon/Carbon.h>
AUTOFRAMEWORK(Carbon)
#undef Boolean
#undef Rect
#undef EventMask
#undef Point
#undef Cursor
#endif
#include <libc.h>
#undef time
AUTOLIB(draw)	/* to cause link of X11 */

#include "../devdraw/x11-memdraw.h"        /* for MODIFY_SELECTION macro */
#include "../devdraw/x11-selection-mod.h"
Xprivate _x;

enum {
	SnarfSize = 65536
};
char snarf[3*SnarfSize+1];
Rune rsnarf[SnarfSize+1];

Xprivate _x;

void xselectionrequest(XEvent*);
char *xgetsnarf(void);
void appleputsnarf(void);
void xputsnarf(void);

int verbose;

#ifdef __APPLE__
PasteboardRef appleclip;
#endif

void
usage(void)
{
	fprint(2, "usage: snarfer [-v]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	XEvent xevent;

	ARGBEGIN{
	default:
		usage();
	case 'v':
		verbose = 1;
		break;
	}ARGEND

	if((_x.display = XOpenDisplay(nil)) == nil)
		sysfatal("XOpenDisplay: %r");
	_x.drawable = XCreateWindow(_x.display, DefaultRootWindow(_x.display),
		0, 0, 1, 1, 0, 0,
		InputOutput, DefaultVisual(_x.display, DefaultScreen(_x.display)),
		0, 0);
	if(_x.drawable == None)
		sysfatal("XCreateWindow: %r");
	XFlush(_x.display);

	_x.clipboard = XInternAtom(_x.display, "CLIPBOARD", False);
	_x.utf8string = XInternAtom(_x.display, "UTF8_STRING", False);
	_x.targets = XInternAtom(_x.display, "TARGETS", False);
	_x.text = XInternAtom(_x.display, "TEXT", False);
	_x.compoundtext = XInternAtom(_x.display, "COMPOUND_TEXT", False);

#ifdef __APPLE__
	if(PasteboardCreate(kPasteboardClipboard, &appleclip) != noErr)
		sysfatal("pasteboard create failed");
#endif

	xgetsnarf();
	appleputsnarf();
	xputsnarf();

	for(;;){
		XNextEvent(_x.display, &xevent);
		switch(xevent.type){
		case DestroyNotify:
			exits(0);
		case SelectionClear:
			xgetsnarf();
			appleputsnarf();
			xputsnarf();
			if(verbose)
				print("snarf{%s}\n", snarf);
			break;
		case SelectionRequest:
			xselectionrequest(&xevent);
			break;
		}
	}
}

void
xselectionrequest(XEvent *e)
{
	char *name;
	Atom a[4];
	XEvent r;
	XSelectionRequestEvent *xe;
	XDisplay *xd;

	xd = _x.display;

	memset(&r, 0, sizeof r);
	xe = (XSelectionRequestEvent*)e;
if(0) fprint(2, "xselect target=%d requestor=%d property=%d selection=%d\n",
	xe->target, xe->requestor, xe->property, xe->selection);
         MODIFY_SELECTION(name, xe){
		name = XGetAtomName(xd, xe->target);
		if(strcmp(name, "TIMESTAMP") != 0)
		        fprint(2, "%s: cannot handle selection request for '%s' (%d)\n",
		            argv0, name, (int)xe->target);
		r.xselection.property = None;
                  if (name) { XFree(name); name= 0x0; }
	}

	r.xselection.display = xe->display;
	/* r.xselection.property filled above */
	r.xselection.target = xe->target;
	r.xselection.type = SelectionNotify;
	r.xselection.requestor = xe->requestor;
	r.xselection.time = xe->time;
	r.xselection.send_event = True;
	r.xselection.selection = xe->selection;
	XSendEvent(xd, xe->requestor, False, 0, &r);
	XFlush(xd);
}

char*
xgetsnarf(void)
{
	uchar *data, *xdata;
	Atom clipboard, type, prop;
	ulong len, lastlen, dummy;
	int fmt, i;
	XWindow w;
	XDisplay *xd;

	xd = _x.display;

	w = None;
	clipboard = None;

	/*
	 * Is there a primary selection (highlighted text in an xterm)?
	 */
	if(0){
		clipboard = XA_PRIMARY;
		w = XGetSelectionOwner(xd, XA_PRIMARY);
		if(w == _x.drawable)
			return snarf;
	}

	/*
	 * If not, is there a clipboard selection?
	 */
	if(w == None && _x.clipboard != None){
		clipboard = _x.clipboard;
		w = XGetSelectionOwner(xd, _x.clipboard);
		if(w == _x.drawable)
			return snarf;
	}

	/*
	 * If not, give up.
	 */
	if(w == None)
		return nil;

	/*
	 * We should be waiting for SelectionNotify here, but it might never
	 * come, and we have no way to time out.  Instead, we will clear
	 * local property #1, request our buddy to fill it in for us, and poll
	 * until he's done or we get tired of waiting.
	 *
	 * We should try to go for _x.utf8string instead of XA_STRING,
	 * but that would add to the polling.
	 */
	prop = 1;
	XChangeProperty(xd, _x.drawable, prop, XA_STRING, 8, PropModeReplace, (uchar*)"", 0);
	XConvertSelection(xd, clipboard, XA_STRING, prop, _x.drawable, CurrentTime);
	XFlush(xd);
	lastlen = 0;
	for(i=0; i<10 || (lastlen!=0 && i<30); i++){
		sleep(100);
		XGetWindowProperty(xd, _x.drawable, prop, 0, 0, 0, AnyPropertyType,
			&type, &fmt, &dummy, &len, &data);
		if(lastlen == len && len > 0)
			break;
		lastlen = len;
	}
	if(i == 10)
		return nil;
	/* get the property */
	data = nil;
	XGetWindowProperty(xd, _x.drawable, prop, 0, SnarfSize/sizeof(ulong), 0,
		AnyPropertyType, &type, &fmt, &len, &dummy, &xdata);
	if(xdata == nil || (type != XA_STRING && type != _x.utf8string) || len == 0){
		if(xdata)
			XFree(xdata);
		return nil;
	}
	if(strlen((char*)xdata) >= SnarfSize){
		XFree(xdata);
		return nil;
	}
	strcpy(snarf, (char*)xdata);
	return snarf;
}

void
xputsnarf(void)
{
	if(0) XSetSelectionOwner(_x.display, XA_PRIMARY, _x.drawable, CurrentTime);
	if(_x.clipboard != None)
		XSetSelectionOwner(_x.display, _x.clipboard, _x.drawable, CurrentTime);
	XFlush(_x.display);
}

void
appleputsnarf(void)
{
#ifdef __APPLE__
	CFDataRef cfdata;
	PasteboardSyncFlags flags;

	runesnprint(rsnarf, nelem(rsnarf), "%s", snarf);
	if(PasteboardClear(appleclip) != noErr){
		fprint(2, "apple pasteboard clear failed\n");
		return;
	}
	flags = PasteboardSynchronize(appleclip);
	if((flags&kPasteboardModified) || !(flags&kPasteboardClientIsOwner)){
		fprint(2, "apple pasteboard cannot assert ownership\n");
		return;
	}
	cfdata = CFDataCreate(kCFAllocatorDefault,
		(uchar*)rsnarf, runestrlen(rsnarf)*2);
	if(cfdata == nil){
		fprint(2, "apple pasteboard cfdatacreate failed\n");
		return;
	}
	if(PasteboardPutItemFlavor(appleclip, (PasteboardItemID)1,
		CFSTR("public.utf16-plain-text"), cfdata, 0) != noErr){
		fprint(2, "apple pasteboard putitem failed\n");
		CFRelease(cfdata);
		return;
	}
	CFRelease(cfdata);
#endif
}
