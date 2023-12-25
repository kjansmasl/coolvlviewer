/**
 * @file lleventpoll.h
 * @brief LLEvDescription of the LLEventPoll class.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2018, Linden Research, Inc.
 * Copyright (c) 2019-2023, Henri Beauchamp.
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

#ifndef LL_LLEVENTPOLL_H
#define LL_LLEVENTPOLL_H

#include <string>

#include "llpointer.h"

class LLEventPollImpl;
class LLHost;
struct LLEventPollReplies;

// Implements the viewer side of server-to-viewer pushed events.

class LLEventPoll
{
public:
	// Starts polling the URL.
	LLEventPoll(U64 handle, const LLHost& sender, const std::string& poll_url);

	// Stops polling, cancelling any poll in progress.
	~LLEventPoll();

	void setRegionName(const std::string& region_name);

	// Returns true when a poll request is waiting for server events and its
	// age is within the "safe" window (i.e. when it is believed to be old
	// enough for the server to have received it and not too close from the
	// timeout). HB
	bool isPollInFlight() const;
	// Returns the age of the active poll request. HB
	F32 getPollAge() const;

	// Margin in seconds. HB
	static F32 getMargin();

	// Must be called at least once per frame, when it is safe to process
	// messages (outside the rendering routines, in particular). HB
	static void dispatchMessages();

private:
	LLPointer<LLEventPollImpl>	mImpl;
};

#endif // LL_LLEVENTPOLL_H
