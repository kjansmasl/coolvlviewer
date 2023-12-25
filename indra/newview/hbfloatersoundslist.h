/**
 * @file hbfloatersoundlist.h
 * @brief HBFloaterSoundsList class definition
 *
 * This class implements a floater where all sounds are listed, allowing
 * the user to mute a source or stop any sound.
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Henri Beauchamp.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_HBFLOATERSOUNDSLIST_H
#define LL_HBFLOATERSOUNDSLIST_H

#include "llfloater.h"

#include "llmutelist.h"

class LLButton;
class LLFlyoutButton;
class LLCheckBoxCtrl;
class LLMessageSystem;
class LLScrollListCtrl;
class LLVector3d;

class HBFloaterSoundsList final
:	public LLFloater,
	public LLFloaterSingleton<HBFloaterSoundsList>,
	public LLMuteListObserver
{
	friend class LLUISingleton<HBFloaterSoundsList,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterSoundsList);

public:
	~HBFloaterSoundsList() override;

	static LLVector3d selectedLocation();

	// Used in llviewermessage.cpp to inform us we changed region
	static void newRegion();

	static void processObjectPropertiesFamily(LLMessageSystem* msg);

	// Used as a callback to avatar name resolution, as well as in
	// hbviewerautomation.cpp when changing the blocked sounds list.
	static void setDirty();

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterSoundsList(const LLSD&);

	bool postBuild() override;
	void draw() override;

	// LLMuteListObserver interface
	void onChange() override;

	void setButtonsStatus();

	void requestInfo(const LLUUID& object_id);

	static void onPlaySoundBtn(LLUICtrl* ctrl, void* userdata);
	static void onBlockSoundBtn(LLUICtrl* ctrl, void* userdata);
	static void onMuteOwnerBtn(void* userdata);
	static void onShowSourceBtn(LLUICtrl* ctrl,void* userdata);
	static void onMuteObjectBtn(LLUICtrl* ctrl, void* userdata);

	static void onDoubleClick(void* userdata);
	static void onSelectSound(LLUICtrl*, void* userdata);

	enum SOUNDS_COLUMN_ORDER
	{
		LIST_SOUND = 0,
		LIST_OBJECT,
		LIST_OWNER,
		LIST_SOURCE_ID,
		LIST_OBJECT_ID,
		LIST_OWNER_ID,
	};

private:
	LLFlyoutButton*		mPlayFlyoutBtn;
	LLFlyoutButton*		mBlockSoundBtn;
	LLButton*			mMuteOwnerBtn;
	LLFlyoutButton*		mShowFlyoutBtn;
	LLFlyoutButton*		mMuteFlyoutBtn;
	LLCheckBoxCtrl*		mFreezeCheck;
	LLScrollListCtrl*	mSoundsList;

	LLUUID				mTrackingID;
	LLVector3d			mTrackingLocation;
	LLVector3d			mSelectedLocation;

	F32					mLastUpdate;

	bool				mIsDirty;
	bool				mTracking;

	std::string			mNoneString;
	std::string			mLoadingString;
	std::string			mAttachmentString;

	uuid_list_t			mIgnoredSounds;
	uuid_list_t			mRequests;

	typedef fast_hmap<LLUUID, std::string> names_map_t;
	static names_map_t	sObjectNames;

	typedef fast_hmap<LLUUID, LLUUID> groups_map_t;
	static groups_map_t	sGroupOwnedObjects;
};

#endif	// LL_HBFLOATERSOUNDSLIST_H
