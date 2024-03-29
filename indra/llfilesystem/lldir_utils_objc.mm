/**
 * @file lldir_utils_objc.cpp
 * @brief Cocoa implementation of directory utilities for Mac OS X
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#if LL_DARWIN

// WARNING: This file CANNOT use standard linden includes due to conflicts
// between definitions of BOOL

#include "lldir_utils_objc.h"

#import <Cocoa/Cocoa.h>

std::string* getSystemTempFolder()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	NSString* tempDir = NSTemporaryDirectory();
	if (tempDir == nil)
	{
		tempDir = @"/tmp";
	}
	std::string* result = (new std::string([tempDir UTF8String]));
	[pool release];

	return result;
}

// findSystemDirectory scoped exclusively to this file.
std::string* findSystemDirectory(NSSearchPathDirectory searchPathDirectory,
								 NSSearchPathDomainMask domainMask)
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	std::string* result = NULL;
	NSString* path = nil;

	// Search for the path
	NSArray* paths = NSSearchPathForDirectoriesInDomains(searchPathDirectory,
														 domainMask, YES);
	if ([paths count])
	{
		path = [paths objectAtIndex:0];
		// *HACK:  always attempt to create directory, ignore errors.
		NSError* error = nil;
		[[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error];

		result = new std::string([path UTF8String]);
	}
	[pool release];
	return result;
}

std::string* getSystemExecutableFolder()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	NSString* bundlePath = [[NSBundle mainBundle] executablePath];
	std::string* result = (new std::string([bundlePath UTF8String]));
	[pool release];

	return result;
}

std::string* getSystemResourceFolder()
{
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

	NSString* bundlePath = [[NSBundle mainBundle] resourcePath];
	std::string* result = (new std::string([bundlePath UTF8String]));
	[pool release];

	return result;
}

std::string* getSystemCacheFolder()
{
	return findSystemDirectory(NSCachesDirectory, NSUserDomainMask);
}

std::string* getSystemApplicationSupportFolder()
{
	return findSystemDirectory(NSApplicationSupportDirectory,
							   NSUserDomainMask);
}

#endif // LL_DARWIN
