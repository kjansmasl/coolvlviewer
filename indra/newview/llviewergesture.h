/**
 * @file llviewergesture.h
 * @brief LLViewerGesture class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERGESTURE_H
#define LL_LLVIEWERGESTURE_H

#include "llgesture.h"

class LLViewerGesture final : public LLGesture
{
public:
	LLViewerGesture();
	LLViewerGesture(KEY key, MASK mask, const std::string& trigger,
					const LLUUID& sound_item_id, const std::string& animation,
					const std::string& output_string);

	// Deserializes, advances buffer
	LLViewerGesture(U8** buffer, S32 max_size);

	LLViewerGesture(const LLViewerGesture& gesture);

	// Triggers if a key/mask matches it
	virtual bool trigger(KEY key, MASK mask);

	// Triggers if case-insensitive substring matches (assumes string is
	// lowercase)
	virtual bool trigger(const std::string& string);

	void doTrigger(bool send_chat);
};

class LLViewerGestureList final : public LLGestureList
{
protected:
	LOG_CLASS(LLViewerGestureList);

public:
	LLViewerGestureList();

	// See if the prefix matches any gesture. If so, return true and place the
	// full text of the gesture trigger into output_str
	bool matchPrefix(const std::string& in_str, std::string* out_str);

	static void xferCallback(void* data, S32 size, void**, S32 status);

protected:
	LLGesture* create_gesture(U8** buffer, S32 max_size);
};

extern LLViewerGestureList gGestureList;

#endif
