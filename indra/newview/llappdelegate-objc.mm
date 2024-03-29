/**
 * @file llappdelegate-objc.mm
 * @brief Class implementation for the Mac version's application delegate.
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// Disable warnings about 'NSKeyUp' and 'NSCommandKeyMask' deprecated: first
// deprecated in macOS 10.12
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import "llappdelegate-objc.h"
#include "llwindowmacosx-objc.h"

// Kuddos to Geir N�klebye for finding these includes in replacement of the
// Carbon/Carbon.h one.
// Saddly, this is insufficient to remove the dependency of the viewer on the
// Carbon library, which is needed for TISCopyCurrentKeyboardInputSource,
// TISGetInputSourceProperty and kTISPropertyInputSourceLanguages, used by the
// languageUpdated method.
#include <Foundation/Foundation.h>
#include <InputMethodKit/IMKCandidates.h>

@implementation LLAppDelegate

@synthesize window;
@synthesize inputWindow;
@synthesize inputView;
@synthesize currentInputLanguage;

- (void)dealloc
{
	[super dealloc];
}

- (void) applicationWillFinishLaunching:(NSNotification*)notification
{
	[[NSAppleEventManager sharedAppleEventManager] setEventHandler:self andSelector:@selector(handleGetURLEvent:withReplyEvent:) forEventClass:kInternetEventClass andEventID:kAEGetURL];
}

- (void) applicationDidFinishLaunching:(NSNotification*)notification
{
	frameTimer = nil;

	[self languageUpdated];

	if (initViewer())
	{
		// Set up recurring calls to oneFrame (repeating timer with timeout 0)
		// until applicationShouldTerminate.
		frameTimer = [NSTimer scheduledTimerWithTimeInterval:0.0 target:self
							  selector:@selector(oneFrame) userInfo:nil repeats:YES];
	}
	else
	{
		handleQuit();
	}

	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(languageUpdated) name:@"NSTextInputContextKeyboardSelectionDidChangeNotification" object:nil];
}

- (void) handleGetURLEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)replyEvent
{
	NSString* url =
		[[[[NSAppleEventManager sharedAppleEventManager] // 1
		   currentAppleEvent] // 2
		  paramDescriptorForKeyword:keyDirectObject] // 3
		 stringValue]; // 4

	const char* url_utf8 = [url UTF8String];
	handleUrl(url_utf8);
}

- (void) applicationDidBecomeActive:(NSNotification*)notification
{
	callWindowFocus();
}

- (void) applicationDidResignActive:(NSNotification*)notification
{
	callWindowUnfocus();
}

- (void) applicationDidHide:(NSNotification*)notification
{
	callWindowHide();
}

- (void) applicationDidUnhide:(NSNotification*)notification
{
	callWindowUnhide();
}

- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication*)sender
{
	// Run one frame to assess state
	if (!pumpMainLoop())
	{
		// pumpMainLoop() returns true when done, false if it wants to be
		// called again. Since it returned false, do not yet cancel frameTimer.
		handleQuit();
		return NSTerminateCancel;
	}

	// pumpMainLoop() returned true: it is done. OK, done with frameTimer.
	[frameTimer release];
	cleanupViewer();
	return NSTerminateNow;
}

- (void) oneFrame
{
	bool appExiting = pumpMainLoop();
	if (appExiting)
	{
		// Once pumpMainLoop() reports that we're done, cancel frameTimer:
		// stop the repetitive calls.
		[frameTimer release];
		[[NSApplication sharedApplication] terminate:self];
	}
}

- (void) showInputWindow:(bool)show withEvent:(NSEvent*)textEvent
{
	if (![self romanScript])
	{
		if (show)
		{
			NSLog(@"Showing input window.");
			[inputWindow makeKeyAndOrderFront:inputWindow];
			if (textEvent != nil)
			{
				[[inputView inputContext] discardMarkedText];
				[[inputView inputContext] handleEvent:textEvent];
			}
		}
		else
		{
			NSLog(@"Hiding input window.");
			[inputWindow orderOut:inputWindow];
			[window makeKeyAndOrderFront:window];
		}
	}
}

// This will get called multiple times by NSNotificationCenter. It will be
// called every time that the window focus changes, and every time the input
// language gets changed. The primary use case for this selector is to update
// our current input language when the user, for whatever reason, changes the
// input language. This is the more elegant way of handling input language
// changes instead of checking every time we want to use the input window.

- (void) languageUpdated
{
	TISInputSourceRef currentInput = TISCopyCurrentKeyboardInputSource();
	CFArrayRef languages =
		(CFArrayRef)TISGetInputSourceProperty(currentInput,
											  kTISPropertyInputSourceLanguages);

	// Typically the language we want is going to be the very first result in
	// the array.
	currentInputLanguage = (NSString*)CFArrayGetValueAtIndex(languages, 0);
}

- (bool) romanScript
{
	// How to add support for new languages with the input window:
	// Simply append this array with the language code (ja for japanese,
	// ko for korean, zh for chinese, etc).
	NSArray* nonRomanScript =
		[[NSArray alloc] initWithObjects:@"ja", @"ko", @"zh-Hant", @"zh-Hans", nil];
	if ([nonRomanScript containsObject:currentInputLanguage])
	{
		return false;
	}

	return true;
}

@end

@implementation LLApplication

- (void)sendEvent:(NSEvent*)event
{
	[super sendEvent:event];
	if ([event type] == NSKeyUp && ([event modifierFlags] & NSCommandKeyMask))
	{
		[[self keyWindow] sendEvent:event];
	}
}

@end
