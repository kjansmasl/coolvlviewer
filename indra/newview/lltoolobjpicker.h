/**
 * @file lltoolobjpicker.h
 * @brief LLToolObjPicker class header file
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_TOOLOBJPICKER_H
#define LL_TOOLOBJPICKER_H

#include "lluuid.h"

#include "lltool.h"

class LLPickInfo;

class LLToolObjPicker final : public LLTool
{
protected:
	LOG_CLASS(LLToolObjPicker);

public:
	LLToolObjPicker();

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	void handleSelect() override;
	void handleDeselect() override;

	void onMouseCaptureLost() override;

	LL_INLINE void setExitCallback(void (*callback)(void*), void* user_data)
	{
		mExitCallback = callback;
		mExitCallbackData = user_data;
	}

	LL_INLINE const LLUUID& getObjectID() const		{ return mHitObjectID; }

	static void pickCallback(const LLPickInfo& pick_info);

protected:
	LLUUID	mHitObjectID;
	void 	(*mExitCallback)(void *callback_data);
	void*	mExitCallbackData;
	bool	mPicked;
};

extern LLToolObjPicker gToolObjPicker;

#endif
