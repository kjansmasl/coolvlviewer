/**
 * @file llfloaterperms.h
 * @brief Asset creation permission preferences.
 * @author Coco
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

#ifndef LL_LLFLOATERPERMPREFS_H
#define LL_LLFLOATERPERMPREFS_H

#include "llfloater.h"

class LLFloaterPerms final : public LLFloater,
							 public LLFloaterSingleton<LLFloaterPerms>
{
	friend class LLUISingleton<LLFloaterPerms, VisibilityPolicy<LLFloater> >;

public:
	void cancel();

	static void onClickOK(void*);
	static void onClickCancel(void*);
	static void onCommitCopy(LLUICtrl* ctrl, void* data);

	// Convenience methods only used in other floaters to get current
	// permission preference bitfields from saved settings:

	// Returns prefix + "EveryoneCopy"
	static U32 getEveryonePerms(const std::string& prefix = LLStringUtil::null);

	// Returns prefix + "ShareWithGroup"
	static U32 getGroupPerms(const std::string& prefix = LLStringUtil::null);

	// Returns bitfield for prefix + "NextOwner" + "Copy", "Modify" and
	// "Transfer"
	static U32 getNextOwnerPerms(const std::string& prefix = LLStringUtil::null);

private:
	LLFloaterPerms(const LLSD& seed);

	bool postBuild() override;
	void refresh() override;
	void onClose(bool app_quitting = false) override;

	/// Callback for the menus help button
	static void onClickHelp(void* data);

private:
	// Cached values only for implementing cancel.
	bool mShareWithGroup;
	bool mEveryoneCopy;
	bool mNextOwnerCopy;
	bool mNextOwnerModify;
	bool mNextOwnerTransfer;
	bool mFullPermNotecards;
	bool mFullPermSnapshots;
	bool mNoModScripts;
};

#endif
