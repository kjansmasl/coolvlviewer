/**
 * @file hbfloatersoundslist.cpp
 * @brief HBFloaterSoundsList class implementation
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

#include "llviewerprecompiledheaders.h"

#include "hbfloatersoundslist.h"

#include "llaudioengine.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "llmessage.h"
#include "sound_ids.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llfloaterinspect.h"
#include "llfloatermute.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "lltracker.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"

// We do not update the sounds list faster than that
constexpr F32 MIN_UPDATE_DELAY = 1.f;	// in seconds

//static
HBFloaterSoundsList::names_map_t HBFloaterSoundsList::sObjectNames;
HBFloaterSoundsList::groups_map_t HBFloaterSoundsList::sGroupOwnedObjects;

HBFloaterSoundsList::HBFloaterSoundsList(const LLSD&)
:	mLastUpdate(0.f),
	mIsDirty(true),
	mTrackingLocation()
{
	// Ignore all collision/sliding/rolling sounds from sound_ids.h
	if (gIsInSecondLife)
	{
		mIgnoredSounds.emplace(SND_FLESH_FLESH);
		mIgnoredSounds.emplace(SND_FLESH_PLASTIC);
		mIgnoredSounds.emplace(SND_FLESH_RUBBER);
		mIgnoredSounds.emplace(SND_GLASS_FLESH);
		mIgnoredSounds.emplace(SND_GLASS_GLASS);
		mIgnoredSounds.emplace(SND_GLASS_PLASTIC);
		mIgnoredSounds.emplace(SND_GLASS_RUBBER);
		mIgnoredSounds.emplace(SND_GLASS_WOOD);
		mIgnoredSounds.emplace(SND_METAL_FLESH);
		mIgnoredSounds.emplace(SND_METAL_GLASS);
		mIgnoredSounds.emplace(SND_METAL_METAL);
		mIgnoredSounds.emplace(SND_METAL_PLASTIC);
		mIgnoredSounds.emplace(SND_METAL_RUBBER);
		mIgnoredSounds.emplace(SND_METAL_WOOD);
		mIgnoredSounds.emplace(SND_PLASTIC_PLASTIC);
		mIgnoredSounds.emplace(SND_RUBBER_PLASTIC);
		mIgnoredSounds.emplace(SND_RUBBER_RUBBER);
		mIgnoredSounds.emplace(SND_STONE_FLESH);
		mIgnoredSounds.emplace(SND_STONE_GLASS);
		mIgnoredSounds.emplace(SND_STONE_METAL);
		mIgnoredSounds.emplace(SND_STONE_PLASTIC);
		mIgnoredSounds.emplace(SND_STONE_RUBBER);
		mIgnoredSounds.emplace(SND_STONE_STONE);
		mIgnoredSounds.emplace(SND_STONE_WOOD);
		mIgnoredSounds.emplace(SND_WOOD_FLESH);
		mIgnoredSounds.emplace(SND_WOOD_PLASTIC);
		mIgnoredSounds.emplace(SND_WOOD_RUBBER);
		mIgnoredSounds.emplace(SND_WOOD_WOOD);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_FLESH);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_FABRIC);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_GRAVEL);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_GRAVEL_02);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_GRAVEL_03);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_GRAVEL);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_GRAVEL_02);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_GRAVEL_03);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_FABRIC);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_FLESH);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_FLESH_02);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GLASS);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GLASS_02);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GLASS_03);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GLASS_04);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GRAVEL);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_GRAVEL_02);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL_02);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL_03);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL_04);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL_05);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_METAL_06);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_RUBBER);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_02);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_03);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_04);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_05);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_06);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_07);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_WOOD_08);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL_02);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL_03);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL_04);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL_05);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_GRAVEL_06);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_FABRIC);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_FABRIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_FABRIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_FABRIC_04);
		mIgnoredSounds.emplace(SND_SLIDE_RUBBER_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_RUBBER_PLASTIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_RUBBER_PLASTIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_PLASTIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_PLASTIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_RUBBER);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_RUBBER_02);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_STONE);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_STONE_02);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_WOOD);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_WOOD_02);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_WOOD_03);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_WOOD_04);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FABRIC);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FABRIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FABRIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FABRIC_04);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FLESH);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FLESH_02);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FLESH_03);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_FLESH_04);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_GRAVEL);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_GRAVEL_02);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_GRAVEL_03);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_GRAVEL_04);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_PLASTIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_PLASTIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_02);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_03);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_04);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_05);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_06);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_07);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_WOOD_08);
		mIgnoredSounds.emplace(SND_ROLL_FLESH_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_FLESH_PLASTIC_02);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_GRAVEL);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_GRAVEL_02);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_WOOD);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_WOOD_02);
		mIgnoredSounds.emplace(SND_ROLL_GRAVEL_GRAVEL);
		mIgnoredSounds.emplace(SND_ROLL_GRAVEL_GRAVEL_02);
		mIgnoredSounds.emplace(SND_ROLL_METAL_FABRIC);
		mIgnoredSounds.emplace(SND_ROLL_METAL_FABRIC_02);
		mIgnoredSounds.emplace(SND_ROLL_METAL_GLASS);
		mIgnoredSounds.emplace(SND_ROLL_METAL_GLASS_02);
		mIgnoredSounds.emplace(SND_ROLL_METAL_GLASS_03);
		mIgnoredSounds.emplace(SND_ROLL_METAL_GRAVEL);
		mIgnoredSounds.emplace(SND_ROLL_METAL_METAL);
		mIgnoredSounds.emplace(SND_ROLL_METAL_METAL_02);
		mIgnoredSounds.emplace(SND_ROLL_METAL_METAL_03);
		mIgnoredSounds.emplace(SND_ROLL_METAL_METAL_04);
		mIgnoredSounds.emplace(SND_ROLL_METAL_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_METAL_PLASTIC_01);
		mIgnoredSounds.emplace(SND_ROLL_METAL_WOOD);
		mIgnoredSounds.emplace(SND_ROLL_METAL_WOOD_02);
		mIgnoredSounds.emplace(SND_ROLL_METAL_WOOD_03);
		mIgnoredSounds.emplace(SND_ROLL_METAL_WOOD_04);
		mIgnoredSounds.emplace(SND_ROLL_METAL_WOOD_05);
		mIgnoredSounds.emplace(SND_ROLL_PLASTIC_FABRIC);
		mIgnoredSounds.emplace(SND_ROLL_PLASTIC_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_PLASTIC_PLASTIC_02);
		mIgnoredSounds.emplace(SND_ROLL_STONE_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_STONE_STONE);
		mIgnoredSounds.emplace(SND_ROLL_STONE_STONE_02);
		mIgnoredSounds.emplace(SND_ROLL_STONE_STONE_03);
		mIgnoredSounds.emplace(SND_ROLL_STONE_STONE_04);
		mIgnoredSounds.emplace(SND_ROLL_STONE_STONE_05);
		mIgnoredSounds.emplace(SND_ROLL_STONE_WOOD);
		mIgnoredSounds.emplace(SND_ROLL_STONE_WOOD_02);
		mIgnoredSounds.emplace(SND_ROLL_STONE_WOOD_03);
		mIgnoredSounds.emplace(SND_ROLL_STONE_WOOD_04);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_FLESH);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_FLESH_02);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_FLESH_03);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_FLESH_04);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_GRAVEL);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_GRAVEL_02);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_GRAVEL_03);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_PLASTIC_02);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_02);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_03);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_04);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_05);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_06);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_07);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_08);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_WOOD_09);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_STONE_01);
		mIgnoredSounds.emplace(SND_STONE_DIRT_01);
		mIgnoredSounds.emplace(SND_STONE_DIRT_02);
		mIgnoredSounds.emplace(SND_STONE_DIRT_03);
		mIgnoredSounds.emplace(SND_STONE_DIRT_04);
		mIgnoredSounds.emplace(SND_STONE_STONE_02);
		mIgnoredSounds.emplace(SND_STONE_STONE_04);
		mIgnoredSounds.emplace(SND_STEP_ON_LAND);
		mIgnoredSounds.emplace(SND_OPENSIM_COLLISION);
#if 0	// These are NULL UUIDs...
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_FLESH_RUBBER);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_FLESH);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_GLASS);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_RUBBER);
		mIgnoredSounds.emplace(SND_SLIDE_GLASS_WOOD);
		mIgnoredSounds.emplace(SND_SLIDE_METAL_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_PLASTIC);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_PLASTIC_02);
		mIgnoredSounds.emplace(SND_SLIDE_PLASTIC_PLASTIC_03);
		mIgnoredSounds.emplace(SND_SLIDE_RUBBER_RUBBER);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_FLESH);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_GLASS);
		mIgnoredSounds.emplace(SND_SLIDE_STONE_METAL);
		mIgnoredSounds.emplace(SND_SLIDE_WOOD_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_FLESH_FLESH);
		mIgnoredSounds.emplace(SND_ROLL_FLESH_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_FLESH);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_GLASS);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_GLASS_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_METAL_FLESH);
		mIgnoredSounds.emplace(SND_ROLL_METAL_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_RUBBER_PLASTIC);
		mIgnoredSounds.emplace(SND_ROLL_RUBBER_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_STONE_FLESH);
		mIgnoredSounds.emplace(SND_ROLL_STONE_GLASS);
		mIgnoredSounds.emplace(SND_ROLL_STONE_METAL);
		mIgnoredSounds.emplace(SND_ROLL_STONE_RUBBER);
		mIgnoredSounds.emplace(SND_ROLL_WOOD_RUBBER);
#endif
	}
	else
	{
		// Just one collision sound available in OpenSIM...
		mIgnoredSounds.emplace(SND_OPENSIM_COLLISION);
	}

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_sounds_list.xml");

	LLMuteList::addObserver(this);
}

//virtual
HBFloaterSoundsList::~HBFloaterSoundsList()
{
	LLMuteList::removeObserver(this);
}

//virtual
bool HBFloaterSoundsList::postBuild()
{
	mSoundsList = getChild<LLScrollListCtrl>("sounds_list");
	mSoundsList->setCommitCallback(onSelectSound);
	mSoundsList->setDoubleClickCallback(onDoubleClick);
	mSoundsList->setCallbackUserData(this);

	mPlayFlyoutBtn = getChild<LLFlyoutButton>("play");
	mPlayFlyoutBtn->setCommitCallback(onPlaySoundBtn);
	mPlayFlyoutBtn->setCallbackUserData(this);
	mPlayFlyoutBtn->setEnabled(false);

	mBlockSoundBtn = getChild<LLFlyoutButton>("block");
	mBlockSoundBtn->setCommitCallback(onBlockSoundBtn);
	mBlockSoundBtn->setCallbackUserData(this);
	mBlockSoundBtn->setEnabled(false);

	mShowFlyoutBtn = getChild<LLFlyoutButton>("show");
	mShowFlyoutBtn->setCommitCallback(onShowSourceBtn);
	mShowFlyoutBtn->setCallbackUserData(this);
	mShowFlyoutBtn->setEnabled(false);

	mMuteFlyoutBtn = getChild<LLFlyoutButton>("mute_object");
	mMuteFlyoutBtn->setCommitCallback(onMuteObjectBtn);
	mMuteFlyoutBtn->setCallbackUserData(this);
	mMuteFlyoutBtn->setEnabled(false);

	mMuteOwnerBtn = getChild<LLButton>("mute_owner");
	mMuteOwnerBtn->setClickedCallback(onMuteOwnerBtn, this);
	mMuteOwnerBtn->setEnabled(false);

	mFreezeCheck = getChild<LLCheckBoxCtrl>("freeze");

	mNoneString = getString("none");
	mLoadingString = getString("loading");
	mAttachmentString = getString("attachment");

	return true;
}

//virtual
void HBFloaterSoundsList::draw()
{
	static LLCachedControl<bool> play_attached(gSavedSettings,
											   "EnableAttachmentSounds");

	if (!mTrackingLocation.isExactlyZero())
	{
		F32 dist = 3.f;
		if (gTracker.getTrackingStatus() == LLTracker::TRACKING_LOCATION)
		{
			dist = fabsf((F32)(gTracker.getTrackedPositionGlobal() -
							   mTrackingLocation).length());
		}
		if (dist > 2.f)
		{
			// Tracker stopped or tracking another location
			mTrackingID.setNull();
			mTrackingLocation.setZero();
			mIsDirty = true;
			mLastUpdate = 0.f;	// force an immediate update
		}
	}

	if (gAudiop && !mFreezeCheck->get() && !isMinimized() &&
		(mIsDirty || gAudiop->mSourcesUpdated) &&
		gFrameTimeSeconds - mLastUpdate >= MIN_UPDATE_DELAY)
	{
		mLastUpdate = gFrameTimeSeconds;
		gAudiop->mSourcesUpdated = false;	// Reset the flag

		LLUUID cur_source_id, cur_object_id, cur_sound_id, selected_id;
		LLScrollListItem* item = mSoundsList->getFirstSelected();
		if (item)
		{
			cur_sound_id = item->getColumn(LIST_SOUND)->getValue().asUUID();
			cur_source_id = item->getColumn(LIST_SOURCE_ID)->getValue().asUUID();
			cur_object_id = item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
		}
		S32 scrollpos = mSoundsList->getScrollPos();

		mSoundsList->deleteAllItems();
		LLUUID owner_id, source_id, object_id, item_id;
		std::string owner_name, object_name, style;
		for (LLAudioEngine::source_map_t::const_iterator
				it = gAudiop->mAllSources.begin(),
				end = gAudiop->mAllSources.end();
			 it != end; ++it)
		{
			const LLAudioSource* source = it->second;
			if (!source)	// Paranoia
			{
				continue;
			}

			const uuid_list_t& sounds = source->getPlayedSoundsUUIDs();
			if (sounds.empty())
			{
				// No sound played for this source, so far... Skip it.
				continue;
			}

			LLViewerObject* objectp = NULL;
			source_id = object_id = source->getID();
			if (object_id.notNull())
			{
				objectp = gObjectList.findObject(object_id);
				if (!objectp)
				{
					// It is most likey an object-less source (triggered sound
					// or UI sound). Treat it as such and report no object for
					// it.
					object_id.setNull();
				}
			}

			bool loading_object = false;
			if (objectp)
			{
				// Always use the root-edit name and Id (since we cannot get
				// any info for non-root objects without editing them for good.
				LLViewerObject* parentp = objectp->getRootEdit();
				if (parentp)
				{
					object_id = parentp->getID();
				}
				else
				{
					parentp = objectp;
				}

				names_map_t::iterator nit = sObjectNames.find(object_id);
				if (nit != sObjectNames.end())
				{
					object_name = nit->second;
				}
				else if (parentp->isAttachment())
				{
					// Do not bother asking for details: attachments info
					// cannot be gathered that easily...
					object_name = mAttachmentString;
					// ... and store for faster, future lookups:
					sObjectNames.emplace(object_id, object_name);
				}
				else
				{
					requestInfo(object_id);
					object_name = mLoadingString;
					loading_object = true;
				}
			}
			else
			{
				object_name = mNoneString;
			}

			bool loading_owner = false;
			owner_id = source->getOwnerID();
			if (owner_id.isNull())
			{
				LL_DEBUGS("SoundsList") << "No object owner stored in LLAudioSource for object:"
										<< object_id << LL_ENDL;
				if (objectp)
				{
					owner_id = objectp->mOwnerID;
				}
			}
			if (owner_id.isNull())
			{
				if (object_id.notNull())
				{
					groups_map_t::iterator git =
						sGroupOwnedObjects.find(object_id);
					if (git != sGroupOwnedObjects.end())
					{
						owner_id = git->second;
					}
					else
					{
						LL_DEBUGS("SoundsList") << "No object owner stored in LLViewerObject for object:"
												<< object_id << LL_ENDL;
						// No info about the owner... We will find out via the
						// object info request
						requestInfo(object_id);
						owner_name = mLoadingString;
						loading_owner = true;
					}
				}
				else
				{
					owner_name = getString("unknown");
				}
			}
			if (gCacheNamep && owner_id.notNull())
			{
				bool group_owned = sGroupOwnedObjects.count(object_id) != 0;
				bool ret = group_owned ?
						   gCacheNamep->getGroupName(owner_id, owner_name) :
						   gCacheNamep->getFullName(owner_id, owner_name);
				if (!ret)
				{
					gCacheNamep->get(owner_id, group_owned,
									 boost::bind(&HBFloaterSoundsList::setDirty));
					owner_name = mLoadingString;
					loading_owner = true;
				}
			}

			bool muted_object = owner_id != gAgentID && object_id.notNull() &&
								(LLMuteList::isMuted(object_id) ||
								 (!play_attached &&
								  object_name == mAttachmentString));
			bool muted_owner = owner_id != gAgentID && owner_id.notNull() &&
							   LLMuteList::isMuted(owner_id,
												   LLMute::flagObjectSounds);

			if ((object_id.notNull() && object_id == mTrackingID) ||
				(object_id.isNull() && source_id == mTrackingID))
			{
				style = "BOLD";
			}
			else
			{
				style = "NORMAL";
			}

			for (uuid_list_t::const_iterator sit = sounds.begin(),
											 send = sounds.end();
				 sit != send; ++sit)
			{
				const LLUUID& sound_id = *sit;
				if (sound_id.isNull() || mIgnoredSounds.count(sound_id) ||
					(object_id.isNull() && owner_id == gAgentID &&
					 gAudiop->isUISound(sound_id)))
				{
					// Do not take into account the sounds played by the grid's
					// physics engine (collision, sliding, rolling sounds),
					// neither the UI sounds played by the viewer, neither a
					// null uuid (paranoia).
					continue;
				}

				// Note: a same source Id may appear several times in the list,
				// associated with several sounds; a sound Id may be used in
				// several sources; an object may use several sources...
				// Since we must use a unique Id for each list element, let's
				// generate one randomly...
				item_id.generate();

				// Retain this line as the selected one if the sound_id and
				// either the object_id (when not null) or source_id match the
				// ones that were selected before the list was cleared.
				if (sound_id == cur_sound_id &&
					((object_id.notNull() && object_id == cur_object_id) ||
					 (object_id.isNull() && source_id == cur_source_id)))
				{
					selected_id = item_id;
				}

				LLSD element;
				element["id"] = item_id;

				LLSD& sound_column = element["columns"][LIST_SOUND];
				sound_column["column"] = "sound";
				sound_column["value"] = sound_id.asString();
				sound_column["font-style"] = style;
				if (LLAudioData::isBlockedSound(sound_id))
				{
					sound_column["color"] = LLColor4::red2.getValue();
				}

				LLSD& object_column = element["columns"][LIST_OBJECT];
				object_column["column"] = "object";
				object_column["value"] = object_name;
				object_column["font-style"] = loading_object ? style + "|ITALIC"
															 : style;
				if (muted_object)
				{
					object_column["color"] = LLColor4::red2.getValue();
				}

				LLSD& owner_column = element["columns"][LIST_OWNER];
				owner_column["column"] = "owner";
				owner_column["value"] = owner_name;
				owner_column["font-style"] = loading_owner ? style + "|ITALIC"
														   : style;
				if (muted_owner)
				{
					owner_column["color"] = LLColor4::red2.getValue();
				}

				LLSD& srcid_column = element["columns"][LIST_SOURCE_ID];
				srcid_column["column"] = "source_id";
				srcid_column["value"] = source_id;

				LLSD& objid_column = element["columns"][LIST_OBJECT_ID];
				objid_column["column"] = "object_id";
				objid_column["value"] = object_id;

				LLSD& ownid_column = element["columns"][LIST_OWNER_ID];
				ownid_column["column"] = "owner_id";
				ownid_column["value"] = owner_id;

				mSoundsList->addElement(element, ADD_SORTED);
			}
		}

		mSoundsList->setScrollPos(scrollpos);
		if (selected_id.notNull())
		{
			mSoundsList->selectByID(selected_id);
#if 0		// This can become rather annoying on fast refreshing lists...
			mSoundsList->scrollToShowSelected();
#endif
		}
		else
		{
			mSoundsList->deselectAllItems(true);
			mPlayFlyoutBtn->setEnabled(false);
			mShowFlyoutBtn->setEnabled(false);
			mBlockSoundBtn->setEnabled(false);
			mMuteFlyoutBtn->setEnabled(false);
			mMuteOwnerBtn->setEnabled(false);
			mSelectedLocation.setZero();
		}

		mIsDirty = false;
	}

	LLFloater::draw();
}

//virtual
void HBFloaterSoundsList::onChange()
{
	mIsDirty = true;
	mLastUpdate = 0.f;	// Force an immediate update
	setButtonsStatus();
}

void HBFloaterSoundsList::setButtonsStatus()
{
	LLScrollListItem* item = mSoundsList->getFirstSelected();
	bool selected = item != NULL;
	mPlayFlyoutBtn->setEnabled(selected);
	mShowFlyoutBtn->setEnabled(selected);
	mBlockSoundBtn->setEnabled(selected);
	mMuteFlyoutBtn->setEnabled(selected);
	mMuteOwnerBtn->setEnabled(selected);

	if (selected)
	{
		const LLUUID& sound_id =
			item->getColumn(LIST_SOUND)->getValue().asUUID();
		if (LLAudioData::isBlockedSound(sound_id))
		{
		   mBlockSoundBtn->setLabel(getString("allow_sound_text"));
		   mPlayFlyoutBtn->setEnabled(false); // We cannot play it anyway
		}
		else
		{
			mBlockSoundBtn->setLabel(getString("block_sound_text"));
		}

		const LLUUID& object_id =
			item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
		if (object_id.notNull() && LLMuteList::isMuted(object_id))
		{
			mMuteFlyoutBtn->setLabel(getString("unmute_object_text"));
		}
		else
		{
			mMuteFlyoutBtn->setLabel(getString("mute_object_text"));
		}

		// Set the selected source location
		LLVector3d pos_global;
		if (object_id.notNull())
		{
			LLViewerObject* objectp = gObjectList.findObject(object_id);
			if (objectp)
			{
				pos_global = objectp->getPositionGlobal();
			}
		}
		if (pos_global.isExactlyZero())
		{
			// Get the source id
			const LLUUID& source_id =
				item->getColumn(LIST_SOURCE_ID)->getValue().asUUID();
			// Find the source (if still there) and its position
			for (LLAudioEngine::source_map_t::const_iterator
					it = gAudiop->mAllSources.begin(),
					end = gAudiop->mAllSources.end();
				 it != end; ++it)
			{
				const LLAudioSource* source = it->second;
				if (source && source->getID() == source_id)
				{
					pos_global = source->getPositionGlobal();
					break;
				}
			}
		}
		mSelectedLocation = pos_global;

		const LLUUID& owner_id =
			item->getColumn(LIST_OWNER_ID)->getValue().asUUID();
		if (owner_id.notNull() &&
			LLMuteList::isMuted(owner_id, LLMute::flagObjectSounds))
		{
			mMuteOwnerBtn->setLabel(getString("unmute_owner_text"));
		}
		else
		{
			mMuteOwnerBtn->setLabel(getString("mute_owner_text"));
		}
		if (owner_id == gAgentID)
		{
			// Cannot mute self...
			mMuteFlyoutBtn->setEnabled(false);
			mMuteOwnerBtn->setEnabled(false);
		}

		if (object_id.isNull())
		{
			mMuteFlyoutBtn->setEnabled(false);
			mShowFlyoutBtn->setEnabled(false);
		}
		if (owner_id.isNull())
		{
			mMuteOwnerBtn->setEnabled(false);
		}
	}
	else
	{
		mSelectedLocation.setZero();
	}
}

void HBFloaterSoundsList::requestInfo(const LLUUID& object_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg && object_id.notNull() && !mRequests.count(object_id))
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp)
		{
			LLViewerRegion* regionp = objectp->getRegion();
			LLMessageSystem* msg = gMessageSystemp;
			if (regionp && msg)
			{
				mRequests.emplace(object_id);
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags, 0);
				msg->addUUIDFast(_PREHASH_ObjectID, object_id);
				msg->sendReliable(regionp->getHost());
				LL_DEBUGS("SoundsList") << "Sent data request for object "
										<< object_id << LL_ENDL;
			}
		}
	}
}

//static
void HBFloaterSoundsList::setDirty()
{
	HBFloaterSoundsList* self = findInstance();
	if (self)
	{
		self->mIsDirty = true;
		self->mLastUpdate = 0.f;		// Force an immediate update
		self->setButtonsStatus();
	}
}

//static
void HBFloaterSoundsList::processObjectPropertiesFamily(LLMessageSystem* msg)
{
	HBFloaterSoundsList* self = findInstance();

	if (!msg || !self) return;

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	if (!self->mRequests.count(object_id))
	{
		// Object data not requested by us.
		return;
	}
	self->mRequests.erase(object_id);

	LL_DEBUGS("SoundsList") << "Got info for object: " << object_id << LL_ENDL;

	LLUUID owner_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, owner_id);
	if (owner_id.isNull())
	{
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, owner_id);
		sGroupOwnedObjects.emplace(object_id, owner_id);
	}
	else
	{
		LLViewerObject* obj = gObjectList.findObject(object_id);
		if (obj && obj->mOwnerID.isNull())
		{
			LL_DEBUGS("SoundsList") << "Setting the owner in LLViewerObject to: "
									<< owner_id << LL_ENDL;
			obj->mOwnerID = owner_id;
		}
	}

	std::string name;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	sObjectNames.emplace(object_id, name);

	self->mIsDirty = true;
}

//static
void HBFloaterSoundsList::newRegion()
{
	// We changed region so we can clear the object names cache.
	sObjectNames.clear();
	sGroupOwnedObjects.clear();
	HBFloaterSoundsList* self = findInstance();
	if (self)
	{
		self->mIsDirty = true;
		self->mLastUpdate = 0.f;	// Force an immediate update
	}
}

//static
LLVector3d HBFloaterSoundsList::selectedLocation()
{
	HBFloaterSoundsList* self = findInstance();
	return self ? self->mSelectedLocation : LLVector3d();
}

//static
void HBFloaterSoundsList::onPlaySoundBtn(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (!self || !ctrl || !gWindowp) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	// Get the sound id
	LLUUID sound_id = item->getColumn(LIST_SOUND)->getValue().asUUID();
	if (ctrl->getValue().asString() == "copy_id")
	{
		gWindowp->copyTextToClipboard(utf8str_to_wstring(sound_id.asString()));
	}
	else if (gAudiop)
	{
		gAudiop->triggerSound(sound_id, gAgentID, 1.f,
							  LLAudioEngine::AUDIO_TYPE_SFX);
	}
}

//static
void HBFloaterSoundsList::onBlockSoundBtn(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (!self || !ctrl) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	const LLUUID& sound_id = item->getColumn(LIST_SOUND)->getValue().asUUID();
	bool blocked = LLAudioData::isBlockedSound(sound_id);

	if (ctrl->getValue().asString() == "block_all_same_owner")
	{
		const LLUUID& owner_id =
			item->getColumn(LIST_OWNER_ID)->getValue().asUUID();
		if (owner_id.isNull()) return;

		std::vector<LLScrollListItem*> list = self->mSoundsList->getAllData();
		for (U32 i = 0, count = list.size(); i < count; ++i)
		{
			item = list[i];
			if (item->getColumn(LIST_OWNER_ID)->getValue().asUUID() ==
					owner_id)
			{
				const LLUUID& id =
					item->getColumn(LIST_SOUND)->getValue().asUUID();
				LLAudioData::blockSound(id, !blocked);
			}
		}
	}
	else if (ctrl->getValue().asString() == "block_all_same_name")
	{
		const LLUUID& object_id =
			item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
		if (object_id.isNull()) return;

		std::string obj_name =
			item->getColumn(LIST_OBJECT)->getValue().asString();
		if (obj_name.empty() || obj_name == self->mLoadingString)
		{
			return;
		}

		std::vector<LLScrollListItem*> list = self->mSoundsList->getAllData();
		for (U32 i = 0, count = list.size(); i < count; ++i)
		{
			item = list[i];
			if (item->getColumn(LIST_OBJECT)->getValue().asString() ==
					obj_name)
			{
				const LLUUID& id =
					item->getColumn(LIST_SOUND)->getValue().asUUID();
				LLAudioData::blockSound(id, !blocked);
			}
		}
	}
	else
	{
		LLAudioData::blockSound(sound_id, !blocked);
	}

	self->mIsDirty = true;
	self->mLastUpdate = 0.f;		// Force an immediate update
	self->setButtonsStatus();
}

//static
void HBFloaterSoundsList::onMuteOwnerBtn(void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (!self) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	const LLUUID& owner_id =
		item->getColumn(LIST_OWNER_ID)->getValue().asUUID();
	if (owner_id.isNull()) return;

	std::string name;
	if (gCacheNamep)
	{
		gCacheNamep->getFullName(owner_id, name);
	}

	LLMute mute(owner_id, name, LLMute::AGENT);
	if (LLMuteList::isMuted(mute.mID, LLMute::flagObjectSounds))
	{
		LLMuteList::remove(mute, LLMute::flagObjectSounds);
	}
	else if (LLMuteList::add(mute, LLMute::flagObjectSounds))
	{
		LLFloaterMute::selectMute(mute.mID);
	}
}

//static
void HBFloaterSoundsList::onShowSourceBtn(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
  	if (!self || !ctrl) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	const LLUUID& object_id =
		item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
	if (object_id.isNull()) return;

	if (ctrl->getValue().asString() == "inspect")
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp)
		{
			LLFloaterInspect::show(objectp);
		}
	}
	else
	{
		gAgent.lookAtObject(object_id, CAMERA_POSITION_OBJECT);
	}
}

//static
void HBFloaterSoundsList::onMuteObjectBtn(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (!self || !ctrl) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	const LLUUID& object_id =
		item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
	if (object_id.isNull()) return;

	std::string obj_name;
	names_map_t::iterator it = sObjectNames.find(object_id);
	if (it != sObjectNames.end())
	{
		obj_name = it->second;
	}

	if (ctrl->getValue().asString() == "mute_by_name")
	{
		LLMute mute(LLUUID::null, obj_name, LLMute::BY_NAME);
		if (LLMuteList::isMuted(LLUUID::null, mute.mName))
		{
			LLMuteList::remove(mute);
		}
		else if (LLMuteList::add(mute))
		{
			LLFloaterMute::selectMute(mute.mName);
		}
	}
	else
	{
		LLMute mute(object_id, obj_name, LLMute::OBJECT);
		if (LLMuteList::isMuted(mute.mID, mute.mName))
		{
			LLMuteList::remove(mute);
		}
		else if (LLMuteList::add(mute))
		{
			LLFloaterMute::selectMute(mute.mID);
		}
	}
}

//static
void HBFloaterSoundsList::onSelectSound(LLUICtrl*, void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (self)
	{
		self->setButtonsStatus();
	}
}

//static
void HBFloaterSoundsList::onDoubleClick(void* userdata)
{
	HBFloaterSoundsList* self = (HBFloaterSoundsList*)userdata;
 	if (!self || !gAudiop) return;

	LLScrollListItem* item = self->mSoundsList->getFirstSelected();
	if (!item) return;

	std::string name;
	LLVector3d pos_global;

	// Get the object id
	const LLUUID& object_id =
		item->getColumn(LIST_OBJECT_ID)->getValue().asUUID();
	if (object_id.notNull())
	{
		// Try to track the most up-to-date object position
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp)
		{
			pos_global = objectp->getPositionGlobal();
		}
		// Get the object name
		names_map_t::iterator it = sObjectNames.find(object_id);
		if (it != sObjectNames.end())
		{
			name = it->second;
		}
	}

	// Get the source id
	const LLUUID& source_id =
		item->getColumn(LIST_SOURCE_ID)->getValue().asUUID();
	if (pos_global.isExactlyZero())
	{
		// Find the source (if still there) and its position
		for (LLAudioEngine::source_map_t::const_iterator
				it = gAudiop->mAllSources.begin(),
				end = gAudiop->mAllSources.end();
			 it != end; ++it)
		{
			const LLAudioSource* source = it->second;
			if (source && source->getID() == source_id)
			{
				pos_global = source->getPositionGlobal();
				break;
			}
		}
	}

	if (pos_global.isExactlyZero())
	{
		// Source gone or ambient sound (cannot track)... Give-up.
		return;
	}

	self->mTrackingLocation = pos_global;
	self->mTrackingID = object_id.notNull() ? object_id : source_id;

	if (name.empty())
	{
		name = self->getString("sound_source");
	}
	gTracker.trackLocation(self->mTrackingLocation, name, "",
						   LLTracker::LOCATION_ITEM);

	self->mIsDirty = true;
	self->mLastUpdate = 0.f;	// Force an immediate update
}
