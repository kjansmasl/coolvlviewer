/**
 * @file llpanelpick.h
 * @brief LLPanelPick class definition
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

// Display of a "Top Pick" used both for the global top picks in the Search
// floater, and also for each individual user's picks in their profile.

#ifndef LL_LLPANELPICK_H
#define LL_LLPANELPICK_H

#include "llpanel.h"
#include "llvector3d.h"
#include "lluuid.h"

#include "llavatarproperties.h"

class LLButton;
class LLCheckBoxCtrl;
class LLIconCtrl;
class LLLineEditor;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;
class LLUICtrl;

class LLPanelPick final : public LLPanel, LLAvatarPropertiesObserver
{
protected:
	LOG_CLASS(LLPanelPick);

public:
	LLPanelPick(bool top_pick);
	~LLPanelPick() override;

	void reset();

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	// LLAvatarPropertiesObserver override
	void processProperties(S32 type, void* data) override;

	// Setup a new pick, including creating an id, giving a sane
	// initial position, etc.
	void initNewPick();

	// We need to know the creator id so the database knows which partition to
	// query for the pick data.
	void setPickID(const LLUUID& pick_id, const LLUUID& creator_id);

	// Schedules the panel to request data from server next time it is drawn
	void markForServerRequest();

	std::string getPickName();
	LL_INLINE const LLUUID& getPickID() const			{ return mPickID; }
	LL_INLINE const LLUUID& getPickCreatorID() const	{ return mCreatorID; }

	void sendPickInfoRequest();
	void sendPickInfoUpdate();

protected:
	static std::string createLocationText(const std::string& owner_name,
										  std::string parcel_name,
										  const std::string& sim_name,
										  const LLVector3d& pos_global);

	static void onClickTeleport(void* data);
	static void onClickMap(void* data);
	static void onClickSetLocation(void* data);

	static void onCommitAny(LLUICtrl* ctrl, void* data);

protected:
	LLTextureCtrl*		mSnapshotCtrl;
	LLLineEditor*		mNameEditor;
	LLTextEditor*		mDescEditor;
	LLLineEditor*		mLocationEditor;

	LLButton*			mTeleportBtn;
	LLButton*			mMapBtn;

	LLTextBox*			mSortOrderText;
	LLLineEditor*		mSortOrderEditor;
	LLCheckBoxCtrl*		mEnabledCheck;
	LLButton*			mSetBtn;

	LLUUID				mPickID;
	LLUUID				mCreatorID;
	LLUUID				mParcelID;

	LLVector3d			mPosGlobal;
	std::string			mSimName;

	bool				mTopPick;
	// Data will be requested on first draw
	bool				mDataRequested;
	bool				mDataReceived;
};

#endif // LL_LLPANELPICK_H
