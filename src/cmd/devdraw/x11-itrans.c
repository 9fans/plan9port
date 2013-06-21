/* input event and data structure translation */

#include <u.h>
#include "x11-inc.h"
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
#include <draw.h>
#include <memdraw.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include "x11-memdraw.h"
#include "x11-keysym2ucs.h"
#undef time

static KeySym
__xtoplan9kbd(XEvent *e)
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

int alting;

void
abortcompose(void)
{
	alting = 0;
}

static Rune* sendrune(Rune);

extern int _latin1(Rune*, int);
static Rune*
xtoplan9latin1(XEvent *e)
{
	Rune r;

	r = __xtoplan9kbd(e);
	if(r == -1)
		return nil;
	return sendrune(r);
}

void
sendalt(void)
{
	sendrune(Kalt);
}

static Rune*
sendrune(Rune r)
{
	static Rune k[10];
	static int nk;
	int n;

	if(alting){
		/*
		 * Kludge for Mac's X11 3-button emulation.
		 * It treats Command+Button as button 3, but also
		 * ends up sending XK_Meta_L twice.
		 */
		if(r == Kalt){
			alting = 0;
			return nil;
		}
		k[nk++] = r;
		n = _latin1(k, nk);
		if(n > 0){
			alting = 0;
			k[0] = n;
			k[1] = 0;
			return k;
		}
		if(n == -1){
			alting = 0;
			k[nk] = 0;
			return k;
		}
		/* n < -1, need more input */
		return nil;
	}else if(r == Kalt){
		alting = 1;
		nk = 0;
		return nil;
	}else{
		k[0] = r;
		k[1] = 0;
		return k;
	}
}

int
_xtoplan9kbd(XEvent *e)
{
	static Rune *r;

	if(e == (XEvent*)-1){
		assert(r);
		r--;
		return 0;
	}
	if(e)
		r = xtoplan9latin1(e);
	if(r && *r)
		return *r++;
	return -1;
}

int
_xtoplan9mouse(XEvent *e, Mouse *m)
{
	int s;
	XButtonEvent *be;
	XMotionEvent *me;

	if(_x.putsnarf != _x.assertsnarf){
		_x.assertsnarf = _x.putsnarf;
		XSetSelectionOwner(_x.display, XA_PRIMARY, _x.drawable, CurrentTime);
		if(_x.clipboard != None)
			XSetSelectionOwner(_x.display, _x.clipboard, _x.drawable, CurrentTime);
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
_xmoveto(Point p)
{
	XWarpPointer(_x.display, None, _x.drawable, 0, 0, 0, 0, p.x, p.y);
	XFlush(_x.display);
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
xcursorarrow(void)
{
	if(_x.cursor != 0){
		XFreeCursor(_x.display, _x.cursor);
		_x.cursor = 0;
	}
	XUndefineCursor(_x.display, _x.drawable);
	XFlush(_x.display);
}


void
_xsetcursor(Cursor *c)
{
	XColor fg, bg;
	XCursor xc;
	Pixmap xsrc, xmask;
	int i;
	uchar src[2*16], mask[2*16];

	if(c == nil){
		xcursorarrow();
		return;
	}
	for(i=0; i<2*16; i++){
		src[i] = revbyte(c->set[i]);
		mask[i] = revbyte(c->set[i] | c->clr[i]);
	}

	fg = _x.map[0];
	bg = _x.map[255];
	xsrc = XCreateBitmapFromData(_x.display, _x.drawable, (char*)src, 16, 16);
	xmask = XCreateBitmapFromData(_x.display, _x.drawable, (char*)mask, 16, 16);
	xc = XCreatePixmapCursor(_x.display, xsrc, xmask, &fg, &bg, -c->offset.x, -c->offset.y);
	if(xc != 0) {
		XDefineCursor(_x.display, _x.drawable, xc);
		if(_x.cursor != 0)
			XFreeCursor(_x.display, _x.cursor);
		_x.cursor = xc;
	}
	XFreePixmap(_x.display, xsrc);
	XFreePixmap(_x.display, xmask);
	XFlush(_x.display);
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
_xgetsnarffrom(XWindow w, Atom clipboard, Atom target, int timeout0, int timeout)
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
	XChangeProperty(_x.display, _x.drawable, prop, target, 8, PropModeReplace, (uchar*)"", 0);
	XConvertSelection(_x.display, clipboard, target, prop, _x.drawable, CurrentTime);
	XFlush(_x.display);
	lastlen = 0;
	timeout0 = (timeout0 + 9)/10;
	timeout = (timeout + 9)/10;
	for(i=0; i<timeout0 || (lastlen!=0 && i<timeout); i++){
		usleep(10*1000);
		XGetWindowProperty(_x.display, _x.drawable, prop, 0, 0, 0, AnyPropertyType,
			&type, &fmt, &dummy, &len, &xdata);
		if(lastlen == len && len > 0)
			break;
		lastlen = len;
		XFree(xdata);
	}
	if(len == 0)
		return nil;

	/* get the property */
	xdata = nil;
	XGetWindowProperty(_x.display, _x.drawable, prop, 0, SnarfSize/sizeof(ulong), 0, 
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
_xgetsnarf(void)
{
	uchar *data;
	Atom clipboard;
	XWindow w;

	qlock(&clip.lk);
	/*
	 * Have we snarfed recently and the X server hasn't caught up?
	 */
	if(_x.putsnarf != _x.assertsnarf)
		goto mine;

	/*
	 * Is there a primary selection (highlighted text in an xterm)?
	 */
	clipboard = XA_PRIMARY;
	w = XGetSelectionOwner(_x.display, XA_PRIMARY);
	if(w == _x.drawable){
	mine:
		data = (uchar*)strdup(clip.buf);
		goto out;
	}

	/*
	 * If not, is there a clipboard selection?
	 */
	if(w == None && _x.clipboard != None){
		clipboard = _x.clipboard;
		w = XGetSelectionOwner(_x.display, _x.clipboard);
		if(w == _x.drawable)
			goto mine;
	}

	/*
	 * If not, give up.
	 */
	if(w == None){
		data = nil;
		goto out;
	}
		
	if((data = _xgetsnarffrom(w, clipboard, _x.utf8string, 10, 100)) == nil)
	if((data = _xgetsnarffrom(w, clipboard, XA_STRING, 10, 100)) == nil){
		/* nothing left to do */
	}

out:
	qunlock(&clip.lk);
	return (char*)data;
}

void
__xputsnarf(char *data)
{
	XButtonEvent e;

	if(strlen(data) >= SnarfSize)
		return;
	qlock(&clip.lk);
	strcpy(clip.buf, data);
	/* leave note for mouse proc to assert selection ownership */
	_x.putsnarf++;

	/* send mouse a fake event so snarf is announced */
	memset(&e, 0, sizeof e);
	e.type = ButtonPress;
	e.window = _x.drawable;
	e.state = ~0;
	e.button = ~0;
	XSendEvent(_x.display, _x.drawable, True, ButtonPressMask, (XEvent*)&e);
	XFlush(_x.display);
	qunlock(&clip.lk);
}

int
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
_xputsnarf(char *data)
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
_xbouncemouse(Mouse *m)
{
	XButtonEvent e;
	XWindow dw;

	e.type = ButtonPress;
	e.state = 0;
	e.button = 0;
	if(m->buttons&1)
		e.button = 1;
	else if(m->buttons&2)
		e.button = 2;
	else if(m->buttons&4)
		e.button = 3;
	e.same_screen = 1;
	XTranslateCoordinates(_x.display, _x.drawable,
		DefaultRootWindow(_x.display),
		m->xy.x, m->xy.y, &e.x_root, &e.y_root, &dw);
	e.root = DefaultRootWindow(_x.display);
	e.window = e.root;
	e.subwindow = None;
	e.x = e.x_root;
	e.y = e.y_root;
#undef time
	e.time = CurrentTime;
	XUngrabPointer(_x.display, m->msec);
	XSendEvent(_x.display, e.root, True, ButtonPressMask, (XEvent*)&e);
	XFlush(_x.display);
}
