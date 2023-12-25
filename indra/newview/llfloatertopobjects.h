/**
 * @file llfloatertopobjects.h
 * @brief Shows top colliders or top scripts
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERTOPOBJECTS_H
#define LL_LLFLOATERTOPOBJECTS_H

#include "llfloater.h"

class LLMessageSystem;
class LLScrollListCtrl;

// Bits for simulator performance query flags
enum LAND_STAT_FLAGS
{
	STAT_FILTER_BY_PARCEL	= 0x00000001,
	STAT_FILTER_BY_OWNER	= 0x00000002,
	STAT_FILTER_BY_OBJECT	= 0x00000004,
	STAT_REQUEST_LAST_ENTRY	= 0x80000000,
};

enum LAND_STAT_REPORT_TYPE
{
	STAT_REPORT_TOP_SCRIPTS = 0,
	STAT_REPORT_TOP_COLLIDERS
};

class LLFloaterTopObjects final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterTopObjects>
{
	friend class LLUISingleton<LLFloaterTopObjects,
							   VisibilityPolicy<LLFloater> >;

public:
	bool postBuild() override;

	// Opens the floater if it's not on-screen.
	// Juggles the UI based on method = "scripts" or "colliders"
	static void handleLandReply(LLMessageSystem* msg, void** data);

	static void clearList();

	static void sendRefreshRequest();

	static void setMode(U32 mode);

private:
	enum
	{
		ACTION_RETURN = 0,
		ACTION_DISABLE
	};

	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterTopObjects(const LLSD&);

	void updateSelectionInfo();

	void handleReply(LLMessageSystem* msg, void** data);

	void doToObjects(S32 action, bool all);

	static void onCommitObjectsList(LLUICtrl* ctrl, void* data);

	static void onGetByOwnerName(LLUICtrl*, void* data);
	static void onGetByObjectName(LLUICtrl*, void* data);

	static void onRefresh(void* data);
	static void onClickShowBeacon(void* data);
	static void onReturnSelected(void* data);
	static void onDisableSelected(void* data);
	static void onReturnAll(void*);
	static void onDisableAll(void*);
	static void onGetByOwnerNameClicked(void* data);
	static void onGetByObjectNameClicked(void* data);

	static bool callbackReturnAll(const LLSD& notification,
								  const LLSD& response);
	static bool callbackDisableAll(const LLSD& notification,
								   const LLSD& response);

	void showBeacon();

private:
	LLScrollListCtrl*	mObjectsList;

	U32					mCurrentMode;
	U32					mFlags;
	F32					mTotalScore;

	bool				mInitialized;

	std::string			mMethod;
	std::string			mFilter;

	LLSD				mObjectListData;
	uuid_vec_t			mObjectListIDs;
};

#endif
