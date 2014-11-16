/*
 * Cocoa's event loop must be in main thread.
 *
 * Unless otherwise stated, all coordinate systems
 * are bottom-left-based.
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
#include "bigarrow.h"
#include "glendapng.h"

AUTOFRAMEWORK(Cocoa)

#define LOG	if(0)NSLog
#define panic	sysfatal

int usegestures = 0;
int useliveresizing = 0;
int useoldfullscreen = 0;
int usebigarrow = 0;

static void setprocname(const char*);

/*
 * By default, devdraw ignores retina displays. A non-zero evironment variable
 * |devdrawretina| will override this.
 */
int devdrawretina = 0;

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	threadexitsall("usage");
}

@interface appdelegate : NSObject @end

void
threadmain(int argc, char **argv)
{
	char *envvar;

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
	
	setprocname(argv0);

	if (envvar = getenv("devdrawretina"))
		devdrawretina = atoi(envvar) > 0;

	if(OSX_VERSION < 100700)
		[NSAutoreleasePool new];

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp setDelegate:[appdelegate new]];
	[NSApp run];
}

#define WIN	win.ofs[win.isofs]

struct
{
	NSWindow	*ofs[2];	/* ofs[1] for old fullscreen; ofs[0] else */
	int			isofs;
	int			isnfs;
	NSView		*content;
	NSBitmapImageRep	*img;
	int			needimg;
	int			deferflush;
	NSCursor		*cursor;
	CGFloat		topointscale;
	CGFloat		topixelscale;
} win;

struct
{
	NSCursor	*bigarrow;
	int		kbuttons;
	int		mbuttons;
	NSPoint	mpos;
	int		mscroll;
	int		willactivate;
} in;

static void hidebars(int);
static void flushimg(NSRect);
static void autoflushwin(int);
static void flushwin(void);
static void followzoombutton(NSRect);
static void getmousepos(void);
static void makeicon(void);
static void makemenu(void);
static void makewin(char*);
static void sendmouse(void);
static void setcursor0(Cursor*);
static void togglefs(void);
static void acceptresizing(int);

static NSCursor* makecursor(Cursor*);

static NSSize winsizepixels();
static NSSize winsizepoints();
static NSRect scalerect(NSRect, CGFloat);
static NSPoint scalepoint(NSPoint, CGFloat);
static NSRect dilate(NSRect);

@implementation appdelegate
- (void)applicationDidFinishLaunching:(id)arg
{
	in.bigarrow = makecursor(&bigarrow);
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
}
- (void)windowWillStartLiveResize:(id)arg
{
	if(useliveresizing == 0)
		[win.content setHidden:YES];
}
- (void)windowDidEndLiveResize:(id)arg
{
	if(useliveresizing == 0)
		[win.content setHidden:NO];
}
- (void)windowDidChangeScreen:(id)arg
{
	if(win.isnfs || win.isofs)
		hidebars(1);
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
- (void)applicationDidBecomeActive:(id)arg{ in.willactivate = 0;}
- (void)windowWillEnterFullScreen:(id)arg{ acceptresizing(1);}
- (void)windowDidEnterFullScreen:(id)arg{ win.isnfs = 1; hidebars(1);}
- (void)windowWillExitFullScreen:(id)arg{ win.isnfs = 0; hidebars(0);}
- (void)windowDidExitFullScreen:(id)arg
{
	NSButton *b;

	b = [WIN standardWindowButton:NSWindowMiniaturizeButton];

	if([b isEnabled] == 0){
		[b setEnabled:YES];
		hidebars(0);
	}
}
- (void)windowWillClose:(id)arg
{
	autoflushwin(0);	/* can crash otherwise */
}

+ (void)callservep9p:(id)arg
{
	servep9p();
	[NSApp terminate:self];
}
- (void)plumbmanual:(id)arg
{
	if(fork() != 0)
		return;
	execl("plumb", "plumb", "devdraw(1)", nil);
}
+ (void)callflushwin:(id)arg{ flushwin();}
- (void)calltogglefs:(id)arg{ togglefs();}

+ (void)callflushimg:(NSValue*)v{ flushimg([v rectValue]);}
+ (void)callmakewin:(NSValue*)v{ makewin([v pointerValue]);}
+ (void)callsetcursor0:(NSValue*)v{ setcursor0([v pointerValue]);}
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
	if(strcmp(label, "page") == 0)
		useliveresizing = 1;

	/*
	 * Create window in main thread, else no cursor
	 * change while resizing.
	 */
	[appdelegate
		performSelectorOnMainThread:@selector(callmakewin:)
		withObject:[NSValue valueWithPointer:winsize]
		waitUntilDone:YES];
//	makewin(winsize);

	kicklabel(label);
	return initimg();
}

@interface appwin : NSWindow @end
@interface contentview : NSView @end

@implementation appwin
- (NSTimeInterval)animationResizeTime:(NSRect)r
{
	return 0;
}
- (BOOL)canBecomeKeyWindow
{
	return YES;	/* else no keyboard for old fullscreen */
}
- (void)makeKeyAndOrderFront:(id)arg
{
	LOG(@"makeKeyAndOrderFront");

	autoflushwin(1);
	[win.content setHidden:NO];
	[super makeKeyAndOrderFront:arg];
}
- (void)miniaturize:(id)arg
{
	[super miniaturize:arg];
	[NSApp hide:nil];

	[win.content setHidden:YES];
	autoflushwin(0);
}
- (void)deminiaturize:(id)arg
{
	autoflushwin(1);
	[win.content setHidden:NO];
	[super deminiaturize:arg];
}
@end

double
min(double a, double b)
{
	return a<b? a : b;
}

enum
{
	Winstyle = NSTitledWindowMask
		| NSClosableWindowMask
		| NSMiniaturizableWindowMask
		| NSResizableWindowMask
};

static void
makewin(char *s)
{
	NSRect r, sr;
	NSWindow *w;
	Rectangle wr;
	int i, set;

	sr = [[NSScreen mainScreen] frame];
	r = [[NSScreen mainScreen] visibleFrame];

	if(s && *s){
		if(parsewinsize(s, &wr, &set) < 0)
			sysfatal("%r");
	}else{
		wr = Rect(0, 0, sr.size.width*2/3, sr.size.height*2/3);
		set = 0;
	}

	r.origin.x = wr.min.x;
	r.origin.y = sr.size.height-wr.max.y;	/* winsize is top-left-based */
	r.size.width = min(Dx(wr), r.size.width);
	r.size.height = min(Dy(wr), r.size.height);
	r = [NSWindow contentRectForFrameRect:r
		styleMask:Winstyle];

	w = [[appwin alloc]
		initWithContentRect:r
		styleMask:Winstyle
		backing:NSBackingStoreBuffered defer:NO];
	[w setTitle:@"devdraw"];

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
	win.content = [contentview new];
	[WIN setContentView:win.content];

	topwin();
}

static Memimage*
initimg(void)
{
	Memimage *i;
	NSSize size, ptsize;
	Rectangle r;

	size = winsizepixels();
	LOG(@"initimg %.0f %.0f", size.width, size.height);

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
	ptsize = winsizepoints();
	[win.img setSize: ptsize];
	win.topixelscale = size.width / ptsize.width;
	win.topointscale = 1.0f / win.topixelscale;
	
	// NOTE: This is not really the display DPI.
	// On retina, topixelscale is 2; otherwise it is 1.
	// This formula gives us 220 for retina, 110 otherwise.
	// That's not quite right but it's close to correct.
	// http://en.wikipedia.org/wiki/List_of_displays_by_pixel_density#Apple
	displaydpi = win.topixelscale * 110;

	return i;
}

void
resizeimg(void)
{
	[win.img release];
	_drawreplacescreenimage(initimg());

	mouseresized = 1;
	sendmouse();
}

static void
waitimg(int msec)
{
	NSDate *limit;
	int n;

	win.needimg = 1;
	win.deferflush = 0;

	n = 0;
	limit = [NSDate dateWithTimeIntervalSinceNow:msec/1000.0];
	do{
		[[NSRunLoop currentRunLoop]
			runMode:@"waiting image"
			beforeDate:limit];
		n++;
	}while(win.needimg && [(NSDate*)[NSDate date] compare:limit]<0);

	win.deferflush = win.needimg;

	LOG(@"waitimg %s (%d loop)", win.needimg?"defer":"ok", n);
}

void
_flushmemscreen(Rectangle r)
{
	static int n;
	NSRect rect;

	LOG(@"_flushmemscreen");

	if(n==0){
		n++;
		return;	/* to skip useless white init rect */
	}else
	if(n==1){
		[WIN performSelectorOnMainThread:
			@selector(makeKeyAndOrderFront:)
			withObject:nil
			waitUntilDone:NO];
		n++;
	}else
	if([win.content canDraw] == 0)
		return;

	rect = NSMakeRect(r.min.x, r.min.y, Dx(r), Dy(r));
	[appdelegate
		performSelectorOnMainThread:@selector(callflushimg:)
		withObject:[NSValue valueWithRect:rect]
		waitUntilDone:YES
		modes:[NSArray arrayWithObjects:
			NSRunLoopCommonModes,
			@"waiting image", nil]];
}

static void drawimg(NSRect, uint);
static void drawresizehandle(void);

enum
{
	Pixel = 1,
	Barsize = 4*Pixel,
	Cornersize = 3*Pixel,
	Handlesize = 3*Barsize + 1*Pixel,
};

/*
 * |rect| is in pixel coordinates.
 */
static void
flushimg(NSRect rect)
{
	NSRect dr, r;

	if([win.content lockFocusIfCanDraw] == 0)
		return;

	if(win.needimg){
		if(!NSEqualSizes(scalerect(rect, win.topointscale).size, [win.img size])){
			LOG(@"flushimg reject %.0f %.0f",
				rect.size.width, rect.size.height);
			[win.content unlockFocus];
			return;
		}
		win.needimg = 0;
	}else
		win.deferflush = 1;

	LOG(@"flushimg ok %.0f %.0f", rect.size.width, rect.size.height);

	/*
	 * Unless we are inside "drawRect", we have to round
	 * the corners ourselves, if this is the custom.
	 * "NSCompositeSourceIn" can do that, but we don't
	 * apply it to the whole rectangle, because this
	 * slows down trackpad scrolling considerably in
	 * Acme.
	 */
	r = [win.content bounds];
	rect = dilate(scalerect(rect, win.topointscale));
	r.size.height -= Cornersize;
	dr = NSIntersectionRect(r, rect);
	LOG(@"r %.0f %.0f", r.origin.x, r.origin.y, rect.size.width, rect.size.height);
	LOG(@"rect in points %f %f %.0f %.0f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
	LOG(@"dr in points %f %f %.0f %.0f", dr.origin.x, dr.origin.y, dr.size.width, dr.size.height);
	drawimg(dr, NSCompositeCopy);

	r.origin.y = r.size.height;
	r.size = NSMakeSize(Cornersize, Cornersize);
	dr = NSIntersectionRect(r, rect);
	drawimg(dr, NSCompositeSourceIn);

	r.origin.x = [win.img size].width - Cornersize;
	dr = NSIntersectionRect(r, rect);
	drawimg(dr, NSCompositeSourceIn);

	r.size.width = r.origin.x - Cornersize;
	r.origin.x -= r.size.width;
	dr = NSIntersectionRect(r, rect);
	drawimg(dr, NSCompositeCopy);

	if(OSX_VERSION<100700 && win.isofs==0){
		r.origin.x = [win.img size].width - Handlesize;
		r.origin.y = [win.img size].height - Handlesize;
		r.size = NSMakeSize(Handlesize, Handlesize);
		if(NSIntersectsRect(r, rect))
			drawresizehandle();
	}
	[win.content unlockFocus];
}

static void
autoflushwin(int set)
{
	static NSTimer *t;

	if(set){
		if(t)
			return;
		/*
		 * We need "NSRunLoopCommonModes", otherwise the
		 * timer will not fire during live resizing.
		 */
		t = [NSTimer
			timerWithTimeInterval:0.033
			target:[appdelegate class]
			selector:@selector(callflushwin:) userInfo:nil
			repeats:YES];
		[[NSRunLoop currentRunLoop] addTimer:t
			forMode:NSRunLoopCommonModes];
	}else{
		[t invalidate];
		t = nil;
		win.deferflush = 0;
	}
}

static void
flushwin(void)
{
	if(win.deferflush && win.needimg==0){
		[WIN flushWindow];
		win.deferflush = 0;
	}
}

/*
 * |dr| is sized in points. What if I make it pixels?
 */
static void
drawimg(NSRect dr, uint op)
{
	CGContextRef c;
	CGImageRef i;
	NSRect sr;

	if(NSIsEmptyRect(dr))
		return;

	sr =  [win.content convertRect:dr fromView:nil];
	LOG(@"before dr: %f %f %f %f\n", dr.origin.x, dr.origin.y, dr.size.width, dr.size.height);
	LOG(@"before sr: %f %f %f %f\n", sr.origin.x, sr.origin.y, sr.size.width, sr.size.height);

	dr = scalerect(dr, win.topixelscale);
	sr = scalerect(sr, win.topixelscale);

	LOG(@"dr: %f %f %f %f\n", dr.origin.x, dr.origin.y, dr.size.width, dr.size.height);
	LOG(@"sr: %f %f %f %f\n", sr.origin.x, sr.origin.y, sr.size.width, sr.size.height);
	if(OSX_VERSION >= 100800){
		i = CGImageCreateWithImageInRect([win.img CGImage], NSRectToCGRect(dr));
		c = [[WIN graphicsContext] graphicsPort];

		CGContextSaveGState(c);
		if(op == NSCompositeSourceIn)
			CGContextSetBlendMode(c, kCGBlendModeSourceIn);
                        LOG(@"wim.img size %f %f\n", [win.img size].width, [win.img size].height);
		CGContextTranslateCTM(c, 0, [win.img size].height);
		CGContextScaleCTM(c, win.topointscale, -win.topointscale);
		CGContextDrawImage(c, NSRectToCGRect(sr), i);
		CGContextRestoreGState(c);

		CGImageRelease(i);
	}else{
		[win.img drawInRect:dr fromRect:sr
			operation:op fraction:1
			respectFlipped:YES hints:nil];
	}
//	NSFrameRect(dr);
}

static void
drawresizehandle(void)
{
	NSColor *color[Barsize];
	NSPoint a,b;
	Point c;
	int i,j;

	c = Pt([win.img size].width, [win.img size].height);

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

static void getgesture(NSEvent*);
static void getkeyboard(NSEvent*);
static void getmouse(NSEvent*);
static void gettouch(NSEvent*, int);
static void updatecursor(void);

@implementation contentview
/*
 * "drawRect" is called each time Cocoa needs an
 * image, and each time we call "display".  It is
 * preceded by background painting, and followed by
 * "flushWindow".
 */
- (void)drawRect:(NSRect)r
{
	static int first = 1;

	LOG(@"drawrect %.0f %.0f %.0f %.0f",
		r.origin.x, r.origin.y, r.size.width, r.size.height);

	if(first)
		first = 0;
	else
		resizeimg();

	if([WIN inLiveResize])
		waitimg(100);
	else
		waitimg(500);
}
- (BOOL)isFlipped
{
	return YES;	/* to make the content's origin top left */
}
- (BOOL)acceptsFirstResponder
{
	return YES;	/* else no keyboard */
}
- (id)initWithFrame:(NSRect)r
{
	[super initWithFrame:r];
	[self setAcceptsTouchEvents:YES];
	[self setHidden:YES];		/* to avoid early "drawRect" call */
	return self;
}
- (void)setHidden:(BOOL)set
{
	if(!set)
		[WIN makeFirstResponder:self];	/* for keyboard focus */
	[super setHidden:set];
}
- (void)cursorUpdate:(NSEvent*)e{ updatecursor();}

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

@interface apptext : NSTextView @end

@implementation apptext
- (void)doCommandBySelector:(SEL)s{}	/* Esc key beeps otherwise */
- (void)insertText:(id)arg{}	/* to avoid a latency after some time */
@end

static void
interpretdeadkey(NSEvent *e)
{
	static apptext *t;

	if(t == nil)
		t = [apptext new];
	[t interpretKeyEvents:[NSArray arrayWithObject:e]];
}

static void
getkeyboard(NSEvent *e)
{
	static int omod;
	NSString *s;
	char c;
	int k, m;
	uint code;

	m = [e modifierFlags];

	switch([e type]){
	case NSKeyDown:
		s = [e characters];
		c = [s UTF8String][0];

		interpretdeadkey(e);

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
		if(m&NSAlternateKeyMask && (omod&NSAlternateKeyMask)==0)
			keystroke(Kalt);
		break;

	default:
		panic("getkey: unexpected event type");
	}
	omod = m;
}

/*
 * Devdraw does not use NSTrackingArea, that often
 * forgets to update the cursor on entering and on
 * leaving the area, and that sometimes stops sending
 * us MouseMove events, at least on OS X Lion.
 */
static void
updatecursor(void)
{
	NSCursor *c;
	int isdown, isinside;

	isinside = NSPointInRect(in.mpos, [win.content bounds]);
	isdown = (in.mbuttons || in.kbuttons);

	if(win.cursor && (isinside || isdown))
		c = win.cursor;
	else if(isinside && usebigarrow)
		c = in.bigarrow;
	else
		c = [NSCursor arrowCursor];
	[c set];

	/*
	 * Without this trick, we can come back from the dock
	 * with a resize cursor.
	 */
	if(OSX_VERSION >= 100700)
		[NSCursor unhide];
}

static void
acceptresizing(int set)
{
	uint old, style;

	old = [WIN styleMask];

	if((old | NSResizableWindowMask) != Winstyle)
		return;	/* when entering new fullscreen */

	if(set)
		style = Winstyle;
	else
		style = Winstyle & ~NSResizableWindowMask;

	if(style != old)
		[WIN setStyleMask:style];
}

static void
getmousepos(void)
{
	NSPoint p, q;

	p = [WIN mouseLocationOutsideOfEventStream];
	q = [win.content convertPoint:p fromView:nil];

	/* q is in point coordinates. in.mpos is in pixels. */
	q = scalepoint(q, win.topixelscale);

	in.mpos.x = round(q.x);
	in.mpos.y = round(q.y);

	updatecursor();

	if(win.isnfs || win.isofs)
		hidebars(1);
	else if(OSX_VERSION>=100700 && [WIN inLiveResize]==0){
		if(p.x<12 && p.y<12 && p.x>2 && p.y>2)
			acceptresizing(0);
		else
			acceptresizing(1);
	}
}

static void
getmouse(NSEvent *e)
{
	float d;
	int b, m;

	if([WIN isKeyWindow] == 0)
		return;

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
				abortcompose();
				b = 2;
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

#define Minpinch	0.02

static void
getgesture(NSEvent *e)
{
	switch([e type]){
	case NSEventTypeMagnify:
		if(fabs([e magnification]) > Minpinch)
			togglefs();
		break;
	}
}

static void sendclick(int);

static uint
msec(void)
{
	return nsec()/1000000;
}

static void
gettouch(NSEvent *e, int type)
{
	static int tapping;
	static uint taptime;
	NSSet *set;
	int p;

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
		break;

	case NSTouchPhaseMoved:
		tapping = 0;
		break;

	case NSTouchPhaseEnded:
		p = NSTouchPhaseTouching;
		set = [e touchesMatchingPhase:p inView:nil];
		if(set.count == 0){
			if(tapping && msec()-taptime<400)
				sendclick(2);
			tapping = 0;
		}
		break;

	case NSTouchPhaseCancelled:
		break;

	default:
		panic("gettouch: unexpected event type");
	}
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
sendmouse(void)
{
	NSSize size;
	int b;

	size = winsizepixels();
	mouserect = Rect(0, 0, size.width, size.height);

	b = in.kbuttons | in.mbuttons | in.mscroll;
	mousetrack(in.mpos.x, in.mpos.y, b, msec());
	in.mscroll = 0;
}

/*
 * |p| is in pixels.
 */
void
setmouse(Point p)
{
	static int first = 1;
	NSPoint q;
	NSRect r;

	if([NSApp isActive]==0 && in.willactivate==0)
		return;

	if(first){
		/* Try to move Acme's scrollbars without that! */
		CGSetLocalEventsSuppressionInterval(0);
		first = 0;
	}
	if([WIN inLiveResize])
		return;

	in.mpos = scalepoint(NSMakePoint(p.x, p.y), win.topointscale);	// race condition

	q = [win.content convertPoint:in.mpos toView:nil];
	q = [WIN convertBaseToScreen:q];

	r = [[[NSScreen screens] objectAtIndex:0] frame];
	q.y = r.size.height - q.y;	/* Quartz is top-left-based here */

	CGWarpMouseCursorPosition(NSPointToCGPoint(q));
}

/*
 *  |r| is in points.
 */
static void
followzoombutton(NSRect r)
{
	NSRect wr;
	Point p;
	NSPoint pt;

	wr = [WIN frame];
	wr.origin.y += wr.size.height;
	r.origin.y += r.size.height;

	getmousepos();
	pt.x = in.mpos.x;
	pt.y = in.mpos.y;
	pt = scalepoint(pt, win.topointscale);
	pt.x = (r.origin.x - wr.origin.x) + pt.x;
	pt.y = -(r.origin.y - wr.origin.y) + pt.y;
	pt = scalepoint(pt, win.topixelscale);

	p.x = pt.x;
	p.y = pt.y;

	setmouse(p);
}

static void
togglefs(void)
{
	uint opt, tmp;

#if OSX_VERSION >= 100700
	NSScreen *s, *s0;
	
	s = [WIN screen];
	s0 = [[NSScreen screens] objectAtIndex:0];
	
	if((s==s0 && useoldfullscreen==0) || win.isnfs) {
		[WIN toggleFullScreen:nil];
		return;
	}
#endif
	[win.content retain];
	[WIN orderOut:nil];
	[WIN setContentView:nil];

	win.isofs = ! win.isofs;
	hidebars(win.isofs);

	/*
	 * If we move the window from one space to another,
	 * ofs[0] and ofs[1] can be on different spaces.
	 * This "setCollectionBehavior" trick moves the
	 * window to the active space.
	 */
	opt = [WIN collectionBehavior];
	tmp = opt | NSWindowCollectionBehaviorCanJoinAllSpaces;
	[WIN setContentView:win.content];
	[WIN setCollectionBehavior:tmp];
	[WIN makeKeyAndOrderFront:nil];
	[WIN setCollectionBehavior:opt];
	[win.content release];
}

enum
{
	Autohiddenbars = NSApplicationPresentationAutoHideDock
		| NSApplicationPresentationAutoHideMenuBar,

	Hiddenbars = NSApplicationPresentationHideDock
		| NSApplicationPresentationHideMenuBar,
};

static void
hidebars(int set)
{
	NSScreen *s,*s0;
	uint old, opt;

	s = [WIN screen];
	s0 = [[NSScreen screens] objectAtIndex:0];
	old = [NSApp presentationOptions];

#if OSX_VERSION >= 100700
	/* This bit can get lost, resulting in dreadful bugs. */
	if(win.isnfs)
		old |= NSApplicationPresentationFullScreen;
#endif

	if(set && s==s0)
		opt = (old & ~Autohiddenbars) | Hiddenbars;
	else
		opt = old & ~(Autohiddenbars | Hiddenbars);

	if(opt != old)
		[NSApp setPresentationOptions:opt];
}

static void
makemenu(void)
{
	NSMenu *m;
	NSMenuItem *i0,*i1;

	m = [NSMenu new];
	i0 = [m addItemWithTitle:@"app" action:NULL keyEquivalent:@""];
	i1 = [m addItemWithTitle:@"help" action:NULL keyEquivalent:@""];
	[NSApp setMainMenu:m];
	[m release];

	m = [[NSMenu alloc] initWithTitle:@"app"];
	[m addItemWithTitle:@"Full Screen"
		action:@selector(calltogglefs:)
		keyEquivalent:@"f"];
	[m addItemWithTitle:@"Hide"
		action:@selector(hide:)
		keyEquivalent:@"h"];
	[m addItemWithTitle:@"Quit"
		action:@selector(terminate:)
		keyEquivalent:@"q"];
	[i0 setSubmenu:m];
	[m release];

	m = [[NSMenu alloc] initWithTitle:@"help"];
	[m addItemWithTitle:@"Plumb devdraw(1)"
		action:@selector(plumbmanual:)
		keyEquivalent:@""];
	[i1 setSubmenu:m];
	[m release];
}

// FIXME: Introduce a high-resolution Glenda image.
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
setcursor(Cursor *c)
{
	/*
	 * No cursor change unless in main thread.
	 */
	[appdelegate
		performSelectorOnMainThread:@selector(callsetcursor0:)
		withObject:[NSValue valueWithPointer:c]
		waitUntilDone:YES];
}

static void
setcursor0(Cursor *c)
{
	NSCursor *d;

	d = win.cursor;

	if(c)
		win.cursor = makecursor(c);
	else
		win.cursor = nil;

	updatecursor();

	if(d)
		[d release];
}

/*
 * Cursors will be scaled on retina display.
 */
static NSCursor*
makecursor(Cursor *c)
{
	NSBitmapImageRep *r;
	NSCursor *d;
	NSImage *i;
	NSPoint p;
	int b;
	uchar *plane[5];

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
	[r release];

	d = [[NSCursor alloc] initWithImage:i hotSpot:p];
	[i release];
	return d;
}

void
topwin(void)
{
	[WIN performSelectorOnMainThread:
		@selector(makeKeyAndOrderFront:)
		withObject:nil
		waitUntilDone:NO];

	in.willactivate = 1;
	[NSApp activateIgnoringOtherApps:YES];
}

static NSSize
winsizepoints()
{
    return [win.content bounds].size;
}

static NSSize
winsizepixels()
{
#if OSX_VERSION >= 100700
	if (OSX_VERSION >= 100700 && devdrawretina)
		return [win.content convertSizeToBacking: winsizepoints()];
	else
#endif
		return winsizepoints();
}

static NSRect
scalerect(NSRect r, CGFloat scale)
{
	r.origin.x *= scale;
	r.origin.y *= scale;
	r.size.width *= scale;
	 r.size.height *= scale;
	 return r;
}

/*
 * Expands rectangle |r|'s bounds to more inclusive integer bounds to
 * eliminate 1 pixel gaps.
 */
static NSRect
dilate(NSRect r)
{
	if(win.topixelscale > 1.0f){
		r.origin.x = floorf(r.origin.x);
		r.origin.y = floorf(r.origin.y);
		r.size.width = ceilf(r.size.width + 0.5);
		r.size.height = ceilf(r.size.height + 0.5);
	}
	return r;
}

static NSPoint
scalepoint(NSPoint pt, CGFloat scale)
{
    pt.x *= scale;
    pt.y *= scale;
    return pt;
}

static void
setprocname(const char *s)
{
  CFStringRef process_name;
  
  process_name = CFStringCreateWithBytes(nil, (uchar*)s, strlen(s), kCFStringEncodingUTF8, false);

  // Adapted from Chrome's mac_util.mm.
  // http://src.chromium.org/viewvc/chrome/trunk/src/base/mac/mac_util.mm
  //
  // Copyright (c) 2012 The Chromium Authors. All rights reserved.
  //
  // Redistribution and use in source and binary forms, with or without
  // modification, are permitted provided that the following conditions are
  // met:
  //
  //    * Redistributions of source code must retain the above copyright
  // notice, this list of conditions and the following disclaimer.
  //    * Redistributions in binary form must reproduce the above
  // copyright notice, this list of conditions and the following disclaimer
  // in the documentation and/or other materials provided with the
  // distribution.
  //    * Neither the name of Google Inc. nor the names of its
  // contributors may be used to endorse or promote products derived from
  // this software without specific prior written permission.
  //
  // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  // "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  // LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  // A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  // OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  // LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  // DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  // (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  // OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  // Warning: here be dragons! This is SPI reverse-engineered from WebKit's
  // plugin host, and could break at any time (although realistically it's only
  // likely to break in a new major release).
  // When 10.7 is available, check that this still works, and update this
  // comment for 10.8.

  // Private CFType used in these LaunchServices calls.
  typedef CFTypeRef PrivateLSASN;
  typedef PrivateLSASN (*LSGetCurrentApplicationASNType)();
  typedef OSStatus (*LSSetApplicationInformationItemType)(int, PrivateLSASN,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);

  static LSGetCurrentApplicationASNType ls_get_current_application_asn_func =
      NULL;
  static LSSetApplicationInformationItemType
      ls_set_application_information_item_func = NULL;
  static CFStringRef ls_display_name_key = NULL;

  static bool did_symbol_lookup = false;
  if (!did_symbol_lookup) {
    did_symbol_lookup = true;
    CFBundleRef launch_services_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
    if (!launch_services_bundle) {
      fprint(2, "Failed to look up LaunchServices bundle\n");
      return;
    }

    ls_get_current_application_asn_func =
        (LSGetCurrentApplicationASNType)(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle, CFSTR("_LSGetCurrentApplicationASN")));
    if (!ls_get_current_application_asn_func)
      fprint(2, "Could not find _LSGetCurrentApplicationASN\n");

    ls_set_application_information_item_func =
        (LSSetApplicationInformationItemType)(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle,
                CFSTR("_LSSetApplicationInformationItem")));
    if (!ls_set_application_information_item_func)
      fprint(2, "Could not find _LSSetApplicationInformationItem\n");

    CFStringRef* key_pointer = (CFStringRef*)(
        CFBundleGetDataPointerForName(launch_services_bundle,
                                      CFSTR("_kLSDisplayNameKey")));
    ls_display_name_key = key_pointer ? *key_pointer : NULL;
    if (!ls_display_name_key)
      fprint(2, "Could not find _kLSDisplayNameKey\n");

    // Internally, this call relies on the Mach ports that are started up by the
    // Carbon Process Manager.  In debug builds this usually happens due to how
    // the logging layers are started up; but in release, it isn't started in as
    // much of a defined order.  So if the symbols had to be loaded, go ahead
    // and force a call to make sure the manager has been initialized and hence
    // the ports are opened.
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
  }
  if (!ls_get_current_application_asn_func ||
      !ls_set_application_information_item_func ||
      !ls_display_name_key) {
    return;
  }

  PrivateLSASN asn = ls_get_current_application_asn_func();
  // Constant used by WebKit; what exactly it means is unknown.
  const int magic_session_constant = -2;
  OSErr err =
      ls_set_application_information_item_func(magic_session_constant, asn,
                                               ls_display_name_key,
                                               process_name,
                                               NULL /* optional out param */);
  if(err != noErr)
    fprint(2, "Call to set process name failed\n");
}
