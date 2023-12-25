/**
 * @file llfloaterchatterbox.h
 * @author Richard
 * @date 2007-05-04
 * @brief Integrated friends and group management/communication tool
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERCHATTERBOX_H
#define LL_LLFLOATERCHATTERBOX_H

#include "llfloater.h"
#include "lltabcontainer.h"

class LLFloaterNewIM;

class LLFloaterChatterBox final : public LLMultiFloater,
								  public LLUISingleton<LLFloaterChatterBox,
													   LLFloaterChatterBox>
{
public:
	LLFloaterChatterBox(const LLSD& seed);

	bool handleKeyHere(KEY key, MASK mask) override;
	void draw() override;
	void onOpen() override;
	void onClose(bool app_quitting) override;
	void setMinimized(bool minimized) override;

	void removeFloater(LLFloater* floaterp) override;
	void addFloater(LLFloater* floaterp, bool select_added_floater,
					LLTabContainer::eInsertionPoint p =
						LLTabContainer::END) override;

	LL_INLINE LLFloaterNewIM* getFloaterNewIM()		{ return mFloaterNewIM; }

	static LLFloater* getCurrentVoiceFloater();

	// Visibility policy for LLUISingleton
	static bool visible(LLFloater* instance, const LLSD& key);
	static void show(LLFloater* instance, const LLSD& key);
	static void hide(LLFloater* instance, const LLSD& key);

private:
	LLFloater* getFloater(const LLSD& key);

protected:
	LLFloater*		mActiveVoiceFloater;
	LLFloaterNewIM*	mFloaterNewIM;

private:
	bool			mFirstOpen;
};

#endif // LL_LLFLOATERCHATTERBOX_H
