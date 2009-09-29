#import <Foundation/NSObject.h>
#import <AppKit/NSMenu.h>

@class NSFileHandle;

@interface DevdrawDelegate : NSObject
{
	NSFileHandle *readHandle;
}
+(void)populateMainMenu;
+(void)populateApplicationMenu:(NSMenu *)aMenu;
+(void)populateViewMenu:(NSMenu *)aMenu;
+(void)populateWindowMenu:(NSMenu *)aMenu;
+(void)populateHelpMenu:(NSMenu *)aMenu;
@end
