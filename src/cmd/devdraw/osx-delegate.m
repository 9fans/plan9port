#define Point OSXPoint
#define Rect OSXRect
#define Cursor OSXCursor
#import "osx-delegate.h"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#undef Cursor
#undef Rect
#undef Point

#include <u.h>
#include <errno.h>
#include <sys/select.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>

AUTOFRAMEWORK(Foundation)
AUTOFRAMEWORK(AppKit)

extern int trace;

extern void fullscreen(int);
extern void kbdevent(NSEvent *event);
extern void mouseevent(NSEvent *event);
extern void eresized(int);

extern void runmsg(Wsysmsg *m);
extern void seticon();

@implementation DevdrawDelegate
+(void)populateMainMenu
{
	NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
	NSMenuItem *menuItem;
	NSMenu *submenu;

	menuItem = [mainMenu addItemWithTitle:@"Apple" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:@"Apple"];
	[NSApp performSelector:@selector(setAppleMenu:) withObject:submenu];
	[self populateApplicationMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];

	menuItem = [mainMenu addItemWithTitle:@"View" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", "@The View menu")];
	[self populateViewMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];

	menuItem = [mainMenu addItemWithTitle:@"Window" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", @"The Window menu")];
	[self populateWindowMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];
	[NSApp setWindowsMenu:submenu];

	menuItem = [mainMenu addItemWithTitle:@"Help" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", @"The Help menu")];
	[self populateHelpMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];

	[NSApp setMainMenu:mainMenu];
}

+(void)populateApplicationMenu:(NSMenu *)aMenu
{
	NSString *applicationName = [[NSProcessInfo processInfo] processName];
	NSMenuItem *menuItem;
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"About", nil), applicationName]
								action:@selector(orderFrontStandardAboutPanel:)
						 keyEquivalent:@""];
	[menuItem setTarget:NSApp];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Preferences...", nil)
								action:NULL
						 keyEquivalent:@","];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Services", nil)
								action:NULL
						 keyEquivalent:@""];
	NSMenu * servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
	[aMenu setSubmenu:servicesMenu forItem:menuItem];
	[NSApp setServicesMenu:servicesMenu];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"Hide", nil), applicationName]
								action:@selector(hide:)
						 keyEquivalent:@"h"];
	[menuItem setTarget:NSApp];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Hide Others", nil)
								action:@selector(hideOtherApplications:)
						 keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
	[menuItem setTarget:NSApp];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Show All", nil)
								action:@selector(unhideAllApplications:)
						 keyEquivalent:@""];
	[menuItem setTarget:NSApp];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"Quit", nil), applicationName]
								action:@selector(terminate:)
						 keyEquivalent:@"q"];
	[menuItem setTarget:NSApp];
}

+(void)populateViewMenu:(NSMenu *)aMenu
{
	NSMenuItem *menuItem;
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Full Screen", nil)
				action:@selector(fullscreen:) keyEquivalent:@"F"];
	[menuItem setTarget:NSApp];

	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Cmd-F exits full screen", nil)
				action:NULL keyEquivalent:@""];
}

+(void)populateWindowMenu:(NSMenu *)aMenu
{
}

+(void)populateHelpMenu:(NSMenu *)aMenu
{
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
	seticon();
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
	[DevdrawDelegate populateMainMenu];

//	[NSThread detachNewThreadSelector:@selector(devdrawMain)
//		toTarget:self withObject:nil];
//	[NSApplication detachDrawingThread:@selector(devdrawMain)
//		toTarget:self withObject:nil];
	[readHandle waitForDataInBackgroundAndNotify];
}

- (id)init
{
	if(self = [super init]){
		readHandle = [[NSFileHandle alloc] initWithFileDescriptor:3 closeOnDealloc:YES];
		[[NSNotificationCenter defaultCenter] addObserver:self
			selector:@selector(devdrawMain:)
			name:NSFileHandleDataAvailableNotification
			object:readHandle];
		[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
			selector:@selector(receiveWake:)
			name:NSWorkspaceDidWakeNotification
			object:NULL];
	}
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[readHandle release];
	return [super dealloc];
}

- (void)devdrawMain:(NSNotification *)notification
{
	uchar buf[4], *mbuf;
	int nmbuf, n, nn;
	Wsysmsg m;
	NSData *data;

	mbuf = nil;
	nmbuf = 0;

	data = [readHandle readDataOfLength:4];
	if([data length] == 4){
		[data getBytes:buf length:4];
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
			sysfatal("malloc: %r");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		data = [readHandle readDataOfLength:(n-4)];
		[data getBytes:(mbuf+4)];
		nn = [data length];
		if(nn != n-4)
			sysfatal("eof during message");

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0)
			sysfatal("cannot convert message");
		if(trace) fprint(2, "<- %W\n", &m);
		runmsg(&m);
	} else {
		[NSApp terminate:self];
	}
	[readHandle waitForDataInBackgroundAndNotify];

return;

	while((n = read(3, buf, 4)) == 4){
		GET(buf, n);
		if(n > nmbuf){
			free(mbuf);
			mbuf = malloc(4+n);
			if(mbuf == nil)
				sysfatal("malloc: %r");
			nmbuf = n;
		}
		memmove(mbuf, buf, 4);
		nn = readn(3, mbuf+4, n-4);
		if(nn != n-4)
			sysfatal("eof during message");

		/* pick off messages one by one */
		if(convM2W(mbuf, nn+4, &m) <= 0)
			sysfatal("cannot convert message");
		if(trace) fprint(2, "<- %W\n", &m);
		runmsg(&m);
	}
}

#pragma mark Notifications

- (void)fullscreen:(NSNotification *)notification
{
	fullscreen(1);
}

- (void)windowWillClose:(NSNotification *)notification
{
//	if(osx.window == [notification object]){
		[[NSNotificationCenter defaultCenter] removeObserver:self];
		[NSApp terminate:self];
//	}
}

- (void)windowDidResize:(NSNotification *)notification
{
//	if(osx.window == [notification object]) {
		eresized(1);
//	}
}

- (void)receiveWake:(NSNotification *)notification
{
	if(trace) NSLog(@"%s:%d %@", __FILE__, __LINE__, notification);
	// redraw
}

- (void)mouseDown:(NSEvent *)anEvent
{
	mouseevent(anEvent);
}

- (void)mouseDragged:(NSEvent *)anEvent
{
	mouseevent(anEvent);
}

- (void)keydown:(NSEvent *)anEvent
{
	kbdevent(anEvent);
}

@end
