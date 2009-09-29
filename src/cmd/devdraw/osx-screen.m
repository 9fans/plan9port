#define Point OSXPoint
#define Rect OSXRect
#define Cursor OSXCursor
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#undef Rect
#undef Point
#undef Cursor
#undef offsetof
#undef nil

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include "osx-screen.h"
#include "osx-keycodes.h"
#include "devdraw.h"
#include "glendapng.h"

AUTOFRAMEWORK(Cocoa)

#define panic sysfatal

extern Rectangle mouserect;

struct {
	char *label;
	char *winsize;
	QLock labellock;

	Rectangle fullscreenr;
	Rectangle screenr;
	Memimage *screenimage;
	int isfullscreen;
	ulong fullscreentime;
	
	Point xy;
	int buttons;
	int kbuttons;

	CGDataProviderRef provider;
	NSWindow *window;
	CGImageRef image;
	CGContextRef windowctx;

	int needflush;
	QLock flushlock;
	int active;
	int infullscreen;
	int kalting;		// last keystroke was Kalt
} osx;

enum
{
	WindowAttrs = NSClosableWindowMask |
		NSTitledWindowMask |
		NSMiniaturizableWindowMask |
		NSResizableWindowMask
};

static void screenproc(void*);
 void eresized(int);
 void fullscreen(int);
 void seticon(void);
static void activated(int);

enum
{
	CmdFullScreen = 1,
};

@interface P9View : NSView
{}
@end

@implementation P9View
- (BOOL)acceptsFirstResponder
{
	return YES;
}
@end

void screeninit(void);
void _flushmemscreen(Rectangle r);

Memimage*
attachscreen(char *label, char *winsize)
{
	if(label == nil)
		label = "gnot a label";
	osx.label = strdup(label);
	osx.winsize = winsize;
	if(osx.screenimage == nil){
		screeninit();
		if(osx.screenimage == nil)
			panic("cannot create OS X screen");
	}
	return osx.screenimage;
}

void
_screeninit(void)
{
	CGRect cgr;
	NSRect or;
	Rectangle r;
	int havemin;

	memimageinit();

	cgr = CGDisplayBounds(CGMainDisplayID());
	osx.fullscreenr = Rect(0, 0, cgr.size.width, cgr.size.height);
	
	// Create the window.
	r = Rect(0, 0, Dx(osx.fullscreenr)*2/3, Dy(osx.fullscreenr)*2/3);
	havemin = 0;
	if(osx.winsize && osx.winsize[0]){
		if(parsewinsize(osx.winsize, &r, &havemin) < 0)
			sysfatal("%r");
	}
	if(!havemin)
		r = rectaddpt(r, Pt((Dx(osx.fullscreenr)-Dx(r))/2, (Dy(osx.fullscreenr)-Dy(r))/2));
	or = NSMakeRect(r.min.x, r.min.y, r.max.x, r.max.y);
	osx.window = [[NSWindow alloc] initWithContentRect:or styleMask:WindowAttrs
		 backing:NSBackingStoreBuffered defer:NO screen:[NSScreen mainScreen]];
	[osx.window setDelegate:[NSApp delegate]];
	[osx.window setAcceptsMouseMovedEvents:YES];

	P9View *view = [[P9View alloc] initWithFrame:or];
	[osx.window setContentView:view];
	[view release];

	setlabel(osx.label);
	seticon();
	
	// Finally, put the window on the screen.
	eresized(0);
	[osx.window makeKeyAndOrderFront:nil];

	[NSCursor unhide];
}

static Rendez scr;
static QLock slock;

void
screeninit(void)
{
	scr.l = &slock;
	qlock(scr.l);
//	proccreate(screenproc, nil, 256*1024);
	screenproc(NULL);
	while(osx.window == nil)
		rsleep(&scr);
	qunlock(scr.l);
}

static void
screenproc(void *v)
{
	qlock(scr.l);
	_screeninit();
	rwakeup(&scr);
	qunlock(scr.l);
}

static ulong
msec(void)
{
	return nsec()/1000000;
}

//static void
void
mouseevent(NSEvent *event)
{
	int wheel;
	NSPoint op;
	
	op = [event locationInWindow];

	osx.xy = subpt(Pt(op.x, op.y), osx.screenr.min);
	wheel = 0;

	switch([event type]){
	case NSScrollWheel:;
		CGFloat delta = [event deltaY];
		if(delta > 0)
			wheel = 8;
		else
			wheel = 16;
		break;
	
	case NSLeftMouseDown:
	case NSRightMouseDown:
	case NSOtherMouseDown:
	case NSLeftMouseUp:
	case NSRightMouseUp:
	case NSOtherMouseUp:;
		NSInteger but;
		NSUInteger  mod;
		but = [event buttonNumber];
		mod = [event modifierFlags];
		
		// OS X swaps button 2 and 3
		but = (but & ~6) | ((but & 4)>>1) | ((but&2)<<1);
		but = mouseswap(but);
		
		// Apply keyboard modifiers and pretend it was a real mouse button.
		// (Modifiers typed while holding the button go into kbuttons,
		// but this one does not.)
		if(but == 1){
			if(mod & NSAlternateKeyMask) {
				// Take the ALT away from the keyboard handler.
				if(osx.kalting) {
					osx.kalting = 0;
					keystroke(Kalt);
				}
				but = 2;
			}
			else if(mod & NSCommandKeyMask)
				but = 4;
		}
		osx.buttons = but;
		break;

	case NSMouseMoved:
	case NSLeftMouseDragged:
	case NSRightMouseDragged:
	case NSOtherMouseDragged:
		break;
	
	default:
		return;
	}

	mousetrack(osx.xy.x, osx.xy.y, osx.buttons|osx.kbuttons|wheel, msec());
}

static int keycvt[] =
{
	[QZ_IBOOK_ENTER] '\n',
	[QZ_RETURN] '\n',
	[QZ_ESCAPE] 27,
	[QZ_BACKSPACE] '\b',
	[QZ_LALT] Kalt,
	[QZ_LCTRL] Kctl,
	[QZ_LSHIFT] Kshift,
	[QZ_F1] KF+1,
	[QZ_F2] KF+2,
	[QZ_F3] KF+3,
	[QZ_F4] KF+4,
	[QZ_F5] KF+5,
	[QZ_F6] KF+6,
	[QZ_F7] KF+7,
	[QZ_F8] KF+8,
	[QZ_F9] KF+9,
	[QZ_F10] KF+10,
	[QZ_F11] KF+11,
	[QZ_F12] KF+12,
	[QZ_INSERT] Kins,
	[QZ_DELETE] 0x7F,
	[QZ_HOME] Khome,
	[QZ_END] Kend,
	[QZ_KP_PLUS] '+',
	[QZ_KP_MINUS] '-',
	[QZ_TAB] '\t',
	[QZ_PAGEUP] Kpgup,
	[QZ_PAGEDOWN] Kpgdown,
	[QZ_UP] Kup,
	[QZ_DOWN] Kdown,
	[QZ_LEFT] Kleft,
	[QZ_RIGHT] Kright,
	[QZ_KP_MULTIPLY] '*',
	[QZ_KP_DIVIDE] '/',
	[QZ_KP_ENTER] '\n',
	[QZ_KP_PERIOD] '.',
	[QZ_KP0] '0',
	[QZ_KP1] '1',
	[QZ_KP2] '2',
	[QZ_KP3] '3',
	[QZ_KP4] '4',
	[QZ_KP5] '5',
	[QZ_KP6] '6',
	[QZ_KP7] '7',
	[QZ_KP8] '8',
	[QZ_KP9] '9',
};

//static void
void
kbdevent(NSEvent *event)
{
	char ch;
	UInt32 code;
	UInt32 mod;
	int k;

	ch = [[event characters] characterAtIndex:0];
	code = [event keyCode];
	mod = [event modifierFlags];

	switch([event type]){
	case NSKeyDown:
		osx.kalting = 0;
		if(mod == NSCommandKeyMask){
			if(ch == 'F' || ch == 'f'){
				if(osx.isfullscreen && msec() - osx.fullscreentime > 500)
					fullscreen(0);
				return;
			}
			
			// Pass most Cmd keys through as Kcmd + ch.
			// OS X interprets a few no matter what we do,
			// so it is useless to pass them through as keystrokes too.
			switch(ch) {
			case 'm':	// minimize window
			case 'h':	// hide window
			case 'H':	// hide others
			case 'q':	// quit
				return;
			}
			if(' ' <= ch && ch <= '~') {
				keystroke(Kcmd + ch);
				return;
			}
			return;
		}
		k = ch;
		if(code < nelem(keycvt) && keycvt[code])
			k = keycvt[code];
		if(k >= 0)
			keystroke(k);
		else{
			keystroke(ch);
		}
		break;

	case NSFlagsChanged:
		if(!osx.buttons && !osx.kbuttons){
			if(mod == NSAlternateKeyMask) {
				osx.kalting = 1;
				keystroke(Kalt);
			}
			break;
		}
		
		// If the mouse button is being held down, treat 
		// changes in the keyboard modifiers as changes
		// in the mouse buttons.
		osx.kbuttons = 0;
		if(mod & NSAlternateKeyMask)
			osx.kbuttons |= 2;
		if(mod & NSCommandKeyMask)
			osx.kbuttons |= 4;
		mousetrack(osx.xy.x, osx.xy.y, osx.buttons|osx.kbuttons, msec());
		break;
	}
	return;
}

//static void
void
eresized(int new)
{
	Memimage *m;
	NSRect or;
	ulong chan;
	Rectangle r;
	int bpl;
	CGDataProviderRef provider;
	CGImageRef image;
	CGColorSpaceRef cspace;

	or = [[osx.window contentView] bounds];
	r = Rect(or.origin.x, or.origin.y, or.size.width, or.size.height);
	if(Dx(r) == Dx(osx.screenr) && Dy(r) == Dy(osx.screenr)){
		// No need to make new image.
		osx.screenr = r;
		return;
	}

	chan = XBGR32;
	m = allocmemimage(Rect(0, 0, Dx(r), Dy(r)), chan);
	if(m == nil)
		panic("allocmemimage: %r");
	if(m->data == nil)
		panic("m->data == nil");
	bpl = bytesperline(r, 32);
	provider = CGDataProviderCreateWithData(0,
		m->data->bdata, Dy(r)*bpl, 0);
	//cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
	cspace = CGColorSpaceCreateDeviceRGB();
	image = CGImageCreate(Dx(r), Dy(r), 8, 32, bpl,
		cspace,
		kCGImageAlphaNoneSkipLast,
		provider, 0, 0, kCGRenderingIntentDefault);
	CGColorSpaceRelease(cspace);
	CGDataProviderRelease(provider);	// CGImageCreate did incref
	
	mouserect = m->r;
	if(new){
		mouseresized = 1;
		mousetrack(osx.xy.x, osx.xy.y, osx.buttons|osx.kbuttons, msec());
	}
//	termreplacescreenimage(m);
	_drawreplacescreenimage(m);	// frees old osx.screenimage if any
	if(osx.image)
		CGImageRelease(osx.image);
	osx.image = image;
	osx.screenimage = m;
	osx.screenr = r;
}

void
flushproc(void *v)
{
	for(;;){
		if(osx.needflush && osx.windowctx && canqlock(&osx.flushlock)){
			if(osx.windowctx){
				CGContextFlush(osx.windowctx);
				osx.needflush = 0;
			}
			qunlock(&osx.flushlock);
		}
		usleep(33333);
	}
}

void
_flushmemscreen(Rectangle r)
{
	CGRect cgr;
	CGImageRef subimg;

	qlock(&osx.flushlock);
	if(osx.windowctx == nil){
		osx.windowctx = [[osx.window graphicsContext] graphicsPort];
//		[osx.window flushWindow];
//		proccreate(flushproc, nil, 256*1024);
	}
	
	cgr.origin.x = r.min.x;
	cgr.origin.y = r.min.y;
	cgr.size.width = Dx(r);
	cgr.size.height = Dy(r);
	subimg = CGImageCreateWithImageInRect(osx.image, cgr);
	cgr.origin.y = Dy(osx.screenr) - r.max.y; // XXX how does this make any sense?
	CGContextDrawImage(osx.windowctx, cgr, subimg);
	osx.needflush = 1;
	qunlock(&osx.flushlock);
	CGImageRelease(subimg);
}

void
activated(int active)
{
	osx.active = active;
}

void
fullscreen(int wascmd)
{
	NSView *view = [osx.window contentView];

	if(osx.isfullscreen){
		[view exitFullScreenModeWithOptions:nil];
		osx.isfullscreen = 0;
	}else{
		[view enterFullScreenMode:[osx.window screen] withOptions:nil];
		osx.isfullscreen = 1;
		osx.fullscreentime = msec();
	}
	eresized(1);
}
		
void
setmouse(Point p)
{
	CGPoint cgp;
	
	cgp.x = p.x + osx.screenr.min.x;
	cgp.y = p.y + osx.screenr.min.y;
	CGWarpMouseCursorPosition(cgp);
}

void
setcursor(Cursor *c)
{
	NSImage *image;
	NSBitmapImageRep *bitmap;
	NSCursor *nsc;
	unsigned char *planes[5];
	int i;

	if(c == nil){
		[NSCursor pop];
		return;
	}
	
	image = [[NSImage alloc] initWithSize:NSMakeSize(16.0, 16.0)];
	bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
		pixelsWide:16
		pixelsHigh:16
		bitsPerSample:1
		samplesPerPixel:2
		hasAlpha:YES
		isPlanar:YES
		colorSpaceName:NSCalibratedWhiteColorSpace
		bytesPerRow:2
		bitsPerPixel:1];

	[bitmap getBitmapDataPlanes:planes];

	for(i=0; i<16; i++){
		planes[0][i] = ((ushort*)c->set)[i];
		planes[1][i] = planes[0][i] | ((ushort*)c->clr)[i];
	}

	[image addRepresentation:bitmap];

	nsc = [[NSCursor alloc] initWithImage:image
		hotSpot:NSMakePoint(c->offset.x, c->offset.y)];
	[nsc push];

	[image release];
	[bitmap release];
	[nsc release];
}

void
getcolor(ulong i, ulong *r, ulong *g, ulong *b)
{
	ulong v;
	
	v = 0;
	*r = (v>>16)&0xFF;
	*g = (v>>8)&0xFF;
	*b = v&0xFF;
}

int
setcolor(ulong i, ulong r, ulong g, ulong b)
{
	/* no-op */
	return 0;
}


int
hwdraw(Memdrawparam *p)
{
	return 0;
}

struct {
	QLock lk;
	char buf[SnarfSize];
	Rune rbuf[SnarfSize];
	NSPasteboard *apple;
} clip;

char*
getsnarf(void)
{
	char *s, *t;
	NSArray *types;
	NSString *string;
	NSData * data;
	NSUInteger ndata;

/*	fprint(2, "applegetsnarf\n"); */
	qlock(&clip.lk);

	clip.apple = [NSPasteboard generalPasteboard];
	types = [clip.apple types];

	string = [clip.apple stringForType:NSStringPboardType];
	if(string == nil){
		fprint(2, "apple pasteboard get item type failed\n");
		qunlock(&clip.lk);
		return nil;
	}

	data = [string dataUsingEncoding:NSUnicodeStringEncoding];
	if(data != nil){
		ndata = [data length];
		qunlock(&clip.lk);
		s = smprint("%.*S", ndata/2, (Rune*)[data bytes]);
		for(t=s; *t; t++)
			if(*t == '\r')
				*t = '\n';
		return s;
	}

	qunlock(&clip.lk);
	return nil;		
}

void
putsnarf(char *s)
{
	NSArray *pboardTypes;
	NSString *string;

/*	fprint(2, "appleputsnarf\n"); */

	if(strlen(s) >= SnarfSize)
		return;
	qlock(&clip.lk);
	strcpy(clip.buf, s);
	runesnprint(clip.rbuf, nelem(clip.rbuf), "%s", s);

	pboardTypes = [NSArray arrayWithObject:NSStringPboardType];

	clip.apple = [NSPasteboard generalPasteboard];
	[clip.apple declareTypes:pboardTypes owner:nil];

	assert(sizeof(clip.rbuf[0]) == 2);
	string = [NSString stringWithCharacters:clip.rbuf length:runestrlen(clip.rbuf)*2];
	if(string == nil){
		fprint(2, "apple pasteboard data create failed\n");
		qunlock(&clip.lk);
		return;
	}
	if(![clip.apple setString:string forType:NSStringPboardType]){
		fprint(2, "apple pasteboard putitem failed\n");
		qunlock(&clip.lk);
		return;
	}
	qunlock(&clip.lk);
}

void
setlabel(char *label)
{
	CFStringRef cs;
	cs = CFStringCreateWithBytes(nil, (uchar*)label, strlen(osx.label), kCFStringEncodingUTF8, false);
	[osx.window setTitle:(NSString*)cs];
	CFRelease(cs);
}

void
kicklabel(char *label)
{
	char *p;

	p = strdup(label);
	if(p == nil)
		return;
	qlock(&osx.labellock);
	free(osx.label);
	osx.label = p;
	qunlock(&osx.labellock);

	setlabel(label);	
}

// static void
void
seticon(void)
{
	NSImage *im;
	NSData *d;

	d = [[NSData alloc] initWithBytes:glenda_png length:(sizeof glenda_png)];
	im = [[NSImage alloc] initWithData:d];
	if(im){
		NSLog(@"here");
		[NSApp setApplicationIconImage:im];
		[[NSApp dockTile] setShowsApplicationBadge:YES];
		[[NSApp dockTile] display];
	}
	[d release];
	[im release];
}
