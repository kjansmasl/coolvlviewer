/**
 * @file llfloatergroups.h
 * @brief LLFloaterGroups class definition
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

#ifndef LL_LLFLOATERGROUPS_H
#define LL_LLFLOATERGROUPS_H

#include "llevents.h"
#include "llfloater.h"

class LLButton;
class LLScrollListCtrl;

class LLFloaterGroupPicker final : public LLFloater
{
public:
	~LLFloaterGroupPicker() override;

	bool postBuild() override;

	typedef fast_hset<LLFloaterGroupPicker*> instances_list_t;
	typedef void(*callback_t)(LLUUID, void*);

	static LLFloaterGroupPicker* show(callback_t callback, void* userdata);

	void setPowersMask(U64 powers_mask);

private:
	// Do not call this directly. Use the show() method above.
	LLFloaterGroupPicker(callback_t callback, void* userdata);

	static void onBtnOK(void* userdata);
	static void onBtnCancel(void* userdata);

private:
	LLScrollListCtrl*		mGroupsList;
	U64						mPowersMask;
	void					(*mSelectCallback)(LLUUID id, void* userdata);
	void*					mCallbackUserdata;

	static instances_list_t	sInstances;
};

class LLFloaterGroups final : public LLFloater,
							  public LLFloaterSingleton<LLFloaterGroups>,
							  public LLOldEvents::LLSimpleListener
{
	friend class LLUISingleton<LLFloaterGroups, VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterGroups() override;

	bool postBuild() override;

	// LLEventListener interface
	bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
					 const LLSD&) override;

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterGroups(const LLSD&);

	void reset();			// Rebuilds the groups list.
	void enableButtons();

	static void onGroupList(LLUICtrl*, void* userdata);

	static bool callbackLeaveGroup(const LLSD& notification,
								   const LLSD& response);

	static void onBtnLeave(void* userdata);
	static void onBtnActivate(void* userdata);
	static void onBtnInfo(void* userdata);
	static void onBtnIM(void* userdata);
	static void onBtnCreate(void*);
	static void onBtnSearch(void*);
	static void onBtnTitles(void*);
	static void onBtnClose(void* userdata);

private:
	LLButton*			mActivateBtn;
	LLButton*			mLeaveBtn;
	LLButton*			mCreateBtn;
	LLButton*			mInfoBtn;
	LLButton*			mIMBtn;
	LLScrollListCtrl*	mGroupsList;
};

#endif // LL_LLFLOATERGROUPS_H
