/**
 * @file llfloateravatarinfo.cpp
 * @brief LLFloaterAvatarInfo class implementation
 * Avatar information as shown in a floating window from right-click
 * Profile.  Used for editing your own avatar info.  Just a wrapper
 * for LLPanelAvatar, shared with the Find directory.
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
#include "llviewerprecompiledheaders.h"

#include "llfloateravatarinfo.h"

#include "llinventory.h"
#include "llnotifications.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "llfloaterinventory.h"
#include "llviewercontrol.h"
#include "llweb.h"

LLFloaterAvatarInfo::instances_map_t LLFloaterAvatarInfo::sInstances;

//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------

class LLProfileHandler final : public LLCommandHandler
{
public:
	LLProfileHandler()
	:	LLCommandHandler("profile", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (params.size() < 1) return false;

		std::string agent_name = params[0];
		std::string url = LLFloaterAvatarInfo::getProfileURL(agent_name);
		if (url.empty())
		{
			LLSD args;
			args["NAME"] = agent_name;
			gNotifications.add("NoWebProfile", args);
		}
		else
		{
			llinfos << "Opening web profile of: " << agent_name << llendl;
			LLWeb::loadURL(url);
		}

		return true;
	}
};
LLProfileHandler gProfileHandler;

class LLShareWithAvatarHandler : public LLCommandHandler
{
public:
	LLShareWithAvatarHandler()
	:	LLCommandHandler("sharewithavatar", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl* web)
	{
		// Make sure we have some parameters
		if (params.size() == 0)
		{
			return false;
		}

		// Get the ID
		LLUUID id;
		if (!id.set(params[0], false))
		{
			return false;
		}

		// Select the 2nd Life tab in the profile panel.
		LLFloaterAvatarInfo::showFromObject(id, "2nd Life");
		// Open the inventory floater and/or bring it to front
		LLFloaterInventory::showAgentInventory();
		// Give some clue to the user as what to do now
		gNotifications.add("ShareInventory");
		return true;
	}
};
LLShareWithAvatarHandler gShareWithAvatar;

class LLPickHandler final : public LLCommandHandler
{
public:
	LLPickHandler()
	:	LLCommandHandler("pick", UNTRUSTED_THROTTLE)
	{
	}

	bool canHandleUntrusted(const LLSD& params, const LLSD&,
							LLMediaCtrl*, const std::string& nav_type) override
	{
		if (!params.size())
		{
			return true;	// Do not block; it will fail later in handle()
		}

		if (nav_type == "clicked" || nav_type == "external")
		{
			return true;
		}

		return params[0].asString() != "create";
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		// Make sure we have some parameters
		if (!params.size())
		{
			return false;
		}

		// *TODO: implement pick selection by UUID (and move to
		// llpanelpick.cpp ?). For now, simply select the Picks tab in the
		// profile panel.
		llinfos << "STUB code for URI secondlife://app/pick/ - Selecting Picks tab in avatar profile."
				<< llendl;
		LLFloaterAvatarInfo::showFromObject(gAgentID, "Picks");
		return true;
	}
};
LLPickHandler gPickHandler;

//-----------------------------------------------------------------------------
// LLFloaterAvatarInfo class
//-----------------------------------------------------------------------------

void* LLFloaterAvatarInfo::createPanelAvatar(void* data)
{
	LLFloaterAvatarInfo* self = (LLFloaterAvatarInfo*)data;
	self->mPanelAvatarp = new LLPanelAvatar("PanelAv", LLRect(), true);
	return self->mPanelAvatarp;
}

bool LLFloaterAvatarInfo::postBuild()
{
	return true;
}

LLFloaterAvatarInfo::LLFloaterAvatarInfo(const LLUUID& avatar_id)
:	LLPreview("avatarinfo"),
	mAvatarID(avatar_id),
	mSuggestedOnlineStatus(ONLINE_STATUS_NO)
{
	setAutoFocus(true);

	LLCallbackMap::map_t factory_map;

	factory_map["Panel Avatar"] = LLCallbackMap(createPanelAvatar, this);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_profile.xml",
												 &factory_map);

	if (mPanelAvatarp)
	{
		mPanelAvatarp->selectTab(0);
	}

	// Must be done before callback below is called.
	sInstances[avatar_id] = this;

	LLAvatarNameCache::get(avatar_id,
						   boost::bind(&callbackLoadAvatarName, _1, _2));
}

//virtual
LLFloaterAvatarInfo::~LLFloaterAvatarInfo()
{
	// Child views automatically deleted
	sInstances.erase(mAvatarID);
}

void LLFloaterAvatarInfo::listAgentGroups()
{
	if (mAvatarID == gAgentID)
	{
		mPanelAvatarp->listAgentGroups();
	}
}

//static
LLFloaterAvatarInfo* LLFloaterAvatarInfo::show(const LLUUID& avatar_id)
{
	if (avatar_id.isNull())
	{
		return NULL;
	}

	LLFloaterAvatarInfo* floater;
	instances_map_t::iterator it = sInstances.find(avatar_id);
	if (it != sInstances.end())
	{
		// Bring the existing floater to front
		floater = it->second;
		floater->open();
	}
	else
	{
		floater = new LLFloaterAvatarInfo(avatar_id);
		floater->open();
	}
	return floater;
}

// Open profile to a certain tab.
//static
void LLFloaterAvatarInfo::showFromObject(const LLUUID& avatar_id,
										 std::string tab_name)
{
	LLFloaterAvatarInfo* floater = show(avatar_id);
	if (floater)
	{
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null,
											ONLINE_STATUS_NO);
		floater->mPanelAvatarp->selectTabByName(tab_name);
	}
}

//static
void LLFloaterAvatarInfo::showFromDirectory(const LLUUID& avatar_id)
{
	LLFloaterAvatarInfo* floater = show(avatar_id);
	if (floater)
	{
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null,
											ONLINE_STATUS_NO);
	}
}

//static
void LLFloaterAvatarInfo::showFromFriend(const LLUUID& agent_id, bool online)
{
	LLFloaterAvatarInfo* floater = show(agent_id);
	if (floater)
	{
		floater->mSuggestedOnlineStatus = online ? ONLINE_STATUS_YES
												 : ONLINE_STATUS_NO;
	}
}

//static
void LLFloaterAvatarInfo::showFromProfile(const LLUUID& avatar_id, LLRect rect)
{
	if (avatar_id.isNull())
	{
		return;
	}

	LLFloaterAvatarInfo* floater;
	instances_map_t::iterator it = sInstances.find(avatar_id);
	if (it != sInstances.end())
	{
		// Use the existing floater
		floater = it->second;
	}
	else
	{
		floater = new LLFloaterAvatarInfo(avatar_id);
		floater->translate(rect.mLeft - floater->getRect().mLeft + 16,
						   rect.mTop - floater->getRect().mTop - 16);
		floater->mPanelAvatarp->setAvatarID(avatar_id, LLStringUtil::null,
											ONLINE_STATUS_NO);
	}
	if (floater)
	{
		floater->open();
	}
}

void LLFloaterAvatarInfo::showProfileCallback(S32 option, void *userdata)
{
	if (option == 0)
	{
		showFromObject(gAgentID);
	}
}

//static
void LLFloaterAvatarInfo::callbackLoadAvatarName(const LLUUID& id,
												 const LLAvatarName& avatar_name)
{
	LLFloaterAvatarInfo* floater = get_ptr_in_map(sInstances, id);
	if (floater)
	{
		// Build a new title including the avatar name.
		std::ostringstream title;
		if (LLAvatarNameCache::useDisplayNames())
		{
			// Always show "Display Name [Legacy Name]" for security reasons
			title << avatar_name.getNames() << " - " << floater->getTitle();
		}
		else
		{
			title << avatar_name.getLegacyName() << " - "
				  << floater->getTitle();
		}
		floater->setTitle(title.str());
	}
}

//virtual
void LLFloaterAvatarInfo::draw()
{
	// skip LLPreview::draw()
	LLFloater::draw();
}

//virtual
bool LLFloaterAvatarInfo::canClose()
{
	return mPanelAvatarp && mPanelAvatarp->canClose();
}

LLFloaterAvatarInfo* LLFloaterAvatarInfo::getInstance(const LLUUID& id)
{
	return get_ptr_in_map(sInstances, id);
}

void LLFloaterAvatarInfo::loadAsset()
{
	if (mPanelAvatarp)
	{
		mPanelAvatarp->setAvatarID(mAvatarID, LLStringUtil::null,
								   mSuggestedOnlineStatus);
		mAssetStatus = PREVIEW_ASSET_LOADING;
	}
}

LLPreview::EAssetStatus LLFloaterAvatarInfo::getAssetStatus()
{
	if (mPanelAvatarp && mPanelAvatarp->haveData())
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}

//static
std::string LLFloaterAvatarInfo::getProfileURL(const std::string& user_name)
{
	std::string url = gSavedSettings.getString("WebProfileURL");
	bool sl_profile = url.find("secondlife") != std::string::npos;
	if ((gIsInSecondLife && !sl_profile) || (!gIsInSecondLife && sl_profile))
	{
		return "";
	}
	LLStringUtil::format_map_t subs;
	subs["[AGENT_NAME]"] = user_name;
	url = LLWeb::expandURLSubstitutions(url, subs);
	LLStringUtil::toLower(url);
	return url;
}
