#define Cursor OSXCursor
#define Point OSXPoint
#define Rect OSXRect

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#undef Cursor
#undef Point
#undef Rect

#include <u.h>
#include <libc.h>
#include <thread.h>
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
AUTOFRAMEWORK(Metal)
AUTOFRAMEWORK(QuartzCore)

#define LOG	if(0)NSLog

static void setprocname(const char*);
static uint keycvt(uint);
static uint msec(void);
static Memimage* initimg(void);

void
usage(void)
{
	fprint(2, "usage: devdraw (don't run directly)\n");
	threadexitsall("usage");
}


@interface AppDelegate : NSObject<NSApplicationDelegate,NSWindowDelegate>
+ (void)makewin:(NSValue *)v;
+ (void)callkicklabel:(NSString *)v;
+ (void)callsetNeedsDisplayInRect:(NSValue *)v;
+ (void)callsetcursor:(NSValue *)v;
@end
@interface DevDrawView : NSView<NSTextInputClient>
- (void)clearInput;
- (void)getmouse:(NSEvent *)e;
- (void)sendmouse:(NSUInteger)b;
- (void)resetLastInputRect;
- (void)enlargeLastInputRect:(NSRect)r;
@end
@interface DrawLayer : CAMetalLayer
@end

static AppDelegate *myApp = NULL;
static DevDrawView *myContent = NULL;
static NSWindow *win = NULL;
static NSCursor *currentCursor = NULL;

static DrawLayer *layer;
static id<MTLDevice> device;
static id<MTLCommandQueue> commandQueue;
static id<MTLTexture> texture;

static Memimage *img = NULL;

static QLock snarfl;

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
	case 'f':		/* fall through for backward compatibility */
	case 'g':
	case 'b':
		break;
	default:
		usage();
	}ARGEND

	setprocname(argv0);

	@autoreleasepool{
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		myApp = [AppDelegate new];
		[NSApp setDelegate:myApp];
		[NSApp run];
	}
}


void
callservep9p(void *v)
{
	USED(v);

	servep9p();
	[NSApp terminate:myApp];
}

@implementation AppDelegate

+ (void)makewin:(NSValue *)v
{
	NSRect r, sr;
	Rectangle wr;
	int set;
	char *s;
	NSArray *allDevices;

	const NSWindowStyleMask Winstyle = NSWindowStyleMaskTitled
		| NSWindowStyleMaskClosable
		| NSWindowStyleMaskMiniaturizable
		| NSWindowStyleMaskResizable;

	sr = [[NSScreen mainScreen] frame];
	r = [[NSScreen mainScreen] visibleFrame];

	s = [v pointerValue];
	LOG(@"makewin(%s)", s);
	if(s && *s){
		if(parsewinsize(s, &wr, &set) < 0)
			sysfatal("%r");
	}else{
		wr = Rect(0, 0, sr.size.width*2/3, sr.size.height*2/3);
		set = 0;
	}

	r.origin.x = wr.min.x;
	r.origin.y = sr.size.height-wr.max.y;	/* winsize is top-left-based */
	r.size.width = fmin(Dx(wr), r.size.width);
	r.size.height = fmin(Dy(wr), r.size.height);
	r = [NSWindow contentRectForFrameRect:r styleMask:Winstyle];

	win = [[NSWindow alloc]
		initWithContentRect:r
		styleMask:Winstyle
		backing:NSBackingStoreBuffered defer:NO];
	[win setTitle:@"devdraw"];

	if(!set)
		[win center];
	[win setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	[win setContentMinSize:NSMakeSize(64,64)];
	[win setOpaque:YES];
	[win setRestorable:NO];
	[win setAcceptsMouseMovedEvents:YES];
	[win setDelegate:myApp];

	myContent = [DevDrawView new];
	[win setContentView:myContent];
	[myContent setWantsLayer:YES];
	[myContent setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawOnSetNeedsDisplay];
	
	device = nil;
	allDevices = MTLCopyAllDevices();
	for(id mtlDevice in allDevices) {
		if ([mtlDevice isLowPower] && ![mtlDevice isRemovable]) {
			device = mtlDevice;
			break;
		}
	}
	if(!device)
		device = MTLCreateSystemDefaultDevice();

	commandQueue = [device newCommandQueue];

	layer = (DrawLayer *)[myContent layer];
	layer.device = device;
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = YES;
	layer.opaque = YES;

	// We use a default transparent layer on top of the CAMetalLayer.
	// This seems to make fullscreen applications behave.
	{
		CALayer *stub = [CALayer layer];
		stub.frame = CGRectMake(0, 0, 1, 1);
		[stub setNeedsDisplay];
		[layer addSublayer:stub];
	}

	[NSEvent setMouseCoalescingEnabled:NO];

	topwin();
}

+ (void)callkicklabel:(NSString *)s
{
	LOG(@"callkicklabel(%@)", s);
	[win setTitle:s];
	[[NSApp dockTile] setBadgeLabel:s];
}


+ (void)callsetNeedsDisplayInRect:(NSValue *)v
{
	NSRect r;
	dispatch_time_t time;

	r = [v rectValue];
	LOG(@"callsetNeedsDisplayInRect(%g, %g, %g, %g)", r.origin.x, r.origin.y, r.size.width, r.size.height);
	r = [win convertRectFromBacking:r];
	LOG(@"setNeedsDisplayInRect(%g, %g, %g, %g)", r.origin.x, r.origin.y, r.size.width, r.size.height);
	[layer setNeedsDisplayInRect:r];

	time = dispatch_time(DISPATCH_TIME_NOW, 16 * NSEC_PER_MSEC);
	dispatch_after(time, dispatch_get_main_queue(), ^(void){
		[layer setNeedsDisplayInRect:r];
	});

	[myContent enlargeLastInputRect:r];
}

typedef struct Cursors Cursors;
struct Cursors {
	Cursor *c;
	Cursor2 *c2;
};

+ (void)callsetcursor:(NSValue *)v
{
	Cursors *cs;
	Cursor *c;
	Cursor2 *c2;
	NSBitmapImageRep *r, *r2;
	NSImage *i;
	NSPoint p;
	uchar *plane[5], *plane2[5];
	uint b;

	cs = [v pointerValue];
	c = cs->c;
	if(!c)
		c = &bigarrow;
	c2 = cs->c2;
	if(!c2)
		c2 = &bigarrow2;

	r = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:nil
		pixelsWide:16
		pixelsHigh:16
		bitsPerSample:1
		samplesPerPixel:2
		hasAlpha:YES
		isPlanar:YES
		colorSpaceName:NSDeviceWhiteColorSpace
		bytesPerRow:2
		bitsPerPixel:0];
	[r getBitmapDataPlanes:plane];
	for(b=0; b<nelem(c->set); b++){
		plane[0][b] = ~c->set[b] & c->clr[b];
		plane[1][b] = c->set[b] | c->clr[b];
	}

	r2 = [[NSBitmapImageRep alloc]
		initWithBitmapDataPlanes:nil
		pixelsWide:32
		pixelsHigh:32
		bitsPerSample:1
		samplesPerPixel:2
		hasAlpha:YES
		isPlanar:YES
		colorSpaceName:NSDeviceWhiteColorSpace
		bytesPerRow:4
		bitsPerPixel:0];
	[r2 getBitmapDataPlanes:plane2];
	for(b=0; b<nelem(c2->set); b++){
		plane2[0][b] = ~c2->set[b] & c2->clr[b];
		plane2[1][b] = c2->set[b] | c2->clr[b];
	}

	// For checking out the cursor bitmap image
/*
	static BOOL saveimg = YES;
	if(saveimg){
		NSData *data = [r representationUsingType: NSBitmapImageFileTypeBMP properties: @{}];
		[data writeToFile: @"/tmp/r.bmp" atomically: NO];
		data = [r2 representationUsingType: NSBitmapImageFileTypeBMP properties: @{}];
		[data writeToFile: @"/tmp/r2.bmp" atomically: NO];
		saveimg = NO;
	}
*/

	i = [[NSImage alloc] initWithSize:NSMakeSize(16, 16)];
	[i addRepresentation:r2];
	[i addRepresentation:r];

	p = NSMakePoint(-c->offset.x, -c->offset.y);
	currentCursor = [[NSCursor alloc] initWithImage:i hotSpot:p];

	[win invalidateCursorRectsForView:myContent];
}

- (void)applicationDidFinishLaunching:(id)arg
{
	NSMenu *m, *sm;
	NSData *d;
	NSImage *i;

	LOG(@"applicationDidFinishLaunching");

	sm = [NSMenu new];
	[sm addItemWithTitle:@"Toggle Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
	[sm addItemWithTitle:@"Hide" action:@selector(hide:) keyEquivalent:@"h"];
	[sm addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
	m = [NSMenu new];
	[m addItemWithTitle:@"DEVDRAW" action:NULL keyEquivalent:@""];
	[m setSubmenu:sm forItem:[m itemWithTitle:@"DEVDRAW"]];
	[NSApp setMainMenu:m];

	d = [[NSData alloc] initWithBytes:glenda_png length:(sizeof glenda_png)];
	i = [[NSImage alloc] initWithData:d];
	[NSApp setApplicationIconImage:i];
	[[NSApp dockTile] display];

	proccreate(callservep9p, nil, 0);
}

- (NSApplicationPresentationOptions)window:(id)arg
		willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions {
	NSApplicationPresentationOptions o;
	o = proposedOptions;
	o &= ~(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar);
	o |= NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;
	return o;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
	return YES;
}

- (void)windowDidResize:(NSNotification *)notification
{
	if(![myContent inLiveResize] && img) {
		resizeimg();
	}
}

- (void)windowDidBecomeKey:(id)arg
{
        [myContent sendmouse:0];
}

@end

@implementation DevDrawView
{
	NSMutableString *_tmpText;
	NSRange _markedRange;
	NSRange _selectedRange;
	NSRect _lastInputRect;	// The view is flipped, this is not.
	BOOL _tapping;
	NSUInteger _tapFingers;
	NSUInteger _tapTime;
}

- (id)init
{
	LOG(@"View init");
	self = [super init];
	[self setAllowedTouchTypes:NSTouchTypeMaskDirect|NSTouchTypeMaskIndirect];
	_tmpText = [[NSMutableString alloc] initWithCapacity:2];
	_markedRange = NSMakeRange(NSNotFound, 0);
	_selectedRange = NSMakeRange(0, 0);
	return self;
}

- (CALayer *)makeBackingLayer
{
	LOG(@"makeBackingLayer");
	return [DrawLayer layer];
}

- (BOOL)wantsUpdateLayer
{
	return YES;
}

- (BOOL)isOpaque
{
	return YES;
}

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)mouseMoved:(NSEvent*)e{ [self getmouse:e];}
- (void)mouseDown:(NSEvent*)e{ [self getmouse:e];}
- (void)mouseDragged:(NSEvent*)e{ [self getmouse:e];}
- (void)mouseUp:(NSEvent*)e{ [self getmouse:e];}
- (void)otherMouseDown:(NSEvent*)e{ [self getmouse:e];}
- (void)otherMouseDragged:(NSEvent*)e{ [self getmouse:e];}
- (void)otherMouseUp:(NSEvent*)e{ [self getmouse:e];}
- (void)rightMouseDown:(NSEvent*)e{ [self getmouse:e];}
- (void)rightMouseDragged:(NSEvent*)e{ [self getmouse:e];}
- (void)rightMouseUp:(NSEvent*)e{ [self getmouse:e];}

- (void)scrollWheel:(NSEvent*)e
{
	NSInteger s;

	s = [e scrollingDeltaY];
	if(s > 0)
		[self sendmouse:8];
	else if (s < 0)
		[self sendmouse:16];
}

- (void)keyDown:(NSEvent*)e
{
	LOG(@"keyDown to interpret");

	[self interpretKeyEvents:[NSArray arrayWithObject:e]];

	[self resetLastInputRect];
}

- (void)flagsChanged:(NSEvent*)e
{
	static NSEventModifierFlags omod;
	NSEventModifierFlags m;
	uint b;

	LOG(@"flagsChanged");
	m = [e modifierFlags];

	b = [NSEvent pressedMouseButtons];
	b = (b&~6) | (b&4)>>1 | (b&2)<<1;
	if(b){
		if(m & ~omod & NSEventModifierFlagControl)
			b |= 1;
		if(m & ~omod & NSEventModifierFlagOption)
			b |= 2;
		if(m & ~omod & NSEventModifierFlagCommand)
			b |= 4;
		[self sendmouse:b];
	}else if(m & ~omod & NSEventModifierFlagOption)
		keystroke(Kalt);

	omod = m;
}

- (void)magnifyWithEvent:(NSEvent*)e
{
	if(fabs([e magnification]) > 0.02)
		[[self window] toggleFullScreen:nil];
}

- (void)touchesBeganWithEvent:(NSEvent*)e
{
	_tapping = YES;
	_tapFingers = [e touchesMatchingPhase:NSTouchPhaseTouching inView:nil].count;
	_tapTime = msec();
}
- (void)touchesMovedWithEvent:(NSEvent*)e
{
	_tapping = NO;
}
- (void)touchesEndedWithEvent:(NSEvent*)e
{
	if(_tapping
		&& [e touchesMatchingPhase:NSTouchPhaseTouching inView:nil].count == 0
		&& msec() - _tapTime < 250){
		switch(_tapFingers){
		case 3:
			[self sendmouse:2];
			[self sendmouse:0];
			break;
		case 4:
			[self sendmouse:2];
			[self sendmouse:1];
			[self sendmouse:0];
			break;
		}
		_tapping = NO;
	}
}
- (void)touchesCancelledWithEvent:(NSEvent*)e
{
	_tapping = NO;
}

- (void)getmouse:(NSEvent *)e
{
	NSUInteger b;
	NSEventModifierFlags m;

	b = [NSEvent pressedMouseButtons];
	b = b&~6 | (b&4)>>1 | (b&2)<<1;
	b = mouseswap(b);

	if(b == 1){
		m = [e modifierFlags];
		if(m & NSEventModifierFlagOption){
			abortcompose();
			b = 2;
		}else
		if(m & NSEventModifierFlagCommand)
			b = 4;
	}
	[self sendmouse:b];
}

- (void)sendmouse:(NSUInteger)b
{
	NSPoint p;

	p = [self.window convertPointToBacking:
		[self.window mouseLocationOutsideOfEventStream]];
	p.y = Dy(mouserect) - p.y;
	// LOG(@"(%g, %g) <- sendmouse(%d)", p.x, p.y, (uint)b);
	mousetrack(p.x, p.y, b, msec());
	if(b && _lastInputRect.size.width && _lastInputRect.size.height)
		[self resetLastInputRect];
}

- (void)resetCursorRects {
	[super resetCursorRects];
	[self addCursorRect:self.bounds cursor:currentCursor];
}

- (void)viewDidEndLiveResize
{
	[super viewDidEndLiveResize];
	if(img)
		resizeimg();
}

- (void)viewDidChangeBackingProperties
{
	[super viewDidChangeBackingProperties];
	if(img)
		resizeimg();
}

// conforms to protocol NSTextInputClient
- (BOOL)hasMarkedText
{
	LOG(@"hasMarkedText");
	return _markedRange.location != NSNotFound;
}
- (NSRange)markedRange
{
	LOG(@"markedRange");
	return _markedRange;
}
- (NSRange)selectedRange
{
	LOG(@"selectedRange");
	return _selectedRange;
}
- (void)setMarkedText:(id)string
	selectedRange:(NSRange)sRange
	replacementRange:(NSRange)rRange
{
	NSString *str;

	LOG(@"setMarkedText: %@ (%ld, %ld) (%ld, %ld)", string,
		sRange.location, sRange.length,
		rRange.location, rRange.length);

	[self clearInput];

	if([string isKindOfClass:[NSAttributedString class]])
		str = [string string];
	else
		str = string;

	if(rRange.location == NSNotFound){
		if(_markedRange.location != NSNotFound){
			rRange = _markedRange;
		}else{
			rRange = _selectedRange;
		}
	}

	if(str.length == 0){
		[_tmpText deleteCharactersInRange:rRange];
		[self unmarkText];
	}else{
		_markedRange = NSMakeRange(rRange.location, str.length);
		[_tmpText replaceCharactersInRange:rRange withString:str];
	}
	_selectedRange.location = rRange.location + sRange.location;
	_selectedRange.length = sRange.length;

	if(_tmpText.length){
		uint i;
		LOG(@"text length %ld", _tmpText.length);
		for(i = 0; i <= _tmpText.length; ++i){
			if(i == _markedRange.location)
				keystroke('[');
			if(_selectedRange.length){
				if(i == _selectedRange.location)
					keystroke('{');
				if(i == NSMaxRange(_selectedRange))
					keystroke('}');
				}
			if(i == NSMaxRange(_markedRange))
				keystroke(']');
			if(i < _tmpText.length)
				keystroke([_tmpText characterAtIndex:i]);
		}
		int l;
		l = 1 + _tmpText.length - NSMaxRange(_selectedRange)
			+ (_selectedRange.length > 0);
		LOG(@"move left %d", l);
		for(i = 0; i < l; ++i)
			keystroke(Kleft);
	}

	LOG(@"text: \"%@\"  (%ld,%ld)  (%ld,%ld)", _tmpText,
		_markedRange.location, _markedRange.length,
		_selectedRange.location, _selectedRange.length);
}
- (void)unmarkText
{
	//NSUInteger i;
	NSUInteger len;

	LOG(@"unmarkText");
	len = [_tmpText length];
	//for(i = 0; i < len; ++i)
	//	keystroke([_tmpText characterAtIndex:i]);
	[_tmpText deleteCharactersInRange:NSMakeRange(0, len)];
	_markedRange = NSMakeRange(NSNotFound, 0);
	_selectedRange = NSMakeRange(0, 0);
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText
{
	LOG(@"validAttributesForMarkedText");
	return @[];
}
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r
	actualRange:(NSRangePointer)actualRange
{
	NSRange sr;
	NSAttributedString *s;

	LOG(@"attributedSubstringForProposedRange: (%ld, %ld) (%ld, %ld)",
		r.location, r.length, actualRange->location, actualRange->length);
	sr = NSMakeRange(0, [_tmpText length]);
	sr = NSIntersectionRange(sr, r);
	if(actualRange)
		*actualRange = sr;
	LOG(@"use range: %ld, %ld", sr.location, sr.length);
	s = nil;
	if(sr.length)
		s = [[NSAttributedString alloc]
			initWithString:[_tmpText substringWithRange:sr]];
	LOG(@"	return %@", s);
	return s;
}
- (void)insertText:(id)s
	replacementRange:(NSRange)r
{
	NSUInteger i;
	NSUInteger len;

	LOG(@"insertText: %@ replacementRange: %ld, %ld", s, r.location, r.length);

	[self clearInput];

	len = [s length];
	for(i = 0; i < len; ++i)
		keystroke([s characterAtIndex:i]);
	[_tmpText deleteCharactersInRange:NSMakeRange(0, _tmpText.length)];
	_markedRange = NSMakeRange(NSNotFound, 0);
	_selectedRange = NSMakeRange(0, 0);
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
	LOG(@"characterIndexForPoint: %g, %g", point.x, point.y);
	return 0;
}
- (NSRect)firstRectForCharacterRange:(NSRange)r
	actualRange:(NSRangePointer)actualRange
{
	LOG(@"firstRectForCharacterRange: (%ld, %ld) (%ld, %ld)",
		r.location, r.length, actualRange->location, actualRange->length);
	if(actualRange)
		*actualRange = r;
	return [[self window] convertRectToScreen:_lastInputRect];
}
- (void)doCommandBySelector:(SEL)s
{
	NSEvent *e;
	NSEventModifierFlags m;
	uint c, k;

	LOG(@"doCommandBySelector (%@)", NSStringFromSelector(s));

	e = [NSApp currentEvent];
	c = [[e characters] characterAtIndex:0];
	k = keycvt(c);
	LOG(@"keyDown: character0: 0x%x -> 0x%x", c, k);
	m = [e modifierFlags];

	if(m & NSEventModifierFlagCommand){
		if((m & NSEventModifierFlagShift) && 'a' <= k && k <= 'z')
			k += 'A' - 'a';
		if(' '<=k && k<='~')
			k += Kcmd;
	}
	if(k>0)
		keystroke(k);
}

// Helper for managing input rect approximately
- (void)resetLastInputRect
{
	LOG(@"resetLastInputRect");
	_lastInputRect.origin.x = 0.0;
	_lastInputRect.origin.y = 0.0;
	_lastInputRect.size.width = 0.0;
	_lastInputRect.size.height = 0.0;
}

- (void)enlargeLastInputRect:(NSRect)r
{
	r.origin.y = [self bounds].size.height - r.origin.y - r.size.height;
	_lastInputRect = NSUnionRect(_lastInputRect, r);
	LOG(@"update last input rect (%g, %g, %g, %g)",
		_lastInputRect.origin.x, _lastInputRect.origin.y,
		_lastInputRect.size.width, _lastInputRect.size.height);
}

- (void)clearInput
{
	if(_tmpText.length){
		uint i;
		int l;
		l = 1 + _tmpText.length - NSMaxRange(_selectedRange)
			+ (_selectedRange.length > 0);
		LOG(@"move right %d", l);
		for(i = 0; i < l; ++i)
			keystroke(Kright);
		l = _tmpText.length+2+2*(_selectedRange.length > 0);
		LOG(@"backspace %d", l);
		for(uint i = 0; i < l; ++i)
			keystroke(Kbs);
	}
}

@end

@implementation DrawLayer

- (void)display
{
	id<MTLCommandBuffer> cbuf;
	id<MTLBlitCommandEncoder> blit;

	LOG(@"display");

	cbuf = [commandQueue commandBuffer];

	LOG(@"display query drawable");

@autoreleasepool{
	id<CAMetalDrawable> drawable;

	drawable = [layer nextDrawable];
	if(!drawable){
		LOG(@"display couldn't get drawable");
		[self setNeedsDisplay];
		return;
	}

	LOG(@"display got drawable");

	blit = [cbuf blitCommandEncoder];
	[blit copyFromTexture:texture
		sourceSlice:0
		sourceLevel:0
		sourceOrigin:MTLOriginMake(0, 0, 0)
		sourceSize:MTLSizeMake(texture.width, texture.height, texture.depth)
		toTexture:drawable.texture
		destinationSlice:0
		destinationLevel:0
		destinationOrigin:MTLOriginMake(0, 0, 0)];
	[blit endEncoding];

	[cbuf presentDrawable:drawable];
	drawable = nil;
}
	[cbuf addCompletedHandler:^(id<MTLCommandBuffer> cmdBuff){
		if(cmdBuff.error){
			NSLog(@"command buffer finished with error: %@",
				cmdBuff.error.localizedDescription);
		}else
			LOG(@"command buffer finishes present drawable");
	}];
	[cbuf commit];

	LOG(@"display commit");
}

@end

static uint
msec(void)
{
	return nsec()/1000000;
}

static uint
keycvt(uint code)
{
	switch(code){
	case '\r': return '\n';
	case 127: return '\b';
	case NSUpArrowFunctionKey: return Kup;
	case NSDownArrowFunctionKey: return Kdown;
	case NSLeftArrowFunctionKey: return Kleft;
	case NSRightArrowFunctionKey: return Kright;
	case NSInsertFunctionKey: return Kins;
	case NSDeleteFunctionKey: return Kdel;
	case NSHomeFunctionKey: return Khome;
	case NSEndFunctionKey: return Kend;
	case NSPageUpFunctionKey: return Kpgup;
	case NSPageDownFunctionKey: return Kpgdown;
	case NSF1FunctionKey: return KF|1;
	case NSF2FunctionKey: return KF|2;
	case NSF3FunctionKey: return KF|3;
	case NSF4FunctionKey: return KF|4;
	case NSF5FunctionKey: return KF|5;
	case NSF6FunctionKey: return KF|6;
	case NSF7FunctionKey: return KF|7;
	case NSF8FunctionKey: return KF|8;
	case NSF9FunctionKey: return KF|9;
	case NSF10FunctionKey: return KF|10;
	case NSF11FunctionKey: return KF|11;
	case NSF12FunctionKey: return KF|12;
	case NSBeginFunctionKey:
	case NSPrintScreenFunctionKey:
	case NSScrollLockFunctionKey:
	case NSF13FunctionKey:
	case NSF14FunctionKey:
	case NSF15FunctionKey:
	case NSF16FunctionKey:
	case NSF17FunctionKey:
	case NSF18FunctionKey:
	case NSF19FunctionKey:
	case NSF20FunctionKey:
	case NSF21FunctionKey:
	case NSF22FunctionKey:
	case NSF23FunctionKey:
	case NSF24FunctionKey:
	case NSF25FunctionKey:
	case NSF26FunctionKey:
	case NSF27FunctionKey:
	case NSF28FunctionKey:
	case NSF29FunctionKey:
	case NSF30FunctionKey:
	case NSF31FunctionKey:
	case NSF32FunctionKey:
	case NSF33FunctionKey:
	case NSF34FunctionKey:
	case NSF35FunctionKey:
	case NSPauseFunctionKey:
	case NSSysReqFunctionKey:
	case NSBreakFunctionKey:
	case NSResetFunctionKey:
	case NSStopFunctionKey:
	case NSMenuFunctionKey:
	case NSUserFunctionKey:
	case NSSystemFunctionKey:
	case NSPrintFunctionKey:
	case NSClearLineFunctionKey:
	case NSClearDisplayFunctionKey:
	case NSInsertLineFunctionKey:
	case NSDeleteLineFunctionKey:
	case NSInsertCharFunctionKey:
	case NSDeleteCharFunctionKey:
	case NSPrevFunctionKey:
	case NSNextFunctionKey:
	case NSSelectFunctionKey:
	case NSExecuteFunctionKey:
	case NSUndoFunctionKey:
	case NSRedoFunctionKey:
	case NSFindFunctionKey:
	case NSHelpFunctionKey:
	case NSModeSwitchFunctionKey: return 0;
	default: return code;
	}
}

Memimage*
attachscreen(char *label, char *winsize)
{
	LOG(@"attachscreen(%s, %s)", label, winsize);
	[AppDelegate
		performSelectorOnMainThread:@selector(makewin:)
		withObject:[NSValue valueWithPointer:winsize]
		waitUntilDone:YES];
	kicklabel(label);
	setcursor(nil, nil);
	mouseresized = 0;
	return initimg();
}

static Memimage*
initimg(void)
{
@autoreleasepool{
	CGFloat scale;
	NSSize size;
	MTLTextureDescriptor *textureDesc;

	size = [myContent convertSizeToBacking:[myContent bounds].size];
	mouserect = Rect(0, 0, size.width, size.height);

	LOG(@"initimg %.0f %.0f", size.width, size.height);

	img = allocmemimage(mouserect, XRGB32);
	if(img == nil)
		panic("allocmemimage: %r");
	if(img->data == nil)
		panic("img->data == nil");

	textureDesc = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		width:size.width
		height:size.height
		mipmapped:NO];
	textureDesc.allowGPUOptimizedContents = YES;
	textureDesc.usage = MTLTextureUsageShaderRead;
	textureDesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
	texture = [device newTextureWithDescriptor:textureDesc];

	scale = [win backingScaleFactor];
	[layer setDrawableSize:size];
	[layer setContentsScale:scale];

	// NOTE: This is not really the display DPI.
	// On retina, scale is 2; otherwise it is 1.
	// This formula gives us 220 for retina, 110 otherwise.
	// That's not quite right but it's close to correct.
	// https://en.wikipedia.org/wiki/Retina_display#Models
	displaydpi = scale * 110;
}
	LOG(@"initimg return");

	return img;
}

void
_flushmemscreen(Rectangle r)
{
	LOG(@"_flushmemscreen(%d,%d,%d,%d)", r.min.x, r.min.y, Dx(r), Dy(r));
	if(!rectinrect(r, Rect(0, 0, texture.width, texture.height))){
		LOG(@"Rectangle is out of bounds, return.");
		return;
	}

	@autoreleasepool{
		[texture
			replaceRegion:MTLRegionMake2D(r.min.x, r.min.y, Dx(r), Dy(r))
			mipmapLevel:0
			withBytes:byteaddr(img, Pt(r.min.x, r.min.y))
			bytesPerRow:img->width*sizeof(u32int)];
		[AppDelegate
			performSelectorOnMainThread:@selector(callsetNeedsDisplayInRect:)
			withObject:[NSValue valueWithRect:NSMakeRect(r.min.x, r.min.y, Dx(r), Dy(r))]
			waitUntilDone:NO];
	}
}

void
setmouse(Point p)
{
	@autoreleasepool{
		NSPoint q;

		LOG(@"setmouse(%d,%d)", p.x, p.y);
		q = [win convertPointFromBacking:NSMakePoint(p.x, p.y)];
		LOG(@"(%g, %g) <- fromBacking", q.x, q.y);
		q = [myContent convertPoint:q toView:nil];
		LOG(@"(%g, %g) <- toWindow", q.x, q.y);
		q = [win convertPointToScreen:q];
		LOG(@"(%g, %g) <- toScreen", q.x, q.y);
		// Quartz has the origin of the "global display
		// coordinate space" at the top left of the primary
		// screen with y increasing downward, while Cocoa has
		// the origin at the bottom left of the primary screen
		// with y increasing upward.  We flip the coordinate
		// with a negative sign and shift upward by the height
		// of the primary screen.
		q.y = NSScreen.screens[0].frame.size.height - q.y;
		LOG(@"(%g, %g) <- setmouse", q.x, q.y);
		CGWarpMouseCursorPosition(NSPointToCGPoint(q));
		CGAssociateMouseAndMouseCursorPosition(true);
	}
}

char*
getsnarf(void)
{
	NSPasteboard *pb;
	NSString *s;

	@autoreleasepool{
		pb = [NSPasteboard generalPasteboard];

		qlock(&snarfl);
		s = [pb stringForType:NSPasteboardTypeString];
		qunlock(&snarfl);

		if(s)
			return strdup((char *)[s UTF8String]);
		else
			return nil;
	}
}

void
putsnarf(char *s)
{
	NSArray *t;
	NSPasteboard *pb;
	NSString *str;

	if(strlen(s) >= SnarfSize)
		return;

	@autoreleasepool{
		t = [NSArray arrayWithObject:NSPasteboardTypeString];
		pb = [NSPasteboard generalPasteboard];
		str = [[NSString alloc] initWithUTF8String:s];

		qlock(&snarfl);
		[pb declareTypes:t owner:nil];
		[pb setString:str forType:NSPasteboardTypeString];
		qunlock(&snarfl);
	}
}

void
kicklabel(char *label)
{
	NSString *s;

	LOG(@"kicklabel(%s)", label);
	if(label == nil)
		return;

	@autoreleasepool{
		s = [[NSString alloc] initWithUTF8String:label];
		[AppDelegate
			performSelectorOnMainThread:@selector(callkicklabel:)
			withObject:s
			waitUntilDone:NO];
	}
}

void
setcursor(Cursor *c, Cursor2 *c2)
{
	Cursors cs;
	
	cs.c = c;
	cs.c2 = c2;

	[AppDelegate
		performSelectorOnMainThread:@selector(callsetcursor:)
		withObject:[NSValue valueWithPointer:&cs]
		waitUntilDone:YES];
}

void
topwin(void)
{
	[win
		performSelectorOnMainThread:
		@selector(makeKeyAndOrderFront:)
		withObject:nil
		waitUntilDone:YES];

	[NSApp activateIgnoringOtherApps:YES];
}

void
resizeimg(void)
{
	zlock();
	_drawreplacescreenimage(initimg());

	mouseresized = 1;
	zunlock();
	[myContent sendmouse:0];
}

void
resizewindow(Rectangle r)
{
	LOG(@"resizewindow %d %d %d %d", r.min.x, r.min.y, Dx(r), Dy(r));
	dispatch_async(dispatch_get_main_queue(), ^(void){
		NSSize s;

		s = [myContent convertSizeFromBacking:NSMakeSize(Dx(r), Dy(r))];
		[win setContentSize:s];
	});
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
