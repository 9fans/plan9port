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
#include  "cocoa-thread.h"	// try libthread when possible
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <cursor.h>
#include "cocoa-screen.h"
#include "osx-keycodes.h"
#include "devdraw.h"
#include "glendapng.h"

AUTOFRAMEWORK(Cocoa)

#define panic sysfatal

/*
 * Incompatible with Magic Mouse?
 */
int reimplementswipe = 0;
	int usecopygesture = 0;

int useoldfullscreen = 0;

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	exits("usage");
}

@interface appdelegate : NSObject
@end

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

	// Libdraw doesn't permit arguments currently.

	ARGBEGIN{
	case 'D':		// only for good ps -a listings
		break;
	default:
		usage();
	}ARGEND

	if(usecopygesture)
		reimplementswipe = 1;

	if(OSX_VERSION < 100700)
		[NSAutoreleasePool new];

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp setDelegate:[appdelegate new]];
	[NSApp activateIgnoringOtherApps:YES];
	[NSApp run];
}

struct {
	NSWindow		*std;
	NSWindow		*ofs;			/* old fullscreen */
	NSWindow		*p;
	NSView			*content;
	Cursor			*cursor;
	char				*rectstr;
	NSBitmapImageRep	*img;
	NSRect			flushrect;
	int				needflush;
} win;

static void autohide(int);
static void drawimg(void);
static void flushwin(void);
static void getmousepos(void);
static void makeicon(void);
static void makemenu(void);
static void makewin(void);
static void resize(void);
static void sendmouse(void);
static void setcursor0(void);
static void togglefs(void);

@implementation appdelegate
- (void)applicationDidFinishLaunching:(id)arg
{
	makeicon();
	makemenu();
	[NSApplication
		detachDrawingThread:@selector(callservep9p:)
		toTarget:[self class] withObject:nil];
}
- (void)windowDidBecomeKey:(id)arg
{
	getmousepos();
	sendmouse();
}
- (void)windowDidResize:(id)arg
{
	getmousepos();
	sendmouse();

	if([win.p inLiveResize])
		return;

	resize();
}
- (void)windowDidEndLiveResize:(id)arg
{
	resize();
}
- (void)windowDidDeminiaturize:(id)arg
{
	resize();
}
- (void)windowDidChangeScreenProfile:(id)arg
{
	resize();
}
- (void)windowDidChangeScreen:(id)arg
{
	if(win.p == win.ofs)
		autohide(1);
	[win.ofs setFrame:[[win.p screen] frame] display:YES];
	resize();
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(id)arg
{
	return YES;
}
+ (void)callservep9p:(id)arg
{
	servep9p();
	[NSApp terminate:self];
}
+ (void)calldrawimg:(id)arg{ drawimg();}
+ (void)callflushwin:(id)arg{ flushwin();}
+ (void)callmakewin:(id)arg{ makewin();}
+ (void)callsetcursor0:(id)arg{ setcursor0();}
- (void)calltogglefs:(id)arg{ togglefs();}
@end

static Memimage* makeimg(void);

Memimage*
attachscreen(char *label, char *winsize)
{
	static int first = 1;

	if(first)
		first = 0;
	else
		panic("attachscreen called twice");

	if(label == nil)
		label = "gnot a label";

	win.rectstr = strdup(winsize);

//	Create window in main thread,
//	else no cursor change when resizing.
	[appdelegate
		performSelectorOnMainThread:@selector(callmakewin:)
		withObject:nil
		waitUntilDone:YES];
//	makewin();

	kicklabel(label);
	return makeimg();
}

@interface appview : NSView
@end

@interface appwin : NSWindow
@end

@implementation appwin
- (NSTimeInterval)animationResizeTime:(NSRect)r
{
	return 0;
}
- (BOOL)canBecomeKeyWindow
{
	// just keyboard? or all inputs?
	return YES;	// else no keyboard focus with NSBorderlessWindowMask
}
@end

enum
{
	Winstyle = NSTitledWindowMask
		| NSClosableWindowMask
		| NSMiniaturizableWindowMask
		| NSResizableWindowMask
};

static void
makewin(void)
{
	NSRect r, sr;
	Rectangle wr;
	char *s;
	int set;

	s = win.rectstr;
	sr = [[NSScreen mainScreen] frame];

	if(s && *s){
		if(parsewinsize(s, &wr, &set) < 0)
			sysfatal("%r");
	}else{
		wr = Rect(0, 0, sr.size.width*2/3, sr.size.height*2/3);
		set = 0;
	}
//	The origin is the left-bottom corner with Cocoa.
	r = NSMakeRect(wr.min.x, r.size.height-wr.min.y, Dx(wr), Dy(wr));

	win.std = [[appwin alloc]
		initWithContentRect:r
		styleMask:Winstyle
		backing:NSBackingStoreBuffered defer:NO];
	if(!set)
		[win.std center];
#if OSX_VERSION >= 100700
	[win.std setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
#endif
	[win.std setMinSize:NSMakeSize(128,128)];
	[win.std setAcceptsMouseMovedEvents:YES];
	[win.std setDelegate:[NSApp delegate]];

	win.ofs = [[appwin alloc]
		initWithContentRect:sr
		styleMask:NSBorderlessWindowMask
		backing:NSBackingStoreBuffered defer:NO];
	[win.ofs setAcceptsMouseMovedEvents:YES];
	[win.ofs setDelegate:[NSApp delegate]];

	win.content = [appview new];
	[win.content setAcceptsTouchEvents:YES];
	win.p = win.std;
	[win.p setContentView:win.content];
	[win.p makeKeyAndOrderFront:nil];
}

// explain the bottom-corner bug here (osx-screen-carbon.m:/^eresized)
static Memimage*
makeimg(void)
{
	static int first = 1;
	Memimage *m;
	NSSize size;
	Rectangle r;
	uint ch;

	if(first){
		memimageinit();
		first = 0;
	}
	size = [win.content bounds].size;

	if(size.width<=0 || size.height<=0){
		NSLog(@"bad content size: %.0f %.0f", size.width, size.height);
		return nil;
	}
	r = Rect(0, 0, size.width, size.height);
	ch = XBGR32;
	m = allocmemimage(r, ch);
	if(m == nil)
		panic("allocmemimage: %r");
	if(m->data == nil)
		panic("m->data == nil");

	if(win.img)
		[win.img release];
	win.img = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:&m->data->bdata
		pixelsWide:Dx(r)
		pixelsHigh:Dy(r)
		bitsPerSample:8
		samplesPerPixel:4
		hasAlpha:YES
		isPlanar:NO
		colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:bytesperline(r, 32)
		bitsPerPixel:32];

	_drawreplacescreenimage(m);
	return m;
}

static void
resize(void)
{
	makeimg();

	mouseresized = 1;
	sendmouse();
}

void
_flushmemscreen(Rectangle r)
{
	win.flushrect = NSMakeRect(r.min.x, r.min.y, Dx(r), Dy(r));

//	Call "lockFocusIfCanDraw" from main thread, else
//	we deadlock while synchronizing both threads with
//	qlock(): main thread must apparently be idle while we call it.
	[appdelegate
		performSelectorOnMainThread:@selector(calldrawimg:)
		withObject:nil
		waitUntilDone:YES];
}

static void drawresizehandle(void);

static void
drawimg(void)
{
	static int first = 1;
	NSRect dr, sr;

	if(first){
		[NSTimer scheduledTimerWithTimeInterval:0.033
			target:[appdelegate class]
			selector:@selector(callflushwin:) userInfo:nil
			repeats:YES];
		first = 0;
	}
	dr = win.flushrect;
	sr =  [win.content convertRect:dr fromView:nil];

	if([win.content lockFocusIfCanDraw]){
		[win.img drawInRect:dr fromRect:sr
			operation:NSCompositeCopy fraction:1
			respectFlipped:YES hints:nil];

		if(OSX_VERSION<100700 && win.p==win.std)
			drawresizehandle();

		[win.content unlockFocus];
		win.needflush = 1;
	}
}

static void
flushwin(void)
{
	if(win.needflush){
		[win.p flushWindow];
		win.needflush = 0;
	}
}

enum
{
	Pixel = 1,
	Handlesize = 16*Pixel,
};

static void
drawresizehandle(void)
{
	NSBezierPath *p;
	NSRect r;
	NSSize size;
	Point o;

	size = [win.img size];
	o = Pt(size.width+1-Handlesize, size.height+1-Handlesize);
	r = NSMakeRect(o.x, o.y, Handlesize, Handlesize);
	if(NSIntersectsRect(r, win.flushrect) == 0)
		return;

	[[NSColor whiteColor] setFill];
	[[NSColor lightGrayColor] setStroke];

	[NSBezierPath fillRect:r];
	[NSBezierPath strokeRect:r];


	[[NSColor darkGrayColor] setStroke];

	p = [NSBezierPath bezierPath];

	[p moveToPoint:NSMakePoint(o.x+4, o.y+13)];
	[p lineToPoint:NSMakePoint(o.x+13, o.y+4)];

	[p moveToPoint:NSMakePoint(o.x+8, o.y+13)];
	[p lineToPoint:NSMakePoint(o.x+13, o.y+8)];

	[p moveToPoint:NSMakePoint(o.x+12, o.y+13)];
	[p lineToPoint:NSMakePoint(o.x+13, o.y+12)];

	[p stroke];
}

static void getgesture(NSEvent*);
static void getkeyboard(NSEvent*);
static void getmouse(NSEvent*);
static void gettouch(NSEvent*, int);

@implementation appview

- (void)drawRect:(NSRect)r
{
	// else no window background
}
- (BOOL)isFlipped
{
	return YES;	// to have the origin at top left
}
- (BOOL)acceptsFirstResponder
{
	return YES;	// to receive mouseMoved events
}	
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

- (void)swipeWithEvent:(NSEvent*)e{ getgesture(e);}
- (void)magnifyWithEvent:(NSEvent*)e{ getgesture(e);}

- (void)touchesBeganWithEvent:(NSEvent*)e
{
	gettouch(e, NSTouchPhaseBegan);
}
- (void)touchesMovedWithEvent:(NSEvent*)e
{
	gettouch(e, NSTouchPhaseMoved);
}
- (void)touchesEndedWithEvent:(NSEvent*)e
{
	gettouch(e, NSTouchPhaseEnded);
}
- (void)touchesCancelledWithEvent:(NSEvent*)e
{
	gettouch(e, NSTouchPhaseCancelled);
}
@end

struct {
	int		kalting;
	int		kbuttons;
	int		mbuttons;
	Point		mpos;
	int		mscroll;
	int		undo;
} in;

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

static void
getkeyboard(NSEvent *e)
{
	char c;
	int k, m;
	uint code;

	m = [e modifierFlags];

	switch([e type]){
	case NSKeyDown:
		in.kalting = 0;
		c = [[e characters] characterAtIndex:0];
		if(m & NSCommandKeyMask){
			if(' '<=c && c<='~'){
				keystroke(Kcmd+c);
			}
			return;
		}
//		to understand
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
		if(in.mbuttons || in.kbuttons){
			in.kbuttons = 0;
			if(m & NSAlternateKeyMask)
				in.kbuttons |= 2;
			if(m & NSCommandKeyMask)
				in.kbuttons |= 4;
			sendmouse();
		}else
		if(m & NSAlternateKeyMask){
			in.kalting = 1;
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

	p = [win.p mouseLocationOutsideOfEventStream];
	p = [win.content convertPoint:p fromView:nil];
	in.mpos = Pt(p.x, p.y);
}

static void
getmouse(NSEvent *e)
{
	float d;
	int b, m;

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
			if(m & NSAlternateKeyMask){
				b = 2;
				// Take the ALT away from the keyboard handler.
				if(in.kalting){
					in.kalting = 0;
					keystroke(Kalt);
				}
			}else
			if(m & NSCommandKeyMask)
				b = 4;
		}
		in.mbuttons = b;
		break;

	case NSScrollWheel:
#if OSX_VERSION >= 100700
		d = [e scrollingDeltaY];
#else
		d = [e deltaY];
#endif
		if(d>0)
			in.mscroll = 8;
		else
		if(d<0)
			in.mscroll = 16;
		break;

	case NSMouseMoved:
	case NSLeftMouseDragged:
	case NSRightMouseDragged:
	case NSOtherMouseDragged:
		break;

	default:
		panic("getmouse: unexpected event type");
	}
	sendmouse();
}

static void sendswipe(int, int);

static void
getgesture(NSEvent *e)
{
	switch([e type]){

	case NSEventTypeMagnify:
//		if(fabs([e magnification]) > 0.025)
			togglefs();
		break;

	case NSEventTypeSwipe:
		if(reimplementswipe)
			break;

		sendswipe(-[e deltaX], -[e deltaY]);
		break;
	}
}

static void sendclick(int);
static void sendchord(int, int);
static void sendcmd(int);

static uint
msec(void)
{
	return nsec()/1000000;
}

enum
{
	Msec = 1,
	Maxtap = 400*Msec,
	Maxtouch = 3,
	Mindelta = 0,
	Minswipe = 15,
};

static void
gettouch(NSEvent *e, int type)
{
	static NSPoint delta, odelta;
	static NSTouch *toucha[Maxtouch];
	static NSTouch *touchb[Maxtouch];
	static int done, ntouch, tapping;
	static uint taptime;
	NSArray *a;
	NSPoint d;
	NSSet *set;
	NSSize s;
	int i, p;

	if(reimplementswipe==0 && type!=NSTouchPhaseEnded)
		return;

	switch(type){

	case NSTouchPhaseBegan:
		p = NSTouchPhaseTouching;
		set = [e touchesMatchingPhase:p inView:nil];
		if(set.count == 3){
			tapping = 1;
			taptime = msec();
		}else
		if(set.count > 3)
			tapping = 0;
		return;

	case NSTouchPhaseMoved:
		p = NSTouchPhaseMoved;
		set = [e touchesMatchingPhase:p inView:nil];
		a = [set allObjects];
		if(set.count > Maxtouch)
			return;
		if(ntouch==0){
			ntouch = set.count;
			for(i=0; i<ntouch; i++){
				assert(toucha[i] == nil);
				toucha[i] = [[a objectAtIndex:i] retain];
			}
			return;
		}
		if(ntouch != set.count)
			break;
		if(done)
			return;

		d = NSMakePoint(0,0);
		for(i=0; i<ntouch; i++){
			assert(touchb[i] == nil);
			touchb[i] = [a objectAtIndex:i];
			d.x += touchb[i].normalizedPosition.x;
			d.y += touchb[i].normalizedPosition.y;
			d.x -= toucha[i].normalizedPosition.x;
			d.y -= toucha[i].normalizedPosition.y;
		}
		s = toucha[0].deviceSize;
		d.x = d.x/ntouch * s.width;
		d.y = d.y/ntouch * s.height;
		if(fabs(d.x)>Mindelta || fabs(d.y)>Mindelta){
			tapping = 0;
			if(ntouch != 3){
				done = 1;
				goto Return;
			}
			delta = NSMakePoint(delta.x+d.x, delta.y+d.y);
			d = NSMakePoint(fabs(delta.x), fabs(delta.y));
			if(d.x>Minswipe || d.y>Minswipe){
				if(d.x > d.y)
					delta = NSMakePoint(-copysign(1,delta.x), 0);
				else
					delta = NSMakePoint(0, copysign(1,delta.y));

				if(! NSEqualPoints(delta, odelta)){
//					if(ntouch == 3)
						sendswipe(-delta.x, -delta.y);
					odelta = delta;
				}
				done = 1;
				goto Return;
			}
			for(i=0; i<ntouch; i++){
				[toucha[i] release];
				toucha[i] = [touchb[i] retain];
			}
		}
Return:
		for(i=0; i<ntouch; i++)
			touchb[i] = nil;
		return;

	case NSTouchPhaseEnded:
		p = NSTouchPhaseTouching;
		set = [e touchesMatchingPhase:p inView:nil];
		if(set.count == 0){
			in.undo = 0;

			if(usecopygesture)
				if(tapping && msec()-taptime<Maxtap)
					sendclick(2);
			tapping = 0;
			odelta = NSMakePoint(0,0);
		}
		break;

	case NSTouchPhaseCancelled:
		break;

	default:
		panic("gettouch: unexpected event type: %d", type);
	}
	for(i=0; i<ntouch; i++){
		[toucha[i] release];
		toucha[i] = nil;
	}
	for(i=0; i<ntouch; i++){
		assert(toucha[i] == nil);
		assert(touchb[i] == nil);
	}
	ntouch = 0;
	delta = NSMakePoint(0,0);
	done = 0;
}

static void
sendswipe(int dx, int dy)
{
	if(dx == -1){
		sendcmd('x');
	}else
	if(dx == +1){
		sendcmd('v');
	}else
	if(dy == -1){
		if(usecopygesture)
			sendcmd('c');
		else
			sendclick(2);
	}else
	if(dy == +1){
		sendchord(2,1);
	}
}

static void
sendcmd(int c)
{
	if(c=='x' || c=='v'){
		if(in.undo)
			c = 'z';
		in.undo = ! in.undo;
	}
	keystroke(Kcmd+c);
}

static void
sendclick(int b)
{
	in.mbuttons = b;
	sendmouse();
	in.mbuttons = 0;
	sendmouse();
}

static void
sendchord(int b1, int b2)
{
	in.mbuttons = b1;
	sendmouse();
	in.mbuttons |= b2;
	sendmouse();
	in.mbuttons = 0;
	sendmouse();
}

static void
sendmouse(void)
{
	NSSize size;
	int b;

	size = [win.img size];
	mouserect = Rect(0, 0, size.width, size.height);

	b = in.kbuttons | in.mbuttons | in.mscroll;
	mousetrack(in.mpos.x, in.mpos.y, b, msec());
	in.mscroll = 0;
}

void
setmouse(Point p)
{
	static int first = 1;
	NSPoint q;
	NSRect r;

	if(first){
//		try to move Acme's scrollbars without that!
		CGSetLocalEventsSuppressionInterval(0);
		first = 0;
	}
	r = [[win.p screen] frame];

	q = NSMakePoint(p.x,p.y);
	q = [win.content convertPoint:q toView:nil];
	q = [win.p convertBaseToScreen:q];
	q.y = r.size.height - q.y;

	CGWarpMouseCursorPosition(NSPointToCGPoint(q));

//	race condition
	in.mpos = p;
}

static void
togglefs(void)
{
#if OSX_VERSION >= 100700
	if(useoldfullscreen == 0){
		[win.p toggleFullScreen:nil];
		return;
	}
#endif
	NSScreen *screen;
	int willfs;

	screen = [win.p screen];

	willfs = !NSEqualRects([win.p frame], [screen frame]);

	autohide(willfs);

	[win.content retain];
	[win.p orderOut:nil];
	[win.p setContentView:nil];

	if(willfs)
		win.p = win.ofs;
	else
		win.p = win.std;

	[win.p setContentView:win.content];
	[win.p makeKeyAndOrderFront:nil];
	[win.content release];

	resize();
}

static void
autohide(int set)
{
	NSScreen *s,*s0;
	int opt;

	s = [win.p screen];
	s0 = [[NSScreen screens] objectAtIndex:0];

	if(set && s==s0)
		opt = NSApplicationPresentationAutoHideDock
			| NSApplicationPresentationAutoHideMenuBar;
	else
		opt = NSApplicationPresentationDefault;

	[NSApp setPresentationOptions:opt];
}

//	Rewrite this function
//	See ./osx-delegate.m implementation (NSLocalizedString)
static void
makemenu(void)
{
	NSString *title;
	NSMenu *menu;
	NSMenuItem *appmenu, *item;

	menu = [NSMenu new];
	appmenu = [NSMenuItem new];
	[menu addItem:appmenu];
	[NSApp setMenu:menu];
	[menu release];

	menu = [NSMenu new];

	title = @"Full Screen";
	item = [[NSMenuItem alloc]
		initWithTitle:title
		action:@selector(calltogglefs:) keyEquivalent:@"f"];
	[menu addItem:item];
	[item release];

	title = @"Quit";
	item = [[NSMenuItem alloc]
		initWithTitle:title
		action:@selector(terminate:) keyEquivalent:@"q"];
	[menu addItem:item];
	[item release];

	[appmenu setSubmenu:menu];
	[appmenu release];
	[menu release];
}

static void
makeicon(void)
{
	NSData *d;
	NSImage *i;

	d = [[NSData alloc]
		initWithBytes:glenda_png
		length:(sizeof glenda_png)];

	i = [[NSImage alloc] initWithData:d];
	[NSApp setApplicationIconImage:i];
	[[NSApp dockTile] display];
	[i release];
	[d release];
}

QLock snarfl;

char*
getsnarf(void)
{
	NSPasteboard *pb;
	NSString *s;

	pb = [NSPasteboard generalPasteboard];

	qlock(&snarfl);
	s = [pb stringForType:NSPasteboardTypeString];
	qunlock(&snarfl);

	if(s)
		return strdup((char*)[s UTF8String]);		
	else
		return nil;
}

void
putsnarf(char *s)
{
	NSArray *t;
	NSPasteboard *pb;
	NSString *str;

	if(strlen(s) >= SnarfSize)
		return;

	t = [NSArray arrayWithObject:NSPasteboardTypeString];
	pb = [NSPasteboard generalPasteboard];
	str = [[NSString alloc] initWithUTF8String:s];

	qlock(&snarfl);
	[pb declareTypes:t owner:nil];
	[pb setString:str forType:NSPasteboardTypeString];
	qunlock(&snarfl);

	[str release];
}

void
kicklabel(char *label)
{
	NSString *s;

	if(label == nil)
		return;

	s = [[NSString alloc] initWithUTF8String:label];
	[win.std setTitle:s];
	[win.ofs setTitle:s];
	[[NSApp dockTile] setBadgeLabel:s];
	[s release];
}

void
setcursor(Cursor *cursor)
{
	win.cursor = cursor;

//	no cursor change unless in main thread
	[appdelegate
		performSelectorOnMainThread:@selector(callsetcursor0:)
		withObject:nil
		waitUntilDone:YES];
//	setcursor0();

	win.cursor = nil;
}

static void
setcursor0(void)
{
	Cursor *c;
	NSBitmapImageRep *r;
	NSCursor *d;
	NSImage *i;
	NSPoint p;
	int b;
	uchar *plane[5];

	c = win.cursor;

	if(c == nil){
		[[NSCursor arrowCursor] set];
		return;
	}
	r = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:nil
		pixelsWide:16
		pixelsHigh:16
		bitsPerSample:1
		samplesPerPixel:2
		hasAlpha:YES
		isPlanar:YES
		colorSpaceName:NSDeviceBlackColorSpace
		bytesPerRow:2
		bitsPerPixel:1];

	[r getBitmapDataPlanes:plane];

	for(b=0; b<2*16; b++){
		plane[0][b] = c->set[b];
		plane[1][b] = c->clr[b];
	}
	p = NSMakePoint(-c->offset.x, -c->offset.y);
	i = [NSImage new];
	[i addRepresentation:r];
	d = [[NSCursor alloc] initWithImage:i hotSpot:p];
	[d set];

	[d release];
	[r release];
	[i release];
}
