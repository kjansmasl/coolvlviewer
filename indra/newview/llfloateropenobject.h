/**
 * @file llfloateropenobject.h
 * @brief LLFloaterOpenObject class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/*
 * Shows the contents of an object and their permissions when you click
 * "Buy..." on an object with "Sell Contents" checked.
 */

#ifndef LL_LLFLOATEROPENOBJECT_H
#define LL_LLFLOATEROPENOBJECT_H

#include "llfloater.h"
#include "llsafehandle.h"

class LLObjectSelection;
class LLPanelInventory;
class LLTextBox;

class LLFloaterOpenObject final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterOpenObject>
{
	friend class LLUISingleton<LLFloaterOpenObject,
							   VisibilityPolicy<LLFloater> >;

public:
	bool postBuild() override;
	void refresh() override;
	void draw() override;

	static void show();
	static void dirty();

	struct LLCatAndWear
	{
		LLUUID	mCatID;
		bool	mWear;
		bool	mFolderResponded;
	};

private:
	LLFloaterOpenObject(const LLSD&);

	void moveToInventory(bool wear);

	static void onClickMoveToInventory(void* data);
	static void onClickMoveAndWear(void* data);
	static void callbackCreateCategory(const LLUUID& category_id,
									   LLUUID object_id, bool wear);
	static void callbackMoveInventory(S32 result, void* data);
	static void* createPanelInventory(void* data);

private:
	LLPanelInventory*				mPanelInventory;
	LLTextBox*						mDescription;
	LLSafeHandle<LLObjectSelection>	mObjectSelection;
	U32								mLastCount;
	bool							mDirty;
};

#endif
