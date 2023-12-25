/** 
 * @file llfloatermute.h
 * @brief Container for mute list
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

#ifndef LL_LLFLOATERMUTE_H
#define LL_LLFLOATERMUTE_H

#include <vector>

#include "llfloater.h"

#include "llmutelist.h"

class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;

class LLFloaterMute final : public LLFloater, public LLMuteListObserver,
							public LLFloaterSingleton<LLFloaterMute>
{
	friend class LLUISingleton<LLFloaterMute, VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterMute() override;

	bool postBuild() override;
	void onClose(bool app_quitting) override		{ setVisible(false); }

	// LLMuteListObserver callback interface implementation.
	void onChange() override;

	static void selectMute(const LLUUID& id);
	static void selectMute(const std::string& name);

private:
	// Open only via either selectMute() or via LLFloaterSingleton interface
	// (showInstance(), toggleInstance() or implicitely via getInstance()).
	LLFloaterMute(const LLSD&);

	void refreshMuteList();
	void updateButtons();

	// UI callbacks
	static void onClickUpdateMutes(void* data);
	static void onClickRemove(void* data);
	static void onClickPick(void* data);
	static void onDoubleClickName(void* data);
	static void onSelectName(LLUICtrl* caller, void* data);
	static void onPickUser(const std::vector<std::string>& names,
						   const std::vector<LLUUID>& ids, void* data);
	static void onClickMuteByName(void*);
	static void callbackMuteByName(const std::string& text, void*);
	static void onMuteAllToggled(LLUICtrl*, void* data);
	static void onMuteTypeToggled(LLUICtrl* ctrl, void* data);

private:
	LLButton*			mUnmute;
	LLButton*			mUpdateMutes;
	LLCheckBoxCtrl*		mMuteAll;
	LLCheckBoxCtrl*		mMuteChat;
	LLCheckBoxCtrl*		mMuteParticles;
	LLCheckBoxCtrl*		mMuteSound;
	LLCheckBoxCtrl*		mMuteVoice;
	LLScrollListCtrl*	mMuteList;
};

#endif
