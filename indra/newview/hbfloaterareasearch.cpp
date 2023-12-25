/**
 * @file hbfloaterareasearch.cpp
 * @brief HBFloaterAreaSearch class implementation
 *
 * This class implements a floater where all surroundind objects are listed.
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Original code Copyright (c) 2009 Modular Systems Ltd. All rights reserved.
 * Rewritten/augmented code Copyright (c) 2010-2019 Henri Beauchamp.
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
 *
 * Revision history:
 * - Initial backport from Emerald viewer, modified, debugged, optimized and
 *   improved by Henri Beauchamp - Feb 2010.
 * - Speed optimization by Henri Beauchamp - Jul 2011.
 * - Tracked object info added by Henri Beauchamp - Nov 2011.
 * - Further modified and augmented with mutes, derender, report & inspect
 *   functions. Henri Beauchamp - May 2013.
 * - Full rewrite by Henri Beauchamp - Aug 2014.
 * - Another partial rewrite by Henri Beauchamp - Nov 2016.
 * - Yet another large rewrite, with proper exclusion of attachments and
 *   viewer-side only objects to avoid fake "pending" requests and to prevent
 *   sending to the server the bogus/spammy/useless messages corresponding to
 *   them. Fixed the bug that caused group-owned objects to stay forever in the
 *   "pending" requests list. Added the "Show", "Mute particles", "Mute owner"
 *   and "Copy UUID" action. Changed input lines for search input lines.
 *   Henri Beauchamp - Jan 2019.
 * - On "Refresh", clear the cached objects list so that any renamed object
 *   will be properly refreshed. Henri Beauchamp - Apr 2021.
 */

#include "llviewerprecompiledheaders.h"

#include "hbfloaterareasearch.h"

#include "llcachename.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llfloaterinspect.h"
#include "llfloatermute.h"
#include "llfloaterreporter.h"
#include "lltracker.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"			// For gWindowp

// static variables
F32 HBFloaterAreaSearch::sLastUpdateTime = 0.f;
bool HBFloaterAreaSearch::sIsDirty = false;
bool HBFloaterAreaSearch::sUpdateDone = false;
bool HBFloaterAreaSearch::sTracking = false;
LLUUID HBFloaterAreaSearch::sTrackingObjectID;
LLVector3d HBFloaterAreaSearch::sTrackingLocation;
std::string HBFloaterAreaSearch::sTrackingInfoLine;
HBFloaterAreaSearch::object_details_map_t HBFloaterAreaSearch::sObjectDetails;

// Minimum interval between idle updates (and list refreshes) in seconds:
constexpr F32 MIN_REFRESH_INTERVAL = 0.25f;
// Interval of time between auto-refresh of stalled objects details requests in
// seconds:
constexpr F32 AUTO_REFRESH_INTERVAL = 10.f;

HBFloaterAreaSearch::HBFloaterAreaSearch(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_area_search.xml");
}

//virtual
bool HBFloaterAreaSearch::postBuild()
{
	mResultsList = getChild<LLScrollListCtrl>("result_list");
	mResultsList->setCommitCallback(onSelectResult);
	mResultsList->setDoubleClickCallback(onDoubleClickResult);
	mResultsList->setCallbackUserData(this);
	mResultsList->sortByColumn("name", true);

	mCounterText = getChild<LLTextBox>("counter");
	if (sTracking)
	{
		mCounterText->setText(sTrackingInfoLine);
	}

	mMuteFlyoutBtn = getChild<LLFlyoutButton>("mute");
	mMuteFlyoutBtn->setCommitCallback(onClickMute);
	mMuteFlyoutBtn->setCallbackUserData(this);

	mDerenderBtn = getChild<LLButton>("derender");
	mDerenderBtn->setClickedCallback(onClickDerender, this);
	
	mReportBtn = getChild<LLButton>("report");
	mReportBtn->setClickedCallback(onClickReport, this);

	mShowBtn = getChild<LLButton>("show");
	mShowBtn->setClickedCallback(onClickShow, this);

	mInspectFlyoutBtn = getChild<LLFlyoutButton>("inspect");
	mInspectFlyoutBtn->setCommitCallback(onClickInspect);
	mInspectFlyoutBtn->setCallbackUserData(this);

	mRefreshBtn = getChild<LLButton>("refresh");
	mRefreshBtn->setClickedCallback(onClickRefresh, this);

	childSetAction("close", onClickClose, this);

	mNameInputLine = getChild<LLSearchEditor>("name_query");
	mNameInputLine->setSearchCallback(onSearchEdit, mNameInputLine);

	mDescInputLine = getChild<LLSearchEditor>("desc_query");
	mDescInputLine->setSearchCallback(onSearchEdit, mDescInputLine);

	mOwnerInputLine = getChild<LLSearchEditor>("owner_query");
	mOwnerInputLine->setSearchCallback(onSearchEdit, mOwnerInputLine);

	mGroupInputLine = getChild<LLSearchEditor>("group_query");
	mGroupInputLine->setSearchCallback(onSearchEdit, mGroupInputLine);

	sIsDirty = true;

	return true;
}

//virtual
void HBFloaterAreaSearch::draw()
{
	if (sTracking)
	{
		F32 dist = 3.f;
		if (gTracker.getTrackingStatus() == LLTracker::TRACKING_LOCATION)
		{
			dist = fabsf((F32)(gTracker.getTrackedPositionGlobal() -
							   sTrackingLocation).length());
		}
		if (dist > 2.f)
		{
			// Tracker stopped or tracking another location
			sTracking = false;
			sIsDirty = true;
			sTrackingInfoLine.clear();
		}
	}

	if (sIsDirty && sUpdateDone && getVisible() && !isMinimized())
	{
		uuid_vec_t selected = mResultsList->getSelectedIDs();
		S32 scrollpos = mResultsList->getScrollPos();
		mResultsList->deleteAllItems();

		LLViewerObject* objectp;
		const LLViewerRegion* our_region = gAgent.getRegion();

		bool searching_uuid = mSearchUUID.notNull();
		if (searching_uuid)
		{
			objectp = gObjectList.findObject(mSearchUUID);
			if (objectp)
			{
				objectp = objectp->getRootEdit();
			}
			if (isObjectOfInterest(objectp) && !objectp->isDead() &&
				objectp->getRegion() == our_region)
			{
				const LLUUID& object_id = objectp->getID();
				if (checkObjectDetails(object_id))
				{
					addInResultsList(object_id, false);
				}
			}
		}

		S32 total= 0;
		S32 pending = 0;
		for (object_details_map_t::iterator it = sObjectDetails.begin();
			 it != sObjectDetails.end(); )
		{
			object_details_map_t::iterator cur_it = it++;
			const LLUUID& object_id = cur_it->first;
			objectp = gObjectList.findObject(object_id);
			if (!objectp || objectp->isDead() ||
				objectp->getRegion() != our_region)
			{
				sObjectDetails.erase(cur_it);
				continue;
			}
			if (!cur_it->second.valid())
			{
				++pending;
			}
			else if (!searching_uuid)
			{
				addInResultsList(object_id, true);
			}
			++total;
		}

		mResultsList->sortItems();
		mResultsList->selectMultiple(selected);
		mResultsList->setScrollPos(scrollpos);

		if (!sTracking)
		{
			mCounterText->setText(llformat("%d listed/%d pending/%d total",
								  mResultsList->getItemCount(), pending,
								  total));
		}

		setButtonsStatus();

		sIsDirty = sUpdateDone = false;
	}

	LLFloater::draw();
}

void HBFloaterAreaSearch::addInResultsList(const LLUUID& object_id,
										   bool match_filters)
{
	HBObjectDetails* details = &sObjectDetails[object_id];
	if (!details->valid())
	{
		// We did not yet receive the details for this object.
		return;
	}

	std::string tmp;
	std::string object_name = details->name;
	if (match_filters && !mSearchedName.empty())
	{
		tmp = object_name;
		LLStringUtil::toLower(tmp);
		if (tmp.find(mSearchedName) == std::string::npos)
		{
			return;	// Failed name filter match
		}
	}

	std::string object_desc = details->desc;
	if (match_filters && !mSearchedDesc.empty())
	{
		tmp = object_desc;
		LLStringUtil::toLower(tmp);
		if (tmp.find(mSearchedDesc) == std::string::npos)
		{
			return;	// Failed description filter match
		}
	}

	std::string object_owner;
	if (gCacheNamep)
	{
		const LLUUID& owner_id = details->owner_id;
		// Note: a valid entry always got either a non-null owner or group Id
		if (owner_id.isNull())
		{
			gCacheNamep->getGroupName(details->group_id, object_owner);
		}
		else
		{
			gCacheNamep->getFullName(owner_id, object_owner);
		}
	}
	if (match_filters && !mSearchedOwner.empty())
	{
		tmp = object_owner;
		LLStringUtil::toLower(tmp);
		if (tmp.find(mSearchedOwner) == std::string::npos)
		{
			return;	// Failed owner name filter match
		}
	}

	std::string object_group;
	if (gCacheNamep && details->group_id.notNull())
	{
		gCacheNamep->getGroupName(details->group_id, object_group);
	}
	if (match_filters && !mSearchedGroup.empty())
	{
		tmp = object_group;
		LLStringUtil::toLower(tmp);
		if (tmp.find(mSearchedGroup) == std::string::npos)
		{
			return;	// Failed group name filter match
		}
	}

	std::string style;
	if (sTracking && object_id == sTrackingObjectID)
	{
		style = "BOLD";
	}
	else
	{
		style = "NORMAL";
	}

	LLSD element;
	element["id"] = object_id;

	LLSD& column_name = element["columns"][LIST_OBJECT_NAME];
	column_name["column"] = "name";
	column_name["type"] = "text";
	column_name["value"] = object_name;
	column_name["font-style"] = style;
	if (object_id == mSearchUUID)
	{
		column_name["color"] = LLColor4::red2.getValue();
	}

	LLSD& column_desc = element["columns"][LIST_OBJECT_DESC];
	column_desc["column"] = "description";
	column_desc["type"] = "text";
	column_desc["value"] = object_desc;
	column_desc["font-style"] = style;

	LLSD& column_owner = element["columns"][LIST_OBJECT_OWNER];
	column_owner["column"] = "owner";
	column_owner["type"] = "text";
	column_owner["value"] = object_owner;
	column_owner["font-style"] = style;

	LLSD& column_group = element["columns"][LIST_OBJECT_GROUP];
	column_group["column"] = "group";
	column_group["type"] = "text";
	column_group["value"] = object_group;
	column_group["font-style"] = style;

	mResultsList->addElement(element, ADD_BOTTOM);
}

void HBFloaterAreaSearch::setButtonsStatus()
{
	LLScrollListItem* item = mResultsList->getFirstSelected();
	if (item)
	{
		object_details_map_t::iterator it =
			sObjectDetails.find(item->getUUID());
		bool is_ours = it != sObjectDetails.end() &&
					   it->second.owner_id == gAgentID;
		mMuteFlyoutBtn->setEnabled(!is_ours);
		mDerenderBtn->setEnabled(true);
		mReportBtn->setEnabled(!is_ours);
		mShowBtn->setEnabled(true);
		mInspectFlyoutBtn->setEnabled(true);
	}
	else
	{
		mMuteFlyoutBtn->setEnabled(false);
		mDerenderBtn->setEnabled(false);
		mReportBtn->setEnabled(false);
		mShowBtn->setEnabled(false);
		mInspectFlyoutBtn->setEnabled(false);
	}
}

//static
void HBFloaterAreaSearch::newRegion()
{
	// We changed region so we can clear the object details cache.
	sObjectDetails.clear();
	sTracking = false;
	sIsDirty = true;

	HBFloaterAreaSearch* self = findInstance();
	if (self)
	{
		self->mResultsList->deleteAllItems();
		self->mCounterText->setText(self->getString("counter_text"));
	}
}

//static
bool HBFloaterAreaSearch::isObjectOfInterest(LLViewerObject* objectp)
{
	if (!objectp) return false;

	LLPCode pcode = objectp->getPCode();
	// Reject all avatars and all viewer-side only objects
	if (pcode != LL_PCODE_VOLUME && pcode != LL_PCODE_LEGACY_GRASS &&
		pcode != LL_PCODE_LEGACY_TREE)
	{
		return false;
	}

	// Reject temporary objects, attachments and child primitives
	return !objectp->flagTemporaryOnRez() && !objectp->isAttachment() &&
		   objectp->isRoot();
}

//static
void HBFloaterAreaSearch::idleUpdate()
{
	if (gFrameTimeSeconds - sLastUpdateTime >= MIN_REFRESH_INTERVAL &&
		findInstance())
	{
		const LLViewerRegion* our_region = gAgent.getRegion();
		for (S32 i = 0, count = gObjectList.getNumObjects(); i < count; ++i)
		{
			LLViewerObject* objectp = gObjectList.getObject(i);
			if (isObjectOfInterest(objectp) &&
				objectp->getRegion() == our_region)
			{
				if (!checkObjectDetails(objectp->getID()))
				{
					sIsDirty = true;
				}
			}
		}

		sLastUpdateTime = gFrameTimeSeconds;
		sUpdateDone = true;
	}
}

//static
bool HBFloaterAreaSearch::checkObjectDetails(const LLUUID& object_id)
{
	HBObjectDetails* details;
	object_details_map_t::iterator it = sObjectDetails.find(object_id);
	if (it == sObjectDetails.end())
	{
		details = &sObjectDetails[object_id];
	}
	else
	{
		details = &it->second;
	}
	if (details->valid())
	{
		return true;
	}

	if (gFrameTimeSeconds - details->time_stamp > AUTO_REFRESH_INTERVAL)
	{
		details->time_stamp = gFrameTimeSeconds;

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_RequestFlags, 0);
		msg->addUUIDFast(_PREHASH_ObjectID, object_id);
		gAgent.sendReliableMessage();

		LL_DEBUGS("AreaSearch") << "Sent data request for object " << object_id
								<< LL_ENDL;
	}

	return false;
}

//static
void HBFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg)
{
	if (!msg || !findInstance()) return;

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	bool exists = sObjectDetails.count(object_id) != 0;
	if (!exists)
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (!isObjectOfInterest(objectp) || objectp->isDead() ||
			objectp->getRegion() != gAgent.getRegion())
		{
			LL_DEBUGS("AreaSearch") << "Rejected info for object "
									<< object_id << LL_ENDL;
			return;	// Not an interesting object for us
		}
	}

	// Update the object's details whether they were requested or not (to avoid
	// having to request them later).
	HBObjectDetails* details = &sObjectDetails[object_id];

	details->time_stamp = gFrameTimeSeconds;

	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, details->owner_id);
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, details->group_id);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, details->name);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description,
					   details->desc);
	if (gCacheNamep)
	{
		if (details->owner_id.notNull())
		{
			gCacheNamep->get(details->owner_id, false,
							 boost::bind(&HBFloaterAreaSearch::setDirty));
		}
		if (details->group_id.notNull())
		{
			gCacheNamep->get(details->group_id, true,
							 boost::bind(&HBFloaterAreaSearch::setDirty));
		}
	}

	LL_DEBUGS("AreaSearch") << "Got info for " << (exists ? "requested" : "new")
							<< " object " << object_id << LL_ENDL;

	sIsDirty = true;
}

//static
void HBFloaterAreaSearch::onSelectResult(LLUICtrl*, void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
 	if (self)
	{
		self->setButtonsStatus();
	}
}

//static
void HBFloaterAreaSearch::onDoubleClickResult(void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
 	if (!self) return;

	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (!item) return;

	LLUUID object_id = item->getUUID();
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (objectp)
	{
		sTrackingObjectID = object_id;
		sTrackingLocation = objectp->getPositionGlobal();
		LLVector3 region_pos = objectp->getPositionRegion();
		sTrackingInfoLine = llformat("Tracking object at position: %d, %d, %d",
									 (S32)region_pos.mV[VX],
									 (S32)region_pos.mV[VY],
									 (S32)region_pos.mV[VZ]);
		self->mCounterText->setText(sTrackingInfoLine);

		gTracker.trackLocation(sTrackingLocation,
							   sObjectDetails[object_id].name, "",
							   LLTracker::LOCATION_ITEM);
		sTracking = sIsDirty = true;
	}
}

//static
void HBFloaterAreaSearch::onClickDerender(void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (!self) return;

	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (!item) return;

	LLUUID object_id = item->getUUID();
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (objectp)
	{
		// Make sure the object is not selected
		gSelectMgr.removeObjectFromSelections(object_id);

		// Remove the object from the list
		sObjectDetails.erase(object_id);

		// Mark the list as dirty
		sIsDirty = true;

		// Derender by killing the object.
		gObjectList.killObject(objectp);
	}
}

//static
void HBFloaterAreaSearch::onClickMute(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (!self || !ctrl) return;

 	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (!item) return;

	LLUUID object_id = item->getUUID();
	object_details_map_t::iterator it = sObjectDetails.find(object_id);
	if (it == sObjectDetails.end() || it->second.owner_id == gAgentID)
	{
		return;	// Abort !
	}

	std::string operation = ctrl->getValue().asString();
	if (operation == "mute_by_name")
	{
		std::string name = it->second.name;
		if (!name.empty())
		{
			LLMute mute(LLUUID::null, name, LLMute::BY_NAME);
			if (LLMuteList::add(mute))
			{
				LLFloaterMute::selectMute(name);
			}
		}
	}
	else if (operation == "mute_particles")
	{
		if (gCacheNamep)
		{
			LLUUID owner_id = it->second.owner_id;
			const LLUUID& group_id = it->second.group_id;
			bool group_owned = owner_id.isNull() && group_id.notNull();
			std::string object_owner;
			if (group_owned)
			{
				gCacheNamep->getGroupName(group_id, object_owner);
				owner_id = group_id;
			}
			else
			{
				gCacheNamep->getFullName(owner_id, object_owner);
			}
			LLMute::EType type = group_owned ? LLMute::GROUP : LLMute::AGENT;
			LLMute mute(owner_id, object_owner, type);
			if (LLMuteList::add(mute, LLMute::flagParticles))
			{
				LLFloaterMute::selectMute(mute.mID);
			}	
		}
	}
	else if (operation == "mute_owner")
	{
		if (gCacheNamep)
		{
			LLUUID owner_id = it->second.owner_id;
			const LLUUID& group_id = it->second.group_id;
			bool group_owned = owner_id.isNull() && group_id.notNull();
			std::string object_owner;
			if (group_owned)
			{
				gCacheNamep->getGroupName(group_id, object_owner);
				owner_id = group_id;
			}
			else
			{
				gCacheNamep->getFullName(owner_id, object_owner);
			}
			LLMute::EType type = group_owned ? LLMute::GROUP : LLMute::AGENT;
			LLMute mute(owner_id, object_owner, type);
			if (LLMuteList::add(mute))
			{
				LLFloaterMute::selectMute(mute.mID);
			}	
		}
	}
	else
	{
		LLMute mute(object_id, it->second.name, LLMute::OBJECT);
		if (LLMuteList::add(mute))
		{
			LLFloaterMute::selectMute(mute.mID);
		}
	}
}

//static
void HBFloaterAreaSearch::onClickReport(void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (item)
	{
		LLFloaterReporter::showFromObject(item->getUUID());
	}
}

//static
void HBFloaterAreaSearch::onClickShow(void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (!self) return;

 	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (item)
	{
		gAgent.lookAtObject(item->getUUID(), CAMERA_POSITION_OBJECT);
	}
}

//static
void HBFloaterAreaSearch::onClickInspect(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (!self || !ctrl || !gWindowp) return;

 	LLScrollListItem* item = self->mResultsList->getFirstSelected();
	if (!item) return;

	LLUUID object_id = item->getUUID();

	std::string operation = ctrl->getValue().asString();
	if (operation == "copy_uuid")
	{
		gWindowp->copyTextToClipboard(utf8str_to_wstring(object_id.asString()));
	}
	else if (operation == "debug")
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp)
		{
			objectp->toggleDebugUpdateMsg();
		}
	}
	else
	{
		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (objectp)
		{
			std::vector<LLViewerObject*> objects;
			objects.push_back(objectp);
			gSelectMgr.selectObjectAndFamily(objects);
			LLFloaterInspect::show();
		}
	}
}

//static
void HBFloaterAreaSearch::onClickRefresh(void* userdata)
{
	sObjectDetails.clear();
	sTracking = false;
	sIsDirty = true;
}

//static
void HBFloaterAreaSearch::onClickClose(void* userdata)
{
	HBFloaterAreaSearch* self = (HBFloaterAreaSearch*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterAreaSearch::onSearchEdit(const std::string& search_string,
									   void* userdata)
{
	HBFloaterAreaSearch* self = findInstance();
	LLSearchEditor* search = (LLSearchEditor*)userdata;
	if (self && search)
	{
		std::string text = search_string;
		LLStringUtil::toLower(text);

		if (search == self->mNameInputLine)
		{
			if (LLUUID::validate(text))
			{
				self->mSearchUUID.set(text);
				self->mSearchedName.clear();
				self->mDescInputLine->clear();
				self->mDescInputLine->setEnabled(false);
				self->mOwnerInputLine->clear();
				self->mOwnerInputLine->setEnabled(false);
				self->mGroupInputLine->clear();
				self->mGroupInputLine->setEnabled(false);
			}
			else
			{
				self->mSearchedName = text;
				self->mSearchUUID.setNull();
				self->mDescInputLine->setText(self->mSearchedDesc);
				self->mDescInputLine->setEnabled(true);
				self->mOwnerInputLine->setEnabled(true);
				self->mOwnerInputLine->setText(self->mSearchedOwner);
				self->mGroupInputLine->setEnabled(true);
				self->mGroupInputLine->setText(self->mSearchedGroup);
			}
		}
		else if (search == self->mDescInputLine)
		{
			self->mSearchedDesc = text;
		}
		else if (search == self->mOwnerInputLine)
		{
			self->mSearchedOwner = text;
		}
		else if (search == self->mGroupInputLine)
		{
			self->mSearchedGroup = text;
		}

		if (text.length() > 2)
		{
			setDirty();
		}
	}
}
