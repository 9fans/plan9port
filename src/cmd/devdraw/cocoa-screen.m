/*
 * Cocoa's event loop must be in main thread.
 */

#define Cursor OSXCursor
#define Point OSXPoint
#define Rect OSXRect

#import <Cocoa/Cocoa.h>

#undef Cursor
#undef Point
#undef Rect

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

AUTOFRAMEWORK(Cocoa)

#define panic sysfatal

int usegestures = 0;
int useoldfullscreen = 0;
int usebigarrow = 0;

extern Cursor bigarrow;

void setcursor0(Cursor *c);

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	threadexitsall("usage");
}

@interface appdelegate : NSObject
@end

void
threadmain(int argc, char **argv)
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
	case 'D':		/* for good ps -a listings */
		break;
	case 'f':
		useoldfullscreen = 1;
		break;
	case 'g':
		usegestures = 1;
		break;
	case 'b':
		usebigarrow = 1;
		break;
	default:
		usage();
	}ARGEND

	if(OSX_VERSION < 100700)
		[NSAutoreleasePool new];

	// Reset cursor to ensure we start
	// with bigarrow.
	if(usebigarrow)
		setcursor0(nil);

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp setDelegate:[appdelegate new]];
	[NSApp activateIgnoringOtherApps:YES];
	[NSApp run];
}

#define WIN	win.ofs[win.isofs]

struct
{
	NSWindow	*ofs[2];	/* ofs[1] for old fullscreen; ofs[0] else */
	int			isofs;
	NSView		*content;
	char			*rectstr;
	NSBitmapImageRep	*img;
	NSRect		flushrect;
	int			needflush;
	NSCursor		*cursor;
	QLock		cursorl;
} win;

struct
{
	int		kalting;
	int		kbuttons;
	int		mbuttons;
	Point		mpos;
	int		mscroll;
	int		undo;
	int		touchevent;
} in;

static void autohide(int);
static void drawimg(void);
static void flushwin(void);
static void followzoombutton(NSRect);
static void getmousepos(void);
static void makeicon(void);
static void makemenu(void);
static void makewin(void);
static void sendmouse(void);
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
	in.touchevent = 0;

	getmousepos();
	sendmouse();
}
- (void)windowDidResize:(id)arg
{
	getmousepos();
	sendmouse();
}
- (void)windowDidEndLiveResize:(id)arg
{
	[win.content display];
}
- (void)windowDidChangeScreen:(id)arg
{
	if(win.isofs)
		autohide(1);
	[win.ofs[1] setFrame:[[WIN screen] frame] display:YES];
}
- (BOOL)windowShouldZoom:(id)arg toFrame:(NSRect)r
{
	followzoombutton(r);
	return YES;
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
+ (void)callsetcursor0:(id)arg{ setcursor0([[arg autorelease] pointerValue]);}
- (void)calltogglefs:(id)arg{ togglefs();}
@end

static Memimage* initimg(void);

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

	/*
	 * Create window in main thread, else no cursor
	 * change while resizing.
	 */
	[appdelegate
		performSelectorOnMainThread:@selector(callmakewin:)
		withObject:nil
		waitUntilDone:YES];
//	makewin();

	kicklabel(label);
	return initimg();
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
	return YES;	/* else no keyboard with old fullscreen */
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
	NSWindow *w;
	Rectangle wr;
	char *s;
	int i, set;

	s = win.rectstr;
	sr = [[NSScreen mainScreen] frame];

	if(s && *s){
		if(parsewinsize(s, &wr, &set) < 0)
			sysfatal("%r");
	}else{
		wr = Rect(0, 0, sr.size.width*2/3, sr.size.height*2/3);
		set = 0;
	}

	/*
	 * The origin is the left bottom corner for Cocoa.
	 */
	r.origin.y = sr.size.height-wr.max.y;
	r = NSMakeRect(wr.min.x, r.origin.y, Dx(wr), Dy(wr));
	r = [NSWindow contentRectForFrameRect:r
		styleMask:Winstyle];

	w = [[appwin alloc]
		initWithContentRect:r
		styleMask:Winstyle
		backing:NSBackingStoreBuffered defer:NO];
	if(!set)
		[w center];
#if OSX_VERSION >= 100700
	[w setCollectionBehavior:
		NSWindowCollectionBehaviorFullScreenPrimary];
#endif
	[w setContentMinSize:NSMakeSize(128,128)];

	win.ofs[0] = w;
	win.ofs[1] = [[appwin alloc]
		initWithContentRect:sr
		styleMask:NSBorderlessWindowMask
		backing:NSBackingStoreBuffered defer:YES];
	for(i=0; i<2; i++){
		[win.ofs[i] setAcceptsMouseMovedEvents:YES];
		[win.ofs[i] setDelegate:[NSApp delegate]];
		[win.ofs[i] setDisplaysWhenScreenProfileChanges:NO];
	}
	win.isofs = 0;
	win.content = [appview new];
	[win.content setAcceptsTouchEvents:YES];
	[WIN setContentView:win.content];
	[WIN makeKeyAndOrderFront:nil];
}

static Memimage*
initimg(void)
{
	Memimage *i;
	NSSize size;
	Rectangle r;

	size = [win.content bounds].size;

	r = Rect(0, 0, size.width, size.height);
	i = allocmemimage(r, XBGR32);
	if(i == nil)
		panic("allocmemimage: %r");
	if(i->data == nil)
		panic("i->data == nil");

	win.img = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:&i->data->bdata
		pixelsWide:Dx(r)
		pixelsHigh:Dy(r)
		bitsPerSample:8
		samplesPerPixel:3
		hasAlpha:NO
		isPlanar:NO
		colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:bytesperline(r, 32)
		bitsPerPixel:32];

	return i;
}

void
_flushmemscreen(Rectangle r)
{
	win.flushrect = NSMakeRect(r.min.x, r.min.y, Dx(r), Dy(r));

	/*
	 * Call "lockFocusIfCanDraw" from main thread, else
	 * we deadlock while synchronizing both threads with
	 * qlock(): main thread must apparently be idle while
	 * we call it.  (This is also why Devdraw shows
	 * occasionally an empty window: I found no
	 * satisfactory way to wait for P9P's image.)
	 */
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

		/*
		 * To round the window's bottom corners, we can use
		 * "NSCompositeSourceIn", but this slows down
		 * trackpad scrolling considerably in Acme.  Else we
		 * can use "bezierPathWithRoundedRect" with "addClip",
		 * but it's still too slow for wide Acme windows.
		 */
		[win.img drawInRect:dr fromRect:sr
//			operation:NSCompositeSourceIn fraction:1
			operation:NSCompositeCopy fraction:1
			respectFlipped:YES hints:nil];

		if(OSX_VERSION<100700 && win.isofs==0)
			drawresizehandle();

		[win.content unlockFocus];
		win.needflush = 1;
	}
}

static void
flushwin(void)
{
	if(win.needflush){
		[WIN flushWindow];
		win.needflush = 0;
	}
}

enum
{
	Pixel = 1,
	Barsize = 4*Pixel,
	Handlesize = 3*Barsize + 1*Pixel,
};

static void
drawresizehandle(void)
{
	NSColor *color[Barsize];
	NSPoint a,b;
	NSRect r;
	NSSize size;
	Point c;
	int i,j;

	size = [win.img size];
	c = Pt(size.width, size.height);
	r = NSMakeRect(0, 0, Handlesize, Handlesize);
	r.origin = NSMakePoint(c.x-Handlesize, c.y-Handlesize);
	if(NSIntersectsRect(r, win.flushrect) == 0)
		return;

	[[WIN graphicsContext] setShouldAntialias:NO];

	color[0] = [NSColor clearColor];
	color[1] = [NSColor darkGrayColor];
	color[2] = [NSColor lightGrayColor];
	color[3] = [NSColor whiteColor];

	for(i=1; i+Barsize <= Handlesize; )
		for(j=0; j<Barsize; j++){
			[color[j] setStroke];
			i++;
			a = NSMakePoint(c.x-i, c.y-1);
			b = NSMakePoint(c.x-2, c.y+1-i);
			[NSBezierPath strokeLineFromPoint:a toPoint:b];
		}
}

static void
resizeimg()
{
	[win.img release];
	_drawreplacescreenimage(initimg());
	mouseresized = 1;
	sendmouse();
}

static void getgesture(NSEvent*);
static void getkeyboard(NSEvent*);
static void getmouse(NSEvent*);
static void gettouch(NSEvent*, int);

@implementation appview

- (void)drawRect:(NSRect)r
{
	static int first = 1;

	if([WIN inLiveResize])
		return;

	if(first)
		first = 0;
	else
		resizeimg();

	/* We should wait for P9P's image here. */
}
- (void)resetCursorRects
{
	NSCursor *c;

	[super resetCursorRects];

	qlock(&win.cursorl);

	c = win.cursor;
	if(c == nil)
		c = [NSCursor arrowCursor];

	[self addCursorRect:[self bounds] cursor:c];
	qunlock(&win.cursorl);
}
- (BOOL)isFlipped
{
	return YES;	/* to have the origin at top left */
}
- (BOOL)acceptsFirstResponder
{
	return YES;	/* to receive mouseMoved events */
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
	NSString *s;
	char c;
	int k, m;
	uint code;

	m = [e modifierFlags];

	switch([e type]){
	case NSKeyDown:
		in.kalting = 0;

		s = [e characters];
		c = [s UTF8String][0];

		if(m & NSCommandKeyMask){
			if(' '<=c && c<='~')
				keystroke(Kcmd+c);
			break;
		}
		k = c;
		code = [e keyCode];
		if(code<nelem(keycvt) && keycvt[code])
			k = keycvt[code];
		if(k==0)
			break;
		if(k>0)
			keystroke(k);
		else
			keystroke([s characterAtIndex:0]);
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

	p = [WIN mouseLocationOutsideOfEventStream];
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

#define Minpinch	0.050

enum
{
	Left		= -1,
	Right	= +1,
	Up		= +2,
	Down	= -2,
};

static int
getdir(int dx, int dy)
{
	return dx + 2*dy;
}

static void interpretswipe(int);

static void
getgesture(NSEvent *e)
{
	static float sum;
	int dir;

	if(usegestures == 0)
		return;

	switch([e type]){

	case NSEventTypeMagnify:
		sum += [e magnification];
		if(fabs(sum) > Minpinch){
			togglefs();
			sum = 0;
		}
		break;

	case NSEventTypeSwipe:
		dir = getdir(-[e deltaX], [e deltaY]);

		if(in.touchevent)
			if(dir==Up || dir==Down)
				break;
		interpretswipe(dir);
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

#define Inch		72
#define Cm		Inch/2.54

#define Mindelta	0.0*Cm
#define Xminswipe	0.5*Cm
#define Yminswipe	0.1*Cm

enum
{
	Finger = 1,
	Msec = 1,

	Maxtap = 400*Msec,
	Maxtouch = 3*Finger,
};

static void
gettouch(NSEvent *e, int type)
{
	static NSPoint delta;
	static NSTouch *toucha[Maxtouch];
	static NSTouch *touchb[Maxtouch];
	static int done, ntouch, odir, tapping;
	static uint taptime;
	NSArray *a;
	NSPoint d;
	NSSet *set;
	NSSize s;
	int dir, i, p;

	if(usegestures == 0)
		return;

	switch(type){

	case NSTouchPhaseBegan:
		in.touchevent = 1;
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
//				assert(toucha[i] == nil);
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
//			assert(touchb[i] == nil);
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
			if(d.x>Xminswipe || d.y>Yminswipe){
				if(d.x > d.y)
					dir = delta.x>0? Right : Left;
				else
					dir = delta.y>0? Up : Down;
				if(dir != odir){
//					if(ntouch == 3)
						if(dir==Up || dir==Down)
							interpretswipe(dir);
					odir = dir;
				}
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
			if(tapping && msec()-taptime<Maxtap)
				sendclick(2);
			odir = 0;
			tapping = 0;
			in.undo = 0;
			in.touchevent = 0;
		}
		break;

	case NSTouchPhaseCancelled:
		break;

	default:
		panic("gettouch: unexpected event type");
	}
	for(i=0; i<ntouch; i++){
		[toucha[i] release];
		toucha[i] = nil;
	}
	delta = NSMakePoint(0,0);
	done = 0;
	ntouch = 0;
}

static void
interpretswipe(int dir)
{
	if(dir == Left)
		sendcmd('x');
	else
	if(dir == Right)
		sendcmd('v');
	else
	if(dir == Up)
		sendcmd('c');
	else
	if(dir == Down)
		sendchord(2,1);
}

static void
sendcmd(int c)
{
	if(in.touchevent && (c=='x' || c=='v')){
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

	size = [win.content bounds].size;
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
		/* Try to move Acme's scrollbars without that! */
		CGSetLocalEventsSuppressionInterval(0);
		first = 0;
	}
	r = [[WIN screen] frame];

	q = NSMakePoint(p.x, p.y);
	q = [win.content convertPoint:q toView:nil];
	q = [WIN convertBaseToScreen:q];
	q.y = r.size.height - q.y;

	CGWarpMouseCursorPosition(NSPointToCGPoint(q));

	in.mpos = p;	// race condition
}

static void
followzoombutton(NSRect r)
{
	NSRect wr;
	Point p;

	wr = [WIN frame];
	wr.origin.y += wr.size.height;
	r.origin.y += r.size.height;

	p.x = (r.origin.x - wr.origin.x) + in.mpos.x;
	p.y = -(r.origin.y - wr.origin.y) + in.mpos.y;
	setmouse(p);
}

static void
togglefs(void)
{
#if OSX_VERSION >= 100700
	if(useoldfullscreen == 0){
		[WIN toggleFullScreen:nil];
		return;
	}
#endif
	NSScreen *screen;
	int willfs;

	screen = [WIN screen];

	willfs = !NSEqualRects([WIN frame], [screen frame]);

	autohide(willfs);

	[win.content retain];
	[WIN orderOut:nil];
	[WIN setContentView:nil];

	win.isofs = willfs;

	[WIN setContentView:win.content];
	[WIN makeKeyAndOrderFront:nil];
	[win.content release];
}

static void
autohide(int set)
{
	NSScreen *s,*s0;
	int opt;

	s = [WIN screen];
	s0 = [[NSScreen screens] objectAtIndex:0];

	if(set && s==s0)
		opt = NSApplicationPresentationAutoHideDock
			| NSApplicationPresentationAutoHideMenuBar;
	else
		opt = NSApplicationPresentationDefault;

	[NSApp setPresentationOptions:opt];
}

static void
makemenu(void)
{
	NSMenu *m;
	NSMenuItem *i,*i0;

	m = [NSMenu new];
	i0 = [NSMenuItem new];
	[m addItem:i0];
	[NSApp setMainMenu:m];
	[m release];

	m = [NSMenu new];

	i = [[NSMenuItem alloc] initWithTitle:@"Full Screen"
		action:@selector(calltogglefs:)
		keyEquivalent:@"f"];
	[m addItem:i];
	[i release];

	i = [[NSMenuItem alloc] initWithTitle:@"Quit"
		action:@selector(terminate:)
		keyEquivalent:@"q"];
	[m addItem:i];
	[i release];

	[i0 setSubmenu:m];
	[i0 release];
	[m release];
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
	[win.ofs[0] setTitle:s];
	[win.ofs[1] setTitle:s];
	[[NSApp dockTile] setBadgeLabel:s];
	[s release];
}

void
setcursor(Cursor *cursor)
{
	[appdelegate
		performSelectorOnMainThread:@selector(callsetcursor0:)
		withObject:[[NSValue valueWithPointer:cursor] retain]
		waitUntilDone:YES];
}

void
setcursor0(Cursor *c)
{
	NSBitmapImageRep *r;
	NSImage *i;
	NSPoint p;
	int b;
	uchar *plane[5];

	qlock(&win.cursorl);

	if(win.cursor){
		[win.cursor release];
		win.cursor = nil;
	}

	if(c == nil && usebigarrow)
		c = &bigarrow;

	if(c){
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

		win.cursor = [[NSCursor alloc] initWithImage:i hotSpot:p];

		[win.cursor set];
		[i release];
		[r release];
	}

	qunlock(&win.cursorl);
	[WIN invalidateCursorRectsForView:win.content];
}
