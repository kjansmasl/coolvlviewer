/**
 * @file llfloateravatarpicker.cpp
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llfloateravatarpicker.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcorehttputil.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llavatartracker.h"
#include "llfolderview.h"
#include "llinventorymodel.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llworld.h"

// static
LLFloaterAvatarPicker::instances_list_t LLFloaterAvatarPicker::sInstances;

//static
LLFloaterAvatarPicker* LLFloaterAvatarPicker::show(callback_t callback,
												   void* userdata,
												   bool allow_multiple,
												   bool close_on_select,
												   const std::string& name)
{
	LLFloaterAvatarPicker* self = NULL;
	for (instances_list_t::iterator it = sInstances.begin(),
									end = sInstances.end();
		 it != end; ++it)
	{
		LLFloaterAvatarPicker* instance = *it;
		if (instance && instance->mCallback == callback &&
			instance->mCallbackUserdata == userdata)
		{
			self = instance;
			break;
		}
	}

	if (!self)
	{
		self = new LLFloaterAvatarPicker(callback, userdata);
	}

	self->open();
	self->setAllowMultiple(allow_multiple);
	self->mNearMeListComplete = false;
	self->mCloseOnSelect = close_on_select;

	// Extension to LL's avatar picker: search for an avatar name on opening,
	// when requested/needed. Used by the Lua PickAvatar() function. HB
	if (!name.empty())
	{
		self->mEdit->setValue(name);
		self->find();
	}

	return self;
}

LLFloaterAvatarPicker* LLFloaterAvatarPicker::findInstance(const LLUUID& query_id)
{
	for (instances_list_t::iterator it = sInstances.begin(),
									end = sInstances.end();
		 it != end; ++it)
	{
		LLFloaterAvatarPicker* floater = *it;
		if (floater && floater->mQueryID == query_id)
		{
			return floater;
		}
	}
	return NULL;
}

LLFloaterAvatarPicker::LLFloaterAvatarPicker(LLFloaterAvatarPicker::callback_t callback,
											 void* userdata)
:	mCallback(callback),
	mCallbackUserdata(userdata)
{
	sInstances.insert(this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_avatar_picker.xml");
}

LLFloaterAvatarPicker::~LLFloaterAvatarPicker()
{
	gFocusMgr.releaseFocusIfNeeded(this);
	sInstances.erase(this);
}

bool LLFloaterAvatarPicker::postBuild()
{
	mEdit = getChild<LLLineEditor>("Edit");
	mEdit->setKeystrokeCallback(editKeystroke);
	mEdit->setCallbackUserData(this);
	mEdit->setFocus(true);

	mFind = getChild<LLButton>("Find");
	mFind->setClickedCallback(onBtnFind, this);
	mFind->setEnabled(false);

	childSetAction("Refresh", onBtnRefresh, this);

	childSetCommitCallback("near_me_range", onRangeAdjust, this);

	mSelect = getChild<LLButton>("Select");
	mSelect->setClickedCallback(onBtnSelect, this);
	mSelect->setEnabled(false);

	childSetAction("Close", onBtnClose, this);

	mSearchResults = getChild<LLScrollListCtrl>("SearchResults");
	mSearchResults->setDoubleClickCallback(onBtnSelect);
	mSearchResults->setCommitCallback(onList);
	mSearchResults->setCallbackUserData(this);
	mSearchResults->setEnabled(false);
	mSearchResults->addCommentText(getString("no_result"));

	mFriends = getChild<LLScrollListCtrl>("Friends");
	mFriends->setDoubleClickCallback(onBtnSelect);
	mFriends->setCommitCallback(onList);
	mFriends->setCallbackUserData(this);

	mNearMe = getChild<LLScrollListCtrl>("NearMe");
	mNearMe->setDoubleClickCallback(onBtnSelect);
	mNearMe->setCommitCallback(onList);
	mNearMe->setCallbackUserData(this);

	mInventoryPanel = getChild<LLInventoryPanel>("InventoryPanel");
	mInventoryPanel->setFilterTypes(0x1 << LLInventoryType::IT_CALLINGCARD);
	mInventoryPanel->setFollowsAll();
	mInventoryPanel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mInventoryPanel->openDefaultFolderForType(LLAssetType::AT_CALLINGCARD);
	mInventoryPanel->setSelectCallback(onCallingCardSelectionChange, this);

	mSearchPanel = getChild<LLPanel>("SearchPanel");
	mSearchPanel->setDefaultBtn(mFind);
	mFriendsPanel = getChild<LLPanel>("FriendsPanel");
	mCallingCardsPanel = getChild<LLPanel>("CallingCardsPanel");
	mNearMePanel = getChild<LLPanel>("NearMePanel");

	mResidentChooserTabs = getChild<LLTabContainer>("ResidentChooserTabs");
	mResidentChooserTabs->setTabChangeCallback(mSearchPanel, onTabChanged);
	mResidentChooserTabs->setTabUserData(mSearchPanel, this);
	mResidentChooserTabs->setTabChangeCallback(mFriendsPanel, onTabChanged);
	mResidentChooserTabs->setTabUserData(mFriendsPanel, this);
	mResidentChooserTabs->setTabChangeCallback(mCallingCardsPanel, onTabChanged);
	mResidentChooserTabs->setTabUserData(mCallingCardsPanel, this);
	mResidentChooserTabs->setTabChangeCallback(mNearMePanel, onTabChanged);
	mResidentChooserTabs->setTabUserData(mNearMePanel, this);

	setAllowMultiple(false);

	center();

	populateFriends();

	return true;
}

//virtual
void LLFloaterAvatarPicker::draw()
{
	if (!mNearMeListComplete &&
		mResidentChooserTabs->getCurrentPanel() == mNearMePanel)
	{
		populateNearMe();
	}
	LLFloater::draw();
}

//virtual
bool LLFloaterAvatarPicker::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		if (mEdit->hasFocus())
		{
			onBtnFind(this);
		}
		else
		{
			onBtnSelect(this);
		}
		return true;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		close();
		return true;
	}

	return LLFloater::handleKeyHere(key, mask);
}

// Callback for inventory picker (select from calling cards)
void LLFloaterAvatarPicker::doCallingCardSelectionChange(LLFolderView* folderp)
{
	bool panel_active =
		mResidentChooserTabs->getCurrentPanel() == mCallingCardsPanel;

	mSelectedInventoryAvatarIDs.clear();
	mSelectedInventoryAvatarNames.clear();

	if (panel_active)
	{
		mSelect->setEnabled(false);
	}

	const LLFolderView::selected_items_t& items = folderp->getSelectedItems();
	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(),
													   end = items.end();
		 it != end; ++it)
	{
		LLFolderViewEventListener* listenerp = (*it)->getListener();
		if (listenerp &&
			listenerp->getInventoryType() == LLInventoryType::IT_CALLINGCARD)
		{
			LLInventoryItem* item = gInventory.getItem(listenerp->getUUID());
			if (item)
			{
				mSelectedInventoryAvatarIDs.emplace_back(item->getCreatorUUID());
				mSelectedInventoryAvatarNames.emplace_back(listenerp->getName());
			}
		}
	}

	if (panel_active)
	{
		mSelect->setEnabled(visibleItemsSelected());
	}
}

void LLFloaterAvatarPicker::populateNearMe()
{
	bool all_loaded = true;
	bool empty = true;

	mNearMe->deleteAllItems();

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		return;
	}
//mk

	uuid_vec_t avatar_ids;
	gWorld.getAvatars(avatar_ids, NULL, NULL, gAgent.getPositionGlobal(),
					  gSavedSettings.getF32("NearMeRange"));
	for (U32 i = 0, count = avatar_ids.size(); i < count; ++i)
	{
		LLUUID& av = avatar_ids[i];
		if (av == gAgentID) continue;
		LLSD element;
		element["id"] = av; // value
		std::string fullname;
		if (!gCacheNamep || !gCacheNamep->getFullName(av, fullname))
		{
			element["columns"][0]["value"] = LLCacheName::getDefaultName();
			all_loaded = false;
		}
		else
		{
			element["columns"][0]["value"] = fullname;
		}
		mNearMe->addElement(element);
		empty = false;
	}

	if (empty)
	{
		mNearMe->setEnabled(false);
		mSelect->setEnabled(false);
		mNearMe->addCommentText(getString("no_one_near"));
	}
	else
	{
		mNearMe->setEnabled(true);
		mSelect->setEnabled(true);
		mNearMe->selectFirstItem();
		onList(mNearMe, this);
		mNearMe->setFocus(true);
	}

	if (all_loaded)
	{
		mNearMeListComplete = true;
	}
}

void LLFloaterAvatarPicker::populateFriends()
{
	mFriends->deleteAllItems();
	LLCollectAllBuddies collector;
	gAvatarTracker.applyFunctor(collector);
	LLCollectAllBuddies::buddy_map_t::iterator it;

	for (it = collector.mOnline.begin(); it != collector.mOnline.end(); ++it)
	{
		mFriends->addStringUUIDItem(it->first, it->second);
	}
	for (it = collector.mOffline.begin(); it != collector.mOffline.end(); ++it)
	{
		mFriends->addStringUUIDItem(it->first, it->second);
	}
	mFriends->sortByColumnIndex(0, true);
}

bool LLFloaterAvatarPicker::visibleItemsSelected() const
{
	LLPanel* active_panel = mResidentChooserTabs->getCurrentPanel();
	if (active_panel == mSearchPanel)
	{
		return mSearchResults->getFirstSelectedIndex() >= 0;
	}
	if (active_panel == mFriendsPanel)
	{
		return mFriends->getFirstSelectedIndex() >= 0;
	}
	if (active_panel == mCallingCardsPanel)
	{
		return mSelectedInventoryAvatarIDs.size() > 0;
	}
	if (active_panel == mNearMePanel)
	{
		return mNearMe->getFirstSelectedIndex() >= 0;
	}
	return false;
}

void LLFloaterAvatarPicker::setAllowMultiple(bool allow_multiple)
{
	mSearchResults->setAllowMultipleSelection(allow_multiple);
	mFriends->setAllowMultipleSelection(allow_multiple);
	mInventoryPanel->setAllowMultiSelect(allow_multiple);
	mNearMe->setAllowMultipleSelection(allow_multiple);
}

void LLFloaterAvatarPicker::find()
{
	const std::string& text = mEdit->getValue().asString();

	mQueryID.generate();

	std::string url = gAgent.getRegionCapability("AvatarPickerSearch");
	if (!url.empty() && LLAvatarNameCache::useDisplayNames())
	{
		// Capability URLs do not always end in '/', but we need one to parse
		// query parameters correctly
		if (url.back() != '/')
		{
			url += "/";
		}
		url += "?page_size=100&names=";
		url += LLURI::escape(text);
		llinfos << "Avatar picker request: " << url << llendl;
		gCoros.launch("LLFloaterAvatarPicker::findCoro",
					  boost::bind(&LLFloaterAvatarPicker::findCoro, url,
								  mQueryID));
	}
	else
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_AvatarPickerRequest);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUID(_PREHASH_QueryID, mQueryID);
		msg->nextBlock(_PREHASH_Data);
		msg->addString(_PREHASH_Name, text);

		gAgent.sendReliableMessage();
	}

	mSearchResults->deleteAllItems();
	mSearchResults->addCommentText(getString("searching"));

	mSelect->setEnabled(false);
}

//static
void LLFloaterAvatarPicker::findCoro(const std::string& url, LLUUID query_id)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("AvatarPickerSearch");
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setTimeout(180);
	LLSD result = adapter.getAndSuspend(url, options);

	LLFloaterAvatarPicker* self =
		LLFloaterAvatarPicker::findInstance(query_id);
	if (!self)
	{
		return;	// Floater closed...
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	// In case of invalid characters, the avatar picker returns a 400; just set
	// it to process so it displays 'not found'
	if (status || status == gStatusBadRequest)
	{
		self->processResponse(query_id, result);
	}
	else
	{
		llwarns << "Avatar picker request failed: " << status.toString()
				<< llendl;
		self->mSearchResults->deleteAllItems();
		self->mSearchResults->addCommentText(status.toString());
	}
}

void LLFloaterAvatarPicker::processResponse(const LLUUID& query_id,
											const LLSD& content)
{
	// Check for out-of-date query
	if (query_id != mQueryID) return;

	const std::string legacy_name = getString("legacy_name");
	const std::string display_name = getString("display_name");

	std::string search_text_raw = mEdit->getValue().asString();

	LLSD agents = content["agents"];
	if (agents.size() == 0)
	{
		LLStringUtil::format_map_t map;
		map["[TEXT]"] = search_text_raw;
		LLSD element;
		element["id"] = LLUUID::null;
		element["columns"][0]["column"] = legacy_name;
		element["columns"][0]["value"] = getString("not_found", map);
		mSearchResults->addElement(element);
		mSearchResults->setEnabled(false);
		mSearchResults->setDisplayHeading(false);
		mSelect->setEnabled(false);
		return;
	}

	std::string search_text = search_text_raw;
	if (search_text.find(' ') == std::string::npos)
	{
		search_text += " Resident";
	}
	LLStringUtil::toLower(search_text);
	LLStringUtil::toLower(search_text_raw);

	// Clear "Searching" label on first results
	mSearchResults->deleteAllItems();
	mSearchResults->setDisplayHeading(true);

	std::string name;
	LLUUID matching_id;
	LLSD element;
	for (LLSD::array_const_iterator it = agents.beginArray(),
									end = agents.endArray();
		 it != end; ++it)
	{
		const LLSD& row = *it;
		LLSD& columns = element["columns"];
		element["id"] = row["id"];

		name = row["legacy_first_name"].asString();
		name += " ";
		name += row["legacy_last_name"].asString();
		columns[0]["column"] = legacy_name;
		columns[0]["value"] = name;
		LLStringUtil::toLower(name);
		if (name == search_text)
		{
			columns[0]["font-style"] = "BOLD";
			matching_id = row["id"].asUUID();
		}
		else
		{
			columns[0]["font-style"] = "NORMAL";
		}

		columns[1]["column"] = display_name;
		name = row["display_name"].asString();
		columns[1]["value"] = name;
		LLStringUtil::toLower(name);
		if (name == search_text_raw)
		{
			columns[1]["font-style"] = "BOLD";
		}
		else
		{
			columns[1]["font-style"] = "NORMAL";
		}

		mSearchResults->addElement(element);
	}

	mSelect->setEnabled(true);
	mSearchResults->selectFirstItem();
	mSearchResults->setEnabled(true);
	onList(mSearchResults, this);
	mSearchResults->setFocus(true);
	if (matching_id.notNull())
	{
		mSearchResults->selectByID(matching_id);
		mSearchResults->scrollToShowSelected();
	}
}

//static
void LLFloaterAvatarPicker::processAvatarPickerReply(LLMessageSystem* msg,
													 void**)
{
	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		// Not for us
		return;
	}

	LLUUID query_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_QueryID, query_id);

	LLFloaterAvatarPicker* self =
		LLFloaterAvatarPicker::findInstance(query_id);
	if (!self)
	{
		// These are not results from our last requests
		return;
	}

	std::string search_text = self->mEdit->getValue().asString();
	if (search_text.find(' ') == std::string::npos)
	{
		search_text += " Resident";
	}
	LLStringUtil::toLower(search_text);

	// clear "Searching" label on first results
	self->mSearchResults->deleteAllItems();
	self->mSearchResults->setDisplayHeading(false);

	const std::string legacy_name = self->getString("legacy_name");
	bool found_one = false;
	LLUUID avatar_id, matching_id;
	std::string first_name, last_name;
	S32 num_new_rows = msg->getNumberOfBlocks("Data");
	for (S32 i = 0; i < num_new_rows; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_AvatarID, avatar_id, i);
		msg->getStringFast(_PREHASH_Data, _PREHASH_FirstName, first_name, i);
		msg->getStringFast(_PREHASH_Data, _PREHASH_LastName, last_name, i);

		std::string avatar_name;
		if (avatar_id.isNull())
		{
			LLStringUtil::format_map_t map;
			map["[TEXT]"] = self->mEdit->getValue().asString();
			avatar_name = self->getString("not_found", map);
			self->mSearchResults->setEnabled(false);
			self->mSelect->setEnabled(false);
		}
		else
		{
			avatar_name = first_name + " " + last_name;
			self->mSearchResults->setEnabled(true);
			found_one = true;
		}
		LLSD element;
		element["id"] = avatar_id; // value
		element["columns"][0]["column"] = legacy_name;
		element["columns"][0]["value"] = avatar_name;
		LLStringUtil::toLower(avatar_name);
		if (avatar_name == search_text)
		{
			element["columns"][0]["font-style"] = "BOLD";
			matching_id = avatar_id;
		}
		else
		{
			element["columns"][0]["font-style"] = "NORMAL";
		}
		self->mSearchResults->addElement(element);
	}

	if (found_one)
	{
		self->mSelect->setEnabled(true);
		self->mSearchResults->selectFirstItem();
		self->onList(self->mSearchResults, self);
		self->mSearchResults->setFocus(true);
		if (matching_id.notNull())
		{
			self->mSearchResults->selectByID(matching_id);
			self->mSearchResults->scrollToShowSelected();
		}
	}
}

//static
void LLFloaterAvatarPicker::onTabChanged(void* userdata, bool from_click)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (self)
	{
		self->mSelect->setEnabled(self->visibleItemsSelected());
	}
}

//static
void LLFloaterAvatarPicker::onBtnFind(void* userdata)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (self)
	{
		self->find();
	}
}

static void getSelectedAvatarData(const LLScrollListCtrl* from,
								  std::vector<std::string>& avatar_names,
								  uuid_vec_t& avatar_ids)
{
	std::vector<LLScrollListItem*> items = from->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator iter = items.begin(),
												  end = items.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item->getUUID().notNull())
		{
			avatar_names.emplace_back(item->getColumn(0)->getValue().asString());
			avatar_ids.emplace_back(item->getUUID());
		}
	}
}

//static
void LLFloaterAvatarPicker::onBtnSelect(void* userdata)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (!self) return;

	if (self->mCallback)
	{
		LLPanel* active_panel = self->mResidentChooserTabs->getCurrentPanel();

		if (active_panel == self->mSearchPanel)
		{
			std::vector<std::string> avatar_names;
			uuid_vec_t avatar_ids;
			getSelectedAvatarData(self->mSearchResults, avatar_names,
								  avatar_ids);
			self->mCallback(avatar_names, avatar_ids, self->mCallbackUserdata);
		}
		else if (active_panel == self->mFriendsPanel)
		{
			std::vector<std::string> avatar_names;
			uuid_vec_t avatar_ids;
			getSelectedAvatarData(self->mFriends, avatar_names, avatar_ids);
			self->mCallback(avatar_names, avatar_ids, self->mCallbackUserdata);
		}
		else if (active_panel == self->mCallingCardsPanel)
		{
			self->mCallback(self->mSelectedInventoryAvatarNames,
							self->mSelectedInventoryAvatarIDs,
							self->mCallbackUserdata);
		}
		else if (active_panel == self->mNearMePanel)
		{
			std::vector<std::string> avatar_names;
			uuid_vec_t avatar_ids;
			getSelectedAvatarData(self->mNearMe, avatar_names, avatar_ids);
			self->mCallback(avatar_names, avatar_ids, self->mCallbackUserdata);
		}
		else
		{
			llwarns << "Unknown active panel !" << llendl;
		}
	}
	self->mSearchResults->deselectAllItems(true);
	self->mFriends->deselectAllItems(true);
	self->mInventoryPanel->setSelection(LLUUID::null, false);
	self->mNearMe->deselectAllItems(true);
	if (self->mCloseOnSelect)
	{
		self->mCloseOnSelect = false;
		self->close();
	}
}

//static
void LLFloaterAvatarPicker::onBtnRefresh(void* userdata)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (self)
	{
		self->mNearMe->deleteAllItems();
		self->mNearMe->addCommentText(self->getString("searching"));
		self->mNearMeListComplete = false;
	}
}

//static
void LLFloaterAvatarPicker::onBtnClose(void* userdata)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (self)
	{
		self->close();
	}
}

void LLFloaterAvatarPicker::onRangeAdjust(LLUICtrl* source, void* data)
{
	LLFloaterAvatarPicker::onBtnRefresh(data);
}

//static
void LLFloaterAvatarPicker::onList(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)userdata;
	if (self)
	{
		self->mSelect->setEnabled(self->visibleItemsSelected());
	}
}

//static
void LLFloaterAvatarPicker::editKeystroke(LLLineEditor* caller,
										  void* user_data)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)user_data;
	if (self)
	{
		self->mFind->setEnabled(caller->getText().size() >= 3);
	}
}

// Callback for inventory picker (select from calling cards)
//static
void LLFloaterAvatarPicker::onCallingCardSelectionChange(LLFolderView* folderp,
														 bool user_action,
														 void* user_data)
{
	LLFloaterAvatarPicker* self = (LLFloaterAvatarPicker*)user_data;
	if (self && folderp)
	{
		self->doCallingCardSelectionChange(folderp);
	}
}
