#import <Cocoa/Cocoa.h>

#include <u.h>
#include <libc.h>

AUTOFRAMEWORK(Cocoa)

@interface appdelegate : NSObject @end

void
main(void)
{
	if(OSX_VERSION < 100700)
		[NSAutoreleasePool new];

	[NSApplication sharedApplication];
	[NSApp setDelegate:[appdelegate new]];
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
@end
