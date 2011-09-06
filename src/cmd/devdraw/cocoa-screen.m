/*
 * Cocoa's event loop must be in the main thread.
 */

#define Point OSXPoint
#define Rect OSXRect
#define Cursor OSXCursor

#import <Cocoa/Cocoa.h>

#undef Rect
#undef Point
#undef Cursor

#include <u.h>
#include <libc.h>
#include  "cocoa-thread.h"
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <cursor.h>
#include "cocoa-screen.h"
#include "osx-keycodes.h"
#include "devdraw.h"
#include "glendapng.h"

#define DEBUG if(0)NSLog

AUTOFRAMEWORK(Cocoa)

#define panic sysfatal

struct {
	NSWindow	*obj;
	NSString		*label;
	char			*winsize;
	int			ispositioned;

	NSImage		*img;
	Memimage	*imgbuf;
	NSSize		imgsize;

	QLock		lock;
	Rendez		meeting;
	NSRect		flushr;
	int			osxdrawing;
	int			p9pflushing;
	int			isresizing;
} win;

@interface appdelegate : NSObject
	+(void)callmakewin:(id)arg; @end
@interface appthreads : NSObject
	+(void)callservep9p:(id)arg; @end
@interface appview : NSView @end

int chatty;
int multitouch = 1;

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	/*
	 * Move the protocol off stdin/stdout so that
	 * any inadvertent prints don't screw things up.
	 */
	dup(0,3);
	dup(1,4);
	close(0);
	close(1);
	open("/dev/null", OREAD);
	open("/dev/null", OWRITE);

	ARGBEGIN{
	case 'D':
		chatty++;
		break;
	case 'M':
		multitouch = 0;
		break;
	default:
		usage();
	}ARGEND

	/*
	 * Ignore arguments.  They're only for good ps -a listings.
	 */


	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp setDelegate:[appdelegate new]];
	[NSApp activateIgnoringOtherApps:YES];
	[NSApp run];
}

static void eresized(int);
static void getmousepos(void);
static void makemenu(NSString*);
static void makewin();
static void seticon(NSString*);

@implementation appdelegate
- (void)applicationDidFinishLaunching:(id)arg
{
	[NSApplication detachDrawingThread:@selector(callservep9p:)
		toTarget:[appthreads class] withObject:nil];
}
+ (void)callmakewin:(id)arg
{
	makewin();
}
- (void)windowDidResize:(id)arg
{
	eresized(1);
}
- (void)windowDidBecomeKey:(id)arg
{
	getmousepos();
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(id)arg
{
	return YES;
}
@end

@implementation appthreads
+(void)callservep9p:(id)arg
{
	servep9p();
	[NSApp terminate:self];
}
@end

Memimage*
attachscreen(char *label, char *winsize)
{
	static int first = 1;

	if(! first--)
		panic("attachscreen called twice");

	if(label == nil)
		label = "gnot a label";

	win.label = [[NSString alloc] initWithUTF8String:label];
	win.meeting.l = &win.lock;
	win.winsize = strdup(winsize);

	makemenu(win.label);

//	make NSWindow in the main thread,
//	else no resize cursor when resizing.
	[appdelegate
		performSelectorOnMainThread:@selector(callmakewin:)
		withObject:nil
		waitUntilDone:YES];
//	makewin();

	seticon(win.label);

	eresized(0);

	return win.imgbuf;
}

void
makewin(id winsize)
{
	char *s;
	int style;
	NSWindow *w;
	NSRect r, sr;
	Rectangle wr;

	s = win.winsize;

	if(s && *s){
		if(parsewinsize(s, &wr, &win.ispositioned) < 0)
			sysfatal("%r");
	}else{
		sr = [[NSScreen mainScreen] frame];
		wr = Rect(0, 0, sr.size.width*2/3, sr.size.height*2/3);
	}
//	The origin is the left-bottom corner with Cocoa.
//	Does the following work with any rectangles?
	r = NSMakeRect(wr.min.x, r.size.height-wr.min.y, Dx(wr), Dy(wr));

	style = NSTitledWindowMask
		| NSClosableWindowMask
		| NSResizableWindowMask
		| NSMiniaturizableWindowMask;

	w = [[NSWindow alloc]
		initWithContentRect:r
		styleMask:style
		backing:NSBackingStoreBuffered
		defer:NO];

	[w setAcceptsMouseMovedEvents:YES];
#if OSX_VERSION >= 100700
	[w setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
#endif
	[w setContentView:[appview new]];
	[w setDelegate:[NSApp delegate]];
	[w setMinSize:NSMakeSize(128,128)];
	[[w contentView] setAcceptsTouchEvents:YES];

	if(win.ispositioned == 0)
		[w center];

	[w setTitle:win.label];
	[w makeKeyAndOrderFront:nil];

	win.obj = w;
}

static void sendmouse(int);

static void
eresized(int new)
{
	static int first = 1;
	uint ch;
	NSSize size;
	Rectangle r;
	Memimage *m;
	int bpl;

	if(first--)
		memimageinit();

	size = [[win.obj contentView] bounds].size;
DEBUG(@"eresized called new=%d, [%.0f %.0f] -> [%.0f %.0f]", new,
win.imgsize.width, win.imgsize.height, size.width, size.height);

	r = Rect(0, 0, size.width, size.height);
	ch = XBGR32;
	m = allocmemimage(r, ch);
	if(m == nil)
		panic("allocmemimage: %r");
	if(m->data == nil)
		panic("m->data == nil");

	bpl = bytesperline(r, 32);

	CGDataProviderRef dp;
	CGImageRef i;
	CGColorSpaceRef cs;

	dp = CGDataProviderCreateWithData(0, m->data->bdata, Dy(r)*bpl, 0);
	cs = CGColorSpaceCreateDeviceRGB();
	i = CGImageCreate(Dx(r), Dy(r), 8, 32, bpl,
		cs, kCGImageAlphaNone, dp, 0, 0, kCGRenderingIntentDefault);

	_drawreplacescreenimage(m);
	if(win.img)
		[win.img release];

	win.img = [[NSImage alloc] initWithCGImage:i size:size];
	win.imgbuf = m;
	win.imgsize = size;

	CGColorSpaceRelease(cs);
	CGDataProviderRelease(dp);
	CGImageRelease(i);

	if(new){
		win.isresizing = 1;	// to call before mousetrack
		sendmouse(1);
	}
DEBUG(@"eresized exit");
}

static void getgesture(NSEvent*);
static void getkeyboard(NSEvent*);
static void getmouse(NSEvent*);

@implementation appview

- (void)mouseMoved:(NSEvent*)e{ getmouse(e);}
- (void)mouseDown:(NSEvent*)e{ getmouse(e);}
- (void)mouseDragged:(NSEvent*)e{ getmouse(e);}
- (void)mouseUp:(NSEvent*)e{ getmouse(e);}
- (void)otherMouseDown:(NSEvent*)e{ getmouse(e);}
- (void)otherMouseDragged:(NSEvent*)e{ getmouse(e);}
- (void)otherMouseUp:(NSEvent*)e{ getmouse(e);}
- (void)rightMouseDown:(NSEvent*)e{ getmouse(e);}
- (void)rightMouseDragged:(NSEvent*)e{ getmouse(e);}
- (void)rightMouseUp:(NSEvent*)e{ getmouse(e);}
- (void)scrollWheel:(NSEvent*)e{ getmouse(e);}

- (void)keyDown:(NSEvent*)e{ getkeyboard(e);}
- (void)flagsChanged:(NSEvent*)e{ getkeyboard(e);}

- (void)magnifyWithEvent:(NSEvent*)e{ DEBUG(@"magnifyWithEvent"); getgesture(e);}
- (void)swipeWithEvent:(NSEvent*)e{ DEBUG(@"swipeWithEvent"); getgesture(e);}
- (void)touchesEndedWithEvent:(NSEvent*)e{ DEBUG(@"touchesEndedWithEvent"); getgesture(e);}

- (BOOL)acceptsFirstResponder{ return YES; }	// to receive mouseMoved events
- (BOOL)isFlipped{ return YES; }
- (BOOL)isOpaque{ return YES; }	// to disable background painting before drawRect calls

- (void)drawRect:(NSRect)r
{
	NSRect sr;
	NSView *v;

	v = [win.obj contentView];

	DEBUG(@"drawRect called [%.0f %.0f] [%.0f %.0f]",
		r.origin.x, r.origin.y, r.size.width, r.size.height);

	if(! NSEqualSizes([v bounds].size, win.imgsize)){
		DEBUG(@"drawRect: contentview & img don't correspond: [%.0f %.0f] [%.0f %.0f]",
			[v bounds].size.width, [v bounds].size.height,
			win.imgsize.width, win.imgsize.height);
		return;
	}

	qlock(win.meeting.l);
	if(win.isresizing){
		if(! NSEqualRects(r, [v bounds])){
			DEBUG(@"drawRect reject osx");
			goto Return;
		}
		win.isresizing = 0;
		DEBUG(@"drawRect serve osx");
	}else{
		if(! NSEqualRects(r, win.flushr)){
			DEBUG(@"drawRect reject p9p");
			goto Return;
		}
		DEBUG(@"drawRect serve p9p");
	}
	win.flushr = r;
	win.osxdrawing = 1;
	rwakeup(&win.meeting);
	DEBUG(@"drawRect rsleep for p9pflushing=1");
	while(win.p9pflushing == 0)
		rsleep(&win.meeting);

	DEBUG(@"drawRect drawInRect [%.0f %.0f] [%.0f %.0f]",
		r.origin.x, r.origin.y, r.size.width, r.size.height);

	sr =  [v convertRect:r fromView:nil];
	[win.img drawInRect:r fromRect:sr
		operation:NSCompositeCopy fraction:1
		respectFlipped:YES hints:nil];

	[win.obj flushWindow];

	win.osxdrawing = 0;
	rwakeup(&win.meeting);
Return:
	DEBUG(@"drawRect exit");
	qunlock(win.meeting.l);
}
@end

void
_flushmemscreen(Rectangle r)
{
	NSRect rect;
	NSView *v;

	v = [win.obj contentView];
	rect = NSMakeRect(r.min.x, r.min.y, Dx(r), Dy(r));

	DEBUG(@"_flushmemscreen called [%.0f %.0f] [%.0f %.0f]",
		rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
	qlock(win.meeting.l);
	if(win.osxdrawing == 0){
		DEBUG(@"_flushmemscreen setNeedsDisplayInRect");
		[v setNeedsDisplayInRect:rect];
		win.flushr = rect;
		DEBUG(@"_flushmemscreen rsleep for osxdrawing=1");
		while(win.osxdrawing == 0)
			rsleep(&win.meeting);
	}
	if(! NSEqualRects(rect, win.flushr)){
		qunlock(win.meeting.l);
		DEBUG(@"_flushmemscreen bad rectangle");
		return;
	}
	win.flushr = NSMakeRect(0,0,0,0);
	win.p9pflushing = 1;
	rwakeup(&win.meeting);
	DEBUG(@"_flushmemscreen rsleep for osxdrawing=0");
	while(win.osxdrawing)
		rsleep(&win.meeting);

	win.p9pflushing = 0;
	DEBUG(@"_flushmemscreen exit");
	qunlock(win.meeting.l);
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

int		kalting;
int		kbuttons;
int		mbuttons;
Point		mpos;
int		scroll;

static void
getkeyboard(NSEvent *e)
{
	uint code;
	int k, m;
	char c;

	m = [e modifierFlags];

	switch([e type]){
	case NSKeyDown:
		kalting = 0;
		c = [[e characters] characterAtIndex:0];
		if(m & NSCommandKeyMask){

// If I add cmd+h in the menu, does the combination
// appear here?  If it doesn't, remove the following
//
//			// OS X interprets a few no matter what we do,
//			switch(c) {
//			case 'm':	// minimize window
//			case 'h':	// hide window
//			case 'H':	// hide others
//			case 'q':	// quit
//				return;
//			}
			if(' '<=c && c<='~') {
				keystroke(Kcmd+c);
				return;
			}
			return;
		}
//		to undersand
		k = c;
		code = [e keyCode];
		if(code < nelem(keycvt) && keycvt[code])
			k = keycvt[code];
		if(k == 0)
			return;
		if(k > 0)
			keystroke(k);
		else
			keystroke(c);
		break;

	case NSFlagsChanged:
		if(mbuttons || kbuttons){
			kbuttons = 0;
			if(m & NSAlternateKeyMask)
				kbuttons |= 2;
			if(m & NSCommandKeyMask)
				kbuttons |= 4;
			sendmouse(0);
		}else
		if(m & NSAlternateKeyMask) {
			kalting = 1;
			keystroke(Kalt);
		}
		break;

	default:
		panic("getkey: unexpected event type");
	}
}

static void
getmousepos(void)
{
	NSPoint p;

	p = [win.obj mouseLocationOutsideOfEventStream];
	p = [[win.obj contentView] convertPoint:p fromView:nil];
//	DEBUG(@"getmousepos: %0.f %0.f", p.x, p.y);
	mpos = Pt(p.x, p.y);
}

static void
getmouse(NSEvent *e)
{
	int b, m;
	float d;

	getmousepos();

	switch([e type]){
	case NSLeftMouseDown:
	case NSLeftMouseUp:
	case NSOtherMouseDown:
	case NSOtherMouseUp:
	case NSRightMouseDown:
	case NSRightMouseUp:

		b = [NSEvent pressedMouseButtons];
		b = b&~6 | (b&4)>>1 | (b&2)<<1;
		b = mouseswap(b);

		if(b == 1){
			m = [e modifierFlags];
			if(m & NSAlternateKeyMask) {
				b = 2;
				// Take the ALT away from the keyboard handler.
				if(kalting) {
					kalting = 0;
					keystroke(Kalt);
				}
			}else
			if(m & NSCommandKeyMask)
				b = 4;
		}
		mbuttons = b;
		break;

	case NSScrollWheel:
#if OSX_VERSION >= 100700
		d = [e scrollingDeltaY];
#else
		d = [e deltaY];
#endif
		if(d>0)
			scroll = 8;
		else if(d<0)
			scroll = 16;
		break;

	case NSMouseMoved:
	case NSLeftMouseDragged:
	case NSRightMouseDragged:
	case NSOtherMouseDragged:
		break;

	default:
		panic("getmouse: unexpected event type");
	}
	sendmouse(0);
}

static void sendexec(int);
static void sendcmd(int, int*);

static void
getgesture(NSEvent *e)
{
	static int undo;
	int dx, dy;

	switch([e type]){

	case NSEventTypeMagnify:
#if OSX_VERSION >= 100700
		[win.obj toggleFullScreen:nil];
#endif
		break;

	case NSEventTypeSwipe:
		
		dx = - [e deltaX];
		dy = - [e deltaY];

		if(dx == -1)
			sendcmd('x', &undo);
		else
		if(dx == +1)
			sendcmd('v', &undo);
		else
		if(dy == -1)
			sendexec(0);
		else
		if(dy == +1)
			sendexec(1);
		else				// fingers lifted
			undo = 0;
		break;

//	When I lift the fingers from the trackpad, I
//	receive 1, 2, or 3 events "touchesEndedWithEvent".
//	Their type is either generic (NSEventTypeGesture)
//	or specific (NSEventTypeSwipe for example).  I
//	always receive at least 1 event of specific type.

//	I sometimes receive NSEventTypeEndGesture
//	apparently, even without implementing
//	"endGestureWithEvent"
//	I even received a NSEventTypeBeginGesture once.

	case NSEventTypeBeginGesture:
		break;

	case NSEventTypeGesture:
	case NSEventTypeEndGesture:
//		do a undo here? because 2 times I had the impression undo was still 1
//		after having lifted my fingers
		undo = 0;
		break;

	default:
		DEBUG(@"getgesture: unexpected event type: %d", [e type]);
	}
}

static void
sendcmd(int c, int *undo)
{
	if(*undo)
		c = 'z';
	*undo = ! *undo;
	keystroke(Kcmd+c);
}

static void
sendexec(int giveargs)
{
	mbuttons = 2;
	sendmouse(0);

	if(giveargs){
		mbuttons |= 1;
		sendmouse(0);
	}
	mbuttons = 0;
	sendmouse(0);
}

static uint
msec(void)
{
	return nsec()/1000000;
}

static void
sendmouse(int resized)
{
	if(resized)
		mouseresized = 1;
	mouserect = win.imgbuf->r;
	mousetrack(mpos.x, mpos.y, kbuttons|mbuttons|scroll, msec());
	scroll = 0;
}

void
setmouse(Point p)
{
	NSPoint q;
	NSRect r;

	r = [[NSScreen mainScreen] frame];

	q = NSMakePoint(p.x,p.y);
	q = [[win.obj contentView] convertPoint:q toView:nil];
	q = [win.obj convertBaseToScreen:q];
	q.y = r.size.height - q.y;

	CGWarpMouseCursorPosition(q);

//	race condition
	mpos = p;
}

//	setBadgeLabel don't have to be in this function.
//	Remove seticon's argument too.
static void
seticon(NSString *s)
{
	NSData *d;
	NSImage *i;

	d = [[NSData alloc]
		initWithBytes:glenda_png
		length:(sizeof glenda_png)];

	i = [[NSImage alloc] initWithData:d];
	if(i){
		[NSApp setApplicationIconImage:i];
		[[NSApp dockTile] display];
		[[NSApp dockTile] setBadgeLabel:s];
	}
	[d release];
	[i release];
}

//	Menu should be called during app creation, not window creation.
//	See ./osx-delegate.m implementation.

//	If an application supports fullscreen, it should
//	add an "Enter Full Screen" menu item to the View
//	menu.  The menu item is now available through
//	Xcode 4.  You can also add the item
//	programmatically, with toggleFullScreen: as the
//	action, nil as the target, and cmd-ctrl-f as the
//	key equivalent.  AppKit will automatically update
//	the menu item title as part of its menu item
//	validation.
static void
makemenu(NSString *s)
{
	NSString *title;
	NSMenu *menu;
	NSMenuItem *appmenu, *item;

	menu = [NSMenu new];
	appmenu = [NSMenuItem new];
	[menu addItem:appmenu];
	[NSApp setMenu:menu];
	[menu release];

	title = [@"Quit " stringByAppendingString:win.label];
	item = [[NSMenuItem alloc]
		initWithTitle:title
		action:@selector(terminate:) keyEquivalent:@"q"];

	menu = [NSMenu new];
	[menu addItem:item];
	[item release];
	[appmenu setSubmenu:menu];
	[appmenu release];
	[menu release];
}

QLock snarfl;

char*
getsnarf(void)
{
	NSString *s;
	NSPasteboard *pb;

	pb = [NSPasteboard generalPasteboard];

//	use NSPasteboardTypeString instead of NSStringPboardType
	qlock(&snarfl);
	s = [pb stringForType:NSStringPboardType];
	qunlock(&snarfl);

//	change the pastebuffer here to see if s is
//	altered. Move the lock accordingly.

	if(s)
		return strdup((char*)[s UTF8String]);		
	else
		return nil;
//	should I call autorelease here for example?
}

void
putsnarf(char *s)
{
	NSArray *t;
	NSString *str;
	NSPasteboard *pb;
	int r;

	if(strlen(s) >= SnarfSize)
		return;

	t = [NSArray arrayWithObject:NSPasteboardTypeString];
	pb = [NSPasteboard generalPasteboard];
	str = [[NSString alloc] initWithUTF8String:s];

	qlock(&snarfl);
	[pb declareTypes:t owner:nil];
	r = [pb setString:str forType:NSPasteboardTypeString];
	qunlock(&snarfl);

	if(!r)
		DEBUG(@"putsnarf: setString failed");
}

void
kicklabel(char *c)
{
}

void
setcursor(Cursor *c)
{
}
