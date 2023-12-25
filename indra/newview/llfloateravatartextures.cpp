/**
 * @file llfloateravatartextures.cpp
 * @brief Debugging view showing underlying avatar textures and baked textures.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llfloateravatartextures.h"

#include "imageids.h"
#include "llcachename.h"
#include "llspinctrl.h"
#include "lltexturectrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
//MK
#include "mkrlinterface.h"
//mk
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"

using namespace LLAvatarAppearanceDefines;

LLFloaterAvatarTextures::instances_map_t LLFloaterAvatarTextures::sInstances;

//static
LLFloaterAvatarTextures* LLFloaterAvatarTextures::show(const LLUUID& id)
{
	LLFloaterAvatarTextures* self;
	if (sInstances.count(id))
	{
		self = sInstances[id];
		self->open();
	}
	else
	{
		self = new LLFloaterAvatarTextures(id);
	}
	return self;
}

LLFloaterAvatarTextures::LLFloaterAvatarTextures(const LLUUID& id)
:	mID(id),
	mLastRefresh(0.f),
	mShallClose(false)
{
	sInstances[id] = this;
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_avatar_textures.xml");
}

LLFloaterAvatarTextures::~LLFloaterAvatarTextures()
{
	sInstances.erase(mID);
}

bool LLFloaterAvatarTextures::postBuild()
{
	mTitle = getTitle();

	for (U32 i = 0; i < TEX_NUM_INDICES; ++i)
	{
		const LLAvatarAppearanceDictionary::TextureEntry* te =
			gAvatarAppDictp->getTexture(ETextureIndex(i));
		if (te)
		{
			LLTextureCtrl* ctrl = getChild<LLTextureCtrl>(te->mName.c_str());
			if (ctrl)
			{
				ctrl->setCaptionAlwaysEnabled();
			}
			mTextures[i] = ctrl;
		}
		else
		{
			mTextures[i] = NULL;
		}
	}

	childSetAction("Dump", onClickDump, this);
	childSetAction("Rebake", onClickRebake, this);

	mSpinLayer = getChild<LLSpinCtrl>("layer");
	if (mID == gAgentID)
	{
		mSpinLayer->setCommitCallback(onCommitLayer);
		mSpinLayer->setCallbackUserData(this);
	}
	else
	{
		mSpinLayer->setEnabled(false);
	}

	center();

	gFloaterViewp->adjustToFitScreen(this);

	refresh();

	return true;
}

void LLFloaterAvatarTextures::draw()
{
	if (gFrameTimeSeconds - mLastRefresh > 1.f)
	{
		refresh();
	}
	if (mShallClose)
	{
		close();
	}
	else
	{
		LLFloater::draw();
	}
}

//static
S32 LLFloaterAvatarTextures::getTextureIds(LLVOAvatar* avatarp,
										   ETextureIndex te,
										   std::string& name, uuid_vec_t& ids)
{
	ids.clear();
	LLWearableType::EType wtype = LLWearableType::WT_NONE;
	const LLAvatarAppearanceDictionary::TextureEntry* tex_entry =
		gAvatarAppDictp->getTexture(te);
	if (tex_entry)
	{
		wtype = tex_entry->mWearableType;
		if (tex_entry->mIsLocalTexture && avatarp->isSelf())
		{
			U32 count = gAgentWearables.getWearableCount(wtype);
			for (U32 layer = 0; layer < count; ++layer)
			{
				LLViewerWearable* wearable =
					gAgentWearables.getViewerWearable(wtype, layer);
				if (wearable)
				{
					LLLocalTextureObject* lto =
						wearable->getLocalTextureObject(te);
					if (lto)
					{
						ids.push_back(lto->getID());
					}
				}
			}
		}
		else
		{
			ids.push_back(avatarp->getTE(te)->getID());
		}
		name = tex_entry->mName;
	}
	else
	{
		name.clear();
	}
	while (ids.size() < 5)
	{
		ids.push_back(IMG_DEFAULT_AVATAR);
	}
	if (wtype == LLWearableType::WT_INVALID)
	{
		// Easier to test (since negative and thus <= WT_EYES) for refresh()
		wtype = LLWearableType::WT_NONE;
	}
	return (S32)wtype;
}

void LLFloaterAvatarTextures::refresh()
{
	bool can_view = gAgent.isGodlikeWithoutAdminMenuFakery() ||
					(mID == gAgentID && enable_avatar_textures(NULL));
	LLVOAvatar* avatarp = gObjectList.findAvatar(mID);
	if (!can_view || !avatarp)
	{
		mShallClose = true;
		return;
	}

	std::string title = mTitle;
//MK
	if (mID == gAgentID || !gRLenabled ||
		!(gRLInterface.mContainsShownames ||
		  gRLInterface.mContainsShownametags))
//mk
	{
		std::string fullname;
		if (gCacheNamep &&
			gCacheNamep->getFullName(avatarp->getID(), fullname))
		{
			title += ": " + fullname;
		}
		else
		{
			title += ": " + mID.asString();
		}
	}
	setTitle(title);

	std::string te_name;
	uuid_vec_t ids;
	U32 layer = llmin((U32)mSpinLayer->get(), 4U);
	for (U32 i = 0; i < TEX_NUM_INDICES; ++i)
	{
		LLTextureCtrl* ctrl = mTextures[i];
		if (!ctrl) continue;

		S32 type = getTextureIds(avatarp, ETextureIndex(i), te_name, ids);
		const LLUUID& id =
			// There is only one layer for baked textures and body parts...
			type <= (S32)LLWearableType::WT_EYES ? ids[0] : ids[layer];
		if (id == IMG_DEFAULT_AVATAR)
		{
			ctrl->setImageAssetID(LLUUID::null);
			ctrl->setToolTip("");
		}
		else
		{
			ctrl->setImageAssetID(id);
			ctrl->setToolTip(te_name + ": " + id.asString());
		}
		ctrl->setEnabled(false);
	}

	mLastRefresh = gFrameTimeSeconds;
}

//static
void LLFloaterAvatarTextures::onClickDump(void* data)
{
	LLFloaterAvatarTextures* self = (LLFloaterAvatarTextures*)data;
	if (!self) return;

	LLVOAvatar* avatarp = gObjectList.findAvatar(self->mID);
	bool can_view = gAgent.isGodlikeWithoutAdminMenuFakery() ||
					(self->mID == gAgentID && enable_avatar_textures(NULL));
	if (can_view && avatarp)
	{
		std::string te_name;
		uuid_vec_t ids;
		for (S32 i = 0, count = avatarp->getNumTEs(); i < count; ++i)
		{
			getTextureIds(avatarp, ETextureIndex(i), te_name, ids);
			for (U32 layer = 0; layer < 5; ++layer)
			{
				const LLUUID& id = ids[layer];
				if (id != IMG_DEFAULT_AVATAR)
				{
					llinfos << "Avatar texture " << te_name << ", layer "
							<< layer << ". Id: " << id << llendl;
				}
			}
		}
	}
}

//static
void LLFloaterAvatarTextures::onClickRebake(void* data)
{
	LLFloaterAvatarTextures* self = (LLFloaterAvatarTextures*)data;
	LLVOAvatar* avatarp = gObjectList.findAvatar(self->mID);
	if (self && avatarp)
	{
		if (avatarp == gAgentAvatarp)
		{
			handle_rebake_textures(NULL);
		}
		else
		{
			handle_refresh_avatar(avatarp, false);
		}
	}
}

void LLFloaterAvatarTextures::onCommitLayer(LLUICtrl*, void* userdata)
{
	LLFloaterAvatarTextures* self = (LLFloaterAvatarTextures*)userdata;
	if (self)
	{
		self->refresh();
	}
}
