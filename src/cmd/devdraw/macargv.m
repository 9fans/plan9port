#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <u.h>
#include <libc.h>

AUTOFRAMEWORK(Foundation)
AUTOFRAMEWORK(Cocoa)

@interface appdelegate : NSObject<NSApplicationDelegate> @end

void
main(void)
{
	[NSApplication sharedApplication];
	NSObject<NSApplicationDelegate> *delegate = [appdelegate new];
	[NSApp setDelegate:delegate];

	NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];    /* Register a call-back for URL Events */
	[appleEventManager setEventHandler:delegate andSelector:@selector(handleGetURLEvent:withReplyEvent:)
		forEventClass:kInternetEventClass andEventID:kAEGetURL];

	[NSApp run];
}

@implementation appdelegate
- (void)application:(id)arg openFiles:(NSArray*)file
{
	int i,n;
	NSString *s;

	n = [file count];
	for(i=0; i<n; i++){
		s = [file objectAtIndex:i];
		print("%s\n", [s UTF8String]);
	}
	[NSApp terminate:self];
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
	NSString* url = [[event descriptorForKeyword:keyDirectObject] stringValue];
	print("%s\n", [url UTF8String] + (sizeof("plumb:") - 1));
	[NSApp terminate:self];
}
@end
