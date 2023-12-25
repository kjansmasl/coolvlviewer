/**
 * @file llfloaterfriends.cpp
 * @author Phoenix
 * @date 2005-01-13
 * @brief Implementation of the friends floater
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


#include "llviewerprecompiledheaders.h"

#include <sstream>

#include "llfloaterfriends.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "lldir.h"
#include "llnamelistctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llfloateravatarinfo.h"
#include "llfloateravatarpicker.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"

// Maximum number of people you can select to do an operation on at once.
#define MAX_FRIEND_SELECT 20
#define DEFAULT_PERIOD 5.0
#define RIGHTS_CHANGE_TIMEOUT 5.0
#define OBSERVER_TIMEOUT 0.5

#define COMMENT_PREFIX "\342\200\243 "

static uuid_list_t sNoBuddy;

// Simple class to observe the calling cards.
class LLLocalFriendsObserver final : public LLFriendObserver,
									 public LLEventTimer
{
protected:
	LOG_CLASS(LLLocalFriendsObserver);

public:
	LLLocalFriendsObserver(LLFloaterFriends* floater)
	:	mFloater(floater),
		LLEventTimer(OBSERVER_TIMEOUT)
	{
		mEventTimer.stop();
	}

	~LLLocalFriendsObserver() override
	{
		mFloater = NULL;
	}

	void changed(U32 mask) override
	{
		LL_DEBUGS("Friends") << "Changed event with mask=" << mask << LL_ENDL;

		// Events can arrive quickly in bulk - we need not process EVERY one of
		// them - so we wait a short while to let others pile-in, and process
		// them in aggregate.
		mEventTimer.start();

		// Save-up all the mask-bits which have come-in
		mMask |= mask;
	}

	void changedBuddies(const uuid_list_t& buddies) override
	{
		for (uuid_list_t::const_iterator it = buddies.begin(),
										 end = buddies.end();
			 it != end; ++it)
		{
			LL_DEBUGS("Friends") << "Changed buddy: " << *it << LL_ENDL;
			mChangedBuddies.emplace(*it);
		}
	}

	bool tick() override
	{
		LL_DEBUGS("Friends") << "Updating friends list. Mask=" << mMask
							 << LL_ENDL;

		mFloater->updateFriends(mMask, mChangedBuddies);
		mMask = 0;
		mChangedBuddies.clear();
		mEventTimer.stop();

		return false;
	}

protected:
	LLFloaterFriends*	mFloater;
	uuid_list_t			mChangedBuddies;
	U32					mMask;
};

LLFloaterFriends::LLFloaterFriends(const LLSD&)
:	LLEventTimer(DEFAULT_PERIOD),
	mListComment(NULL),
	mObserver(NULL),
	mNumRightsChanged(0)
{
	mEventTimer.stop();
	mObserver = new LLLocalFriendsObserver(this);
	gAvatarTracker.addObserver(mObserver);
	gSavedSettings.setBool("ShowFriends", true);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_friends.xml");
	refreshUI();
}

LLFloaterFriends::~LLFloaterFriends()
{
	gAvatarTracker.removeObserver(mObserver);
	delete mObserver;
	gSavedSettings.setBool("ShowFriends", false);
}

//virtual
bool LLFloaterFriends::postBuild()
{
	mFriendsList = getChild<LLScrollListCtrl>("friend_list");
	mFriendsList->setMaxSelectable(MAX_FRIEND_SELECT);
	mFriendsList->setMaximumSelectCallback(onMaximumSelect);
	mFriendsList->setCommitOnSelectionChange(true);
	mFriendsList->setCommitCallback(onSelectName);
	mFriendsList->setCallbackUserData(this);
	mFriendsList->setDoubleClickCallback(onClickIM);

	mIMButton = getChild<LLButton>("im_btn");
	mIMButton->setClickedCallback(onClickIM, this);

	mProfileButton = getChild<LLButton>("profile_btn");
	mProfileButton->setClickedCallback(onClickProfile, this);

	mOfferTPButton = getChild<LLButton>("offer_teleport_btn");
	mOfferTPButton->setClickedCallback(onClickOfferTeleport, this);

	mRequestTPButton = getChild<LLButton>("request_teleport_btn");
	mRequestTPButton->setClickedCallback(onClickRequestTeleport, this);

	mPayButton = getChild<LLButton>("pay_btn");
	mPayButton->setClickedCallback(onClickPay, this);

	mRemoveButton = getChild<LLButton>("remove_btn");
	mRemoveButton->setClickedCallback(onClickRemove, this);

	childSetAction("add_btn", onClickAddFriend, this);
	childSetAction("close_btn", onClickClose, this);

	setDefaultBtn(mIMButton);

	refreshNames();

	updateFriends(LLFriendObserver::ADD, sNoBuddy);
	refreshUI();

	// Primary sort = online status, secondary sort = name
	mFriendsList->sortByColumn("friend_name", true);
	mFriendsList->sortByColumn("icon_online_status", false);

	// Force a refesh to get the latest display names.
	gAvatarTracker.dirtyBuddies();

	return true;
}

//virtual
bool LLFloaterFriends::tick()
{
	mEventTimer.stop();
	mPeriod = DEFAULT_PERIOD;
	updateFriends(LLFriendObserver::ADD, sNoBuddy);
	return false;
}

void LLFloaterFriends::updateFriends(U32 changed_mask,
									 const uuid_list_t& buddies)
{
	LLFloaterFriends* self = findInstance();
	if (!self) return;

	uuid_vec_t selected_friends = self->getSelectedIDs();
	if (changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		// Rebuild the whole list unconditionnally
		refreshNames();
	}
	else if (changed_mask & LLFriendObserver::ONLINE)
	{
		bool success = true;
		// Refresh only the changed items
		for (uuid_list_t::const_iterator it = buddies.begin(),
										 end = buddies.end();
			 success && it != end; ++it)
		{
			success &= updateFriendItem(*it);
		}
		if (!success)
		{
			// Rebuild the whole list unconditionnally
			refreshNames();
		}
	}
	if (changed_mask & LLFriendObserver::POWERS)
	{
		if (--mNumRightsChanged > 0)
		{
			mPeriod = RIGHTS_CHANGE_TIMEOUT;
			mEventTimer.start();
		}
		else
		{
			tick();
		}
	}
	if (selected_friends.size() > 0)
	{
		// Only non-null if friends was already found. This may fail, but we
		// do not really care here, because refreshUI() will clean up the
		// interface.
		for (uuid_vec_t::iterator it = selected_friends.begin();
			 it != selected_friends.end(); ++it)
		{
			self->mFriendsList->setSelectedByValue(*it, true);
		}
	}

	refreshUI();
}

bool LLFloaterFriends::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = gAvatarTracker;
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	if (!relationInfo) return false;

	bool online = relationInfo->isOnline();

	std::string fullname;
	bool has_name = gCacheNamep &&
					gCacheNamep->getFullName(agent_id, fullname);
	if (has_name)
	{
		if (!LLAvatarName::sLegacyNamesForFriends &&
			LLAvatarNameCache::useDisplayNames())
		{
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				if (LLAvatarNameCache::useDisplayNames() == 2)
				{
					fullname = avatar_name.mDisplayName;
				}
				else
				{
					fullname = avatar_name.getNames();
				}
			}
		}
	}

	LLSD element;
	element["id"] = agent_id;
	LLSD& friend_column = element["columns"][LIST_FRIEND_NAME];
	friend_column["column"] = "friend_name";
	friend_column["value"] = fullname;
	friend_column["font"] = "SANSSERIF";
	friend_column["font-style"] = "NORMAL";

	LLSD& online_status_column = element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"] = "icon_online_status";
	online_status_column["type"] = "icon";

	if (online)
	{
		friend_column["font-style"] = "BOLD";
		online_status_column["value"] = "icon_avatar_online.tga";
	}

	LLSD& online_column = element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"] = "icon_visible_online";
	online_column["type"] = "checkbox";
	online_column["value"] =
		relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column = element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"] = "icon_visible_map";
	visible_map_column["type"] = "checkbox";
	visible_map_column["value"] =
		relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column = element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"] = "icon_edit_mine";
	edit_my_object_column["type"] = "checkbox";
	edit_my_object_column["value"] =
		relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& see_them_online_or_on_map = element["columns"][LIST_ONLINE_OR_MAP_THEIRS];
	see_them_online_or_on_map["column"] = "icon_visible_online_or_map_theirs";
	see_them_online_or_on_map["type"] = "icon";
	if (relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION))
	{
		see_them_online_or_on_map["value"] = "ff_visible_map_theirs.tga";
	}
	else if (online ||
			 relationInfo->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS))
	{
		see_them_online_or_on_map["value"] = "ff_visible_online_theirs.tga";
	}

	LLSD& edit_their_object_column = element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"] = "icon_edit_theirs";
	edit_their_object_column["type"] = "icon";
	if (relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS))
	{
		edit_their_object_column["value"] = "ff_edit_theirs.tga";
	}

	LLSD& update_gen_column = element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"] = "friend_last_update_generation";
	update_gen_column["value"] = has_name ? relationInfo->getChangeSerialNum()
										   : -1;

	mFriendsList->addElement(element, ADD_BOTTOM);

	return has_name;
}

// Propagate actual relationship to UI. Does not re-sort the UI list because it
// can be called frequently. JC
bool LLFloaterFriends::updateFriendItem(const LLUUID& agent_id)
{
	const LLRelationship* info = gAvatarTracker.getBuddyInfo(agent_id);
	if (!info) return false;

	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp) return false;

	bool online = info->isOnline();

	std::string fullname;
	bool has_name = gCacheNamep &&
					gCacheNamep->getFullName(agent_id, fullname);
	if (has_name)
	{
		if (!LLAvatarName::sLegacyNamesForFriends &&
			LLAvatarNameCache::useDisplayNames())
		{
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				if (LLAvatarNameCache::useDisplayNames() == 2)
				{
					fullname = avatar_name.mDisplayName;
				}
				else
				{
					fullname = avatar_name.getNames();
				}
			}
		}
	}

	LL_DEBUGS("Friends") << "Updating entry for: " << fullname << " - Online: "
						 << (online ? "yes" : "no") << LL_ENDL;

	// Name of the status icon to use
	std::string status_icon;
	if (online)
	{
		status_icon = "icon_avatar_online.tga";
	}
	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(status_icon);

	itemp->getColumn(LIST_FRIEND_NAME)->setValue(fullname);

	// Render name of online friends in bold text
	LLScrollListText* textp =
		(LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME);
	textp->setFontStyle(online ? LLFontGL::BOLD : LLFontGL::NORMAL);

	LLScrollListCell* cellp = itemp->getColumn(LIST_VISIBLE_ONLINE);
	cellp->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	cellp = itemp->getColumn(LIST_VISIBLE_MAP);
	cellp->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	cellp = itemp->getColumn(LIST_EDIT_MINE);
	cellp->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));

	if (info->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION))
	{
		status_icon = "ff_visible_map_theirs.tga";
	}
	else if (online ||
			 info->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS))
	{
		status_icon = "ff_visible_online_theirs.tga";
	}
	else
	{
		status_icon.clear();
	}
	itemp->getColumn(LIST_ONLINE_OR_MAP_THEIRS)->setValue(status_icon);

	if (info->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS))
	{
		status_icon = "ff_edit_theirs.tga";
	}
	else
	{
		status_icon.clear();
	}
	itemp->getColumn(LIST_EDIT_THEIRS)->setValue(status_icon);

	S32 change_generation = has_name ? info->getChangeSerialNum() : -1;
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// Enable these items, in case they were disabled after user input
	itemp->getColumn(LIST_VISIBLE_ONLINE)->setEnabled(true);
	itemp->getColumn(LIST_VISIBLE_MAP)->setEnabled(true);
	itemp->getColumn(LIST_EDIT_MINE)->setEnabled(true);

	// Do not resort, this function can be called frequently.
	return has_name;
}

void LLFloaterFriends::refreshRightsChangeList()
{
	uuid_vec_t friends = getSelectedIDs();
	S32 num_selected = friends.size();

	bool can_offer_teleport = num_selected >= 1;
	bool selected_friends_online = true;

	const LLRelationship* friend_status = NULL;
	for (uuid_vec_t::iterator it = friends.begin();
		 it != friends.end(); ++it)
	{
		friend_status = gAvatarTracker.getBuddyInfo(*it);
		if (friend_status)
		{
			if (!friend_status->isOnline())
			{
				can_offer_teleport = false;
				selected_friends_online = false;
			}
		}
		else // Missing buddy info, do not allow any operations
		{
			can_offer_teleport = false;
		}
	}

	if (num_selected == 0)  // nothing selected
	{
		mIMButton->setEnabled(false);
		mOfferTPButton->setEnabled(false);
		mRequestTPButton->setEnabled(false);
	}
	else // we have at least one friend selected...
	{
		// Only allow IMs to groups when everyone in the group is online to be
		// consistent with context menus in inventory and because otherwise
		// offline friends would be silently dropped from the session
		mIMButton->setEnabled(selected_friends_online || num_selected == 1);
		mOfferTPButton->setEnabled(can_offer_teleport);
		mRequestTPButton->setEnabled(can_offer_teleport && num_selected == 1);
	}
}

struct SortFriendsByID
{
	bool operator() (const LLScrollListItem* const a,
					 const LLScrollListItem* const b) const
	{
		return a->getValue().asUUID() < b->getValue().asUUID();
	}
};

void LLFloaterFriends::refreshNames()
{
	LL_DEBUGS("Friends") << "Refreshing all names" << LL_ENDL;

	uuid_vec_t selected_ids = getSelectedIDs();
	S32 pos = mFriendsList->getScrollPos();
	mFriendsList->deleteAllItems();

	// Get all buddies we know about
	LLAvatarTracker::buddy_map_t all_buddies;
	gAvatarTracker.copyBuddyList(all_buddies);

	bool has_names = true;
	for (LLAvatarTracker::buddy_map_t::const_iterator it = all_buddies.begin(),
													  end = all_buddies.end();
		 it != end; ++it)
	{
		has_names &= addFriend(it->first);
	}
	if (!has_names)
	{
		mEventTimer.start();
	}

	// Changed item in place, need to request sort and update columns because
	// we might have changed data in a column on which the user has already
	// sorted. JC
	mFriendsList->sortItems();

	// Re-select items
	mFriendsList->selectMultiple(selected_ids);
	mFriendsList->setScrollPos(pos);
}

void LLFloaterFriends::refreshUI()
{
	if (mListComment)
	{
		mFriendsList->deleteItem(mListComment);
		mListComment = NULL;
	}

	S32 num_selected = mFriendsList->getAllSelected().size();
	bool single_selected = num_selected == 1;
	bool some_selected = num_selected > 0;

	// Options that can only be performed with one friend selected
	mProfileButton->setEnabled(single_selected);
	mPayButton->setEnabled(single_selected);

	// Options that can be performed with up to MAX_FRIEND_SELECT friends
	// selected
	mRemoveButton->setEnabled(some_selected);
	mIMButton->setEnabled(some_selected);

	refreshRightsChangeList();

	S32 count = mFriendsList->getItemCount();
	std::string comment = COMMENT_PREFIX;
	if (count > 0)
	{
		comment += getString("total_friends") + llformat(" %d", count);
	}
	else
	{
		comment += getString("no_friend");
	}
	mListComment = mFriendsList->addCommentText(comment);
}

//static
uuid_vec_t LLFloaterFriends::getSelectedIDs()
{
	uuid_vec_t friend_ids;
	LLFloaterFriends* self = findInstance();
	if (self)
	{
		std::vector<LLScrollListItem*> selected =
			self->mFriendsList->getAllSelected();
		for (S32 i = 0, count = selected.size(); i < count; ++i)
		{
			friend_ids.emplace_back(selected[i]->getUUID());
		}
	}
	return friend_ids;
}

//static
void LLFloaterFriends::onSelectName(LLUICtrl* ctrl, void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		self->refreshUI();
		// check to see if rights have changed
		self->applyRightsToFriends();
	}
}

//static
void LLFloaterFriends::onMaximumSelect(void* data)
{
	LLSD args;
	args["MAX_SELECT"] = llformat("%d", MAX_FRIEND_SELECT);
	gNotifications.add("MaxListSelectMessage", args);
}

//static
void LLFloaterFriends::onClickProfile(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		uuid_vec_t ids = self->getSelectedIDs();
		if (ids.size() > 0)
		{
			const LLUUID& agent_id = ids[0];
			bool online = gAvatarTracker.isBuddyOnline(agent_id);
			LLFloaterAvatarInfo::showFromFriend(agent_id, online);
		}
	}
}

//static
void LLFloaterFriends::onClickIM(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		uuid_vec_t ids = self->getSelectedIDs();
		LLAvatarActions::startIM(ids, true);
	}
}

//static
void LLFloaterFriends::onPickAvatar(const std::vector<std::string>& names,
									const std::vector<LLUUID>& ids, void*)
{
	if (!names.empty() && !ids.empty())
	{
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0]);
	}
}

//static
void LLFloaterFriends::onClickAddFriend(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		LLFloaterAvatarPicker* picker;
		picker = LLFloaterAvatarPicker::show(onPickAvatar, data, false, true);
		if (picker)
		{
			self->addDependentFloater(picker);
		}
	}
}

//static
void LLFloaterFriends::onClickRemove(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (!self) return;

	uuid_vec_t ids = self->getSelectedIDs();
	if (!ids.size())
	{
		return;
	}

	LLSD args;
	std::string type;
	if (ids.size() == 1)
	{
		const LLUUID& agent_id = ids[0];
		std::string name;
		if (gCacheNamep && gCacheNamep->getFullName(agent_id, name))
		{
			if (!LLAvatarName::sLegacyNamesForFriends &&
				LLAvatarNameCache::useDisplayNames())
			{
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(agent_id, &avatar_name))
				{
					// Always show "Display Name [Legacy Name]" for
					// security reasons
					name = avatar_name.getNames();
				}
			}
			args["NAME"] = name;
		}
		type = "RemoveFromFriends";
	}
	else
	{
		type = "RemoveMultipleFromFriends";
	}

	LLSD payload;
	for (uuid_vec_t::iterator it = ids.begin(); it != ids.end(); ++it)
	{
		payload["ids"].append(*it);
	}

	gNotifications.add(type, args, payload, handleRemove);
}

//static
void LLFloaterFriends::onClickOfferTeleport(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		uuid_vec_t ids = self->getSelectedIDs();
		LLAvatarActions::offerTeleport(ids);
	}
}

//static
void LLFloaterFriends::onClickRequestTeleport(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		uuid_vec_t ids = self->getSelectedIDs();
		if (ids.size() == 1)
		{
			LLAvatarActions::teleportRequest(ids[0]);
		}
	}
}

//static
void LLFloaterFriends::onClickPay(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		uuid_vec_t ids = self->getSelectedIDs();
		if (ids.size() == 1)
		{
			LLAvatarActions::pay(ids[0]);
		}
	}
}

//static
void LLFloaterFriends::onClickClose(void* data)
{
	LLFloaterFriends* self = (LLFloaterFriends*)data;
	if (self)
	{
		self->close();
	}
}

void LLFloaterFriends::confirmModifyRights(rights_map_t& ids,
										   EGrantRevoke command)
{
	if (ids.empty()) return;

	LLSD args;
	if (ids.size() > 0)
	{
		rights_map_t* rights = new rights_map_t(ids);

		// For a single friend, show their name
		if (ids.size() == 1)
		{
			const LLUUID& agent_id = ids.begin()->first;
			std::string name;
			if (gCacheNamep && gCacheNamep->getFullName(agent_id, name))
			{
				if (!LLAvatarName::sLegacyNamesForFriends &&
					LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(agent_id, &avatar_name))
					{
						// Always show "Display Name [Legacy Name]" for
						// security reasons
						name = avatar_name.getNames();
					}
				}
				args["NAME"] = name;
			}
			if (command == GRANT)
			{
				gNotifications.add("GrantModifyRights", args, LLSD(),
								   boost::bind(&LLFloaterFriends::modifyRightsConfirmation,
							    			  this, _1, _2, rights));
			}
			else
			{
				gNotifications.add("RevokeModifyRights", args, LLSD(),
								   boost::bind(&LLFloaterFriends::modifyRightsConfirmation,
											   this, _1, _2, rights));
			}
		}
		else if (command == GRANT)
		{
			gNotifications.add("GrantModifyRightsMultiple", args, LLSD(),
							   boost::bind(&LLFloaterFriends::modifyRightsConfirmation,
										   this, _1, _2, rights));
		}
		else
		{
			gNotifications.add("RevokeModifyRightsMultiple", args, LLSD(),
							   boost::bind(&LLFloaterFriends::modifyRightsConfirmation,
										   this, _1, _2, rights));
		}
	}
}

bool LLFloaterFriends::modifyRightsConfirmation(const LLSD& notification,
												const LLSD& response,
												rights_map_t* rights)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		sendRightsGrant(*rights);
	}
	else
	{
		// We need to resync view with model, since user cancelled operation
		rights_map_t::iterator rights_it;
		for (rights_it = rights->begin(); rights_it != rights->end();
			 ++rights_it)
		{
			updateFriendItem(rights_it->first);
		}
	}
	delete rights;
	return false;
}

void LLFloaterFriends::applyRightsToFriends()
{
	bool rights_changed = false;

	// store modify rights separately for confirmation
	rights_map_t rights_updates;

	bool need_confirmation = false;
	EGrantRevoke confirmation_type = GRANT;

	// This assumes that changes only happened to selected items
	LLUUID id;
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for (S32 i = 0, count = selected.size(); i < count; ++i)
	{
		LLScrollListItem* itemp = selected[i];
		id = itemp->getUUID();

		const LLRelationship* rel = gAvatarTracker.getBuddyInfo(id);
		if (!rel) continue;

		bool show_online_status =
			itemp->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location =
			itemp->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects =
			itemp->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();

		S32 rights = rel->getRightsGrantedTo();
		if (rel->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS) !=
				show_online_status)
		{
			rights_changed = true;
			if (show_online_status)
			{
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
			}
			else
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights &= ~LLRelationship::GRANT_ONLINE_STATUS;
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
				// propagate rights constraint to UI
				itemp->getColumn(LIST_VISIBLE_MAP)->setValue(false);
			}
		}
		if (rel->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION) !=
				show_map_location)
		{
			rights_changed = true;
			if (show_map_location)
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights |= LLRelationship::GRANT_MAP_LOCATION;
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
				itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(true);
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
			}
		}

		// Now check for change in modify object rights, which requires
		// confirmation
		if (rel->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS) !=
				allow_modify_objects)
		{
			rights_changed = need_confirmation = true;

			if (allow_modify_objects)
			{
				rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = GRANT;
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = REVOKE;
			}
		}

		if (rights_changed)
		{
			rights_updates.emplace(id, rights);
			// Disable these UI elements until response from server to avoid
			// race conditions
			itemp->getColumn(LIST_VISIBLE_ONLINE)->setEnabled(false);
			itemp->getColumn(LIST_VISIBLE_MAP)->setEnabled(false);
			itemp->getColumn(LIST_EDIT_MINE)->setEnabled(false);
		}
	}

	// Separately confirm grant and revoke of modify rights
	if (need_confirmation)
	{
		confirmModifyRights(rights_updates, confirmation_type);
	}
	else
	{
		sendRightsGrant(rights_updates);
	}
}

void LLFloaterFriends::sendRightsGrant(rights_map_t& ids)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || ids.empty()) return;

	// Setup message header
	msg->newMessageFast(_PREHASH_GrantUserRights);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);

	for (rights_map_t::iterator it = ids.begin(), end = ids.end();
		 it != end; ++it)
	{
		msg->nextBlockFast(_PREHASH_Rights);
		msg->addUUID(_PREHASH_AgentRelated, it->first);
		msg->addS32(_PREHASH_RelatedRights, it->second);
	}

	mNumRightsChanged = ids.size();
	gAgent.sendReliableMessage();
}

//static
bool LLFloaterFriends::handleRemove(const LLSD& notification,
									const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) != 0) // NO
	{
		llinfos << "No removal performed." << llendl;
		return false;
	}

	const LLSD& ids = notification["payload"]["ids"];
	for (LLSD::array_const_iterator it = ids.beginArray(),
									end = ids.endArray();
		 it != end; ++it)
	{
		LLUUID id = it->asUUID();
		const LLRelationship* ip = gAvatarTracker.getBuddyInfo(id);
		if (ip)
		{
			if (ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
			{
#if TRACK_POWER
				gAvatarTracker.empower(id, false);
#endif
				gAvatarTracker.notifyObservers();
			}
			gAvatarTracker.terminateBuddy(id);
			gAvatarTracker.notifyObservers();
			gInventory.addChangedMask(LLInventoryObserver::LABEL |
									  LLInventoryObserver::CALLING_CARD,
									  LLUUID::null);
			gInventory.notifyObservers();
		}
	}

	return false;
}
