#ifdef __APPLE__

#import <AppKit/AppKit.h>

extern "C" void RD_HideDockIcon()
{
	[[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyProhibited];
}

#endif
