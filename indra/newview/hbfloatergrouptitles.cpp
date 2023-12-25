/**
 * @file hbfloatergrouptitles.cpp
 * @brief HBFloaterGroupTitles class implementation
 *
 * This class implements a floater where all available group titles are
 * listed, allowing the user to activate any via simple double-click.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Henri Beauchamp.
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

#include "hbfloatergrouptitles.h"

#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "llmessage.h"
#include "roles_constants.h"

#include "llagent.h"

using namespace LLOldEvents;

// HBFloaterGroupTitlesObserver class

HBFloaterGroupTitlesObserver::HBFloaterGroupTitlesObserver(HBFloaterGroupTitles* instance,
														   const LLUUID& group_id)
:	LLGroupMgrObserver(group_id),
	mFloaterInstance(instance)
{
	gGroupMgr.addObserver(this);
}

//virtual
HBFloaterGroupTitlesObserver::~HBFloaterGroupTitlesObserver()
{
	gGroupMgr.removeObserver(this);
}

//virtual
void HBFloaterGroupTitlesObserver::changed(LLGroupChange gc)
{
	if (gc != GC_PROPERTIES)
	{
		mFloaterInstance->setDirty();
	}
}

// HBFloaterGroupTitles class

HBFloaterGroupTitles::HBFloaterGroupTitles(const LLSD&)
:	mIsDirty(true)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_group_titles.xml");
	gAgent.addListener(this, "new group");
}

//virtual
HBFloaterGroupTitles::~HBFloaterGroupTitles()
{
	gAgent.removeListener(this);
	observers_map_t::iterator it;
	while (!mObservers.empty())
	{
		it = mObservers.begin();
		delete it->second;
		mObservers.hmap_erase(it);
	}
}

//virtual
bool HBFloaterGroupTitles::postBuild()
{
	mTitlesList = getChild<LLScrollListCtrl>("titles_list");
	mTitlesList->setDoubleClickCallback(onActivate);
	mTitlesList->setCallbackUserData(this);

	childSetAction("close", onCloseButtonPressed, this);
	childSetAction("refresh", onRefreshButtonPressed, this);
	childSetAction("activate", onActivate, this);

	return true;
}

//virtual
void HBFloaterGroupTitles::draw()
{
	if (mIsDirty)
	{
		const LLUUID& current_group_id = gAgent.getGroupID();
		LLUUID highlight_id;
		std::string style;
		LLSD element;

		S32 scrollpos = mTitlesList->getScrollPos();
		mTitlesList->deleteAllItems();

		for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
		{
			LLGroupData* gdatap = &gAgent.mGroups[i];
			if (!gdatap) continue;	// Paranoia

			const LLUUID& id = gdatap->mID;

			// Add an observer for this group if there is none so far.
			if (mObservers.find(id) == mObservers.end())
			{
				HBFloaterGroupTitlesObserver* obs =
					new HBFloaterGroupTitlesObserver(this, id);
				mObservers.emplace(id, obs);
			}
			LLGroupMgrGroupData* mgrdatap = gGroupMgr.getGroupData(id);
			if (!mgrdatap)
			{
				gGroupMgr.sendGroupTitlesRequest(id);
				continue;
			}
			for (S32 j = 0, count2 = mgrdatap->mTitles.size(); j < count2; ++j)
			{
				const LLGroupTitle& title = mgrdatap->mTitles[j];
				style = "NORMAL";
				if (current_group_id == id && title.mSelected)
				{
					style = "BOLD";
					highlight_id = title.mRoleID;
				}
				element["id"] = title.mRoleID;
				element["columns"][LIST_TITLE]["column"] = "title";
				element["columns"][LIST_TITLE]["value"] = title.mTitle;
				element["columns"][LIST_TITLE]["font-style"] = style;
				element["columns"][LIST_GROUP_NAME]["column"] = "group_name";
				// NOTE/FIXME: mgrdatap->mName is apparently always empty !
				element["columns"][LIST_GROUP_NAME]["value"] = gdatap->mName;
				element["columns"][LIST_GROUP_NAME]["font-style"] = style;
				element["columns"][LIST_GROUP_ID]["column"] = "group_id";
				element["columns"][LIST_GROUP_ID]["value"] = id;
				mTitlesList->addElement(element, ADD_SORTED);
			}
		}

		// Add "none" at top of list
		style = "NORMAL";
		if (current_group_id.isNull())
		{
			style = "BOLD";
		}
		element["id"] = LLUUID::null;
		element["columns"][LIST_TITLE]["column"] = "title";
		element["columns"][LIST_TITLE]["value"] = "none";
		element["columns"][LIST_TITLE]["font-style"] = style;
		element["columns"][LIST_GROUP_NAME]["column"] = "group_name";
		element["columns"][LIST_GROUP_NAME]["value"] = "none";
		element["columns"][LIST_GROUP_NAME]["font-style"] = style;
		element["columns"][LIST_GROUP_ID]["column"] = "group_id";
		element["columns"][LIST_GROUP_ID]["value"] = LLUUID::null;
		mTitlesList->addElement(element, ADD_TOP);

		mTitlesList->setScrollPos(scrollpos);
		mTitlesList->selectByValue(highlight_id);
		mIsDirty = false;
	}

	LLFloater::draw();
}

//static
void HBFloaterGroupTitles::onCloseButtonPressed(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterGroupTitles::onRefreshButtonPressed(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*)userdata;
	if (!self) return;

	observers_map_t::iterator it;
	while (!self->mObservers.empty())
	{
		it = self->mObservers.begin();
		delete it->second;
		self->mObservers.hmap_erase(it);
	}

	LLGroupMgr::debugClearAllGroups(NULL);
	self->setDirty();
}

//static
void HBFloaterGroupTitles::onActivate(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*)userdata;
	if (!self) return;

	LLScrollListItem* item = self->mTitlesList->getFirstSelected();
	if (!item) return;

	// Get the group id
	LLUUID group_id = item->getColumn(LIST_GROUP_ID)->getValue().asUUID();

	// Set the title for this group
	gGroupMgr.sendGroupTitleUpdate(group_id, item->getUUID());

	// Set the group if needed.
	const LLUUID& old_group_id = gAgent.getGroupID();
	if (group_id != old_group_id)
	{
		gAgent.setGroup(group_id);
	}
	else
	{
		// Force a refresh via the observer
		if (group_id.isNull())
		{
			group_id = old_group_id;
		}
		gGroupMgr.sendGroupTitlesRequest(group_id);
	}
}

// LLSimpleListener
//virtual
bool HBFloaterGroupTitles::handleEvent(LLPointer<LLEvent> event,
									   const LLSD& userdata)
{
	if (event.notNull() && event->desc() == "new group")
	{
		setDirty();
		return true;
	}
	return false;
}
