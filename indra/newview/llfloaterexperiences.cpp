/**
 * @file llfloaterexperiences.cpp
 * @brief LLFloaterExperiences class implementation
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "boost/signals2.hpp"

#include "llfloaterexperiences.h"

#include "llbutton.h"
#include "llevents.h"
#include "llexperiencecache.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llexperiencelog.h"			// For PUMP_EXPERIENCE
#include "llfloaterexperiencepicker.h"	// For LLPanelExperiencePicker class
#include "llfloaterexperienceprofile.h"
#include "llfloaterregioninfo.h"
#include "llpanelexperiencelog.h"
#include "llviewerregion.h"

//static
S32 LLFloaterExperiences::sLastTab = 0;

// LLFloaterExperiences class proper

LLFloaterExperiences::LLFloaterExperiences(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_experiences.xml");
}

LLFloaterExperiences::~LLFloaterExperiences()
{
	sLastTab = mTabContainer->getCurrentPanelIndex();
}

LLPanelExperiences* LLFloaterExperiences::addTab(const std::string& name,
												 bool select)
{
	LLPanelExperiences* panel = LLPanelExperiences::create(name);
	mTabContainer->addTabPanel(panel, LLTrans::getString(name), select);
	return panel;
}

bool LLFloaterExperiences::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("xp_tabs");

	// Add the experiences picker panel and set it in non-picker (list) mode
	LLPanelExperiencePicker* picker = new LLPanelExperiencePicker();
	mTabContainer->addTabPanel(picker, picker->getLabel());
	picker->hideOkCancel();

	// Add the filtered experiences panels
	addTab("Allowed_Experiences_Tab", true);
	addTab("Blocked_Experiences_Tab", false);
	addTab("Admin_Experiences_Tab", false);
	addTab("Contrib_Experiences_Tab", false);

	LLPanelExperiences* owned = addTab("Owned_Experiences_Tab", false);
	owned->setButtonAction("acquire", LLFloaterExperiences::sendPurchaseRequest,
						 this);
	owned->enableButton(false);

	// Add the events log panel
	LLPanelExperienceLog* logs = new LLPanelExperienceLog();
	mTabContainer->addTabPanel(logs, logs->getLabel());

	childSetAction("close_btn", onCloseBtn, this);

	LLEventPump& pump = gEventPumps.obtain(PUMP_EXPERIENCE);
	pump.listen("LLFloaterExperiences",
				boost::bind(&LLFloaterExperiences::updatePermissions,
							this, _1));

	if (sLastTab < mTabContainer->getTabCount())
	{
		mTabContainer->selectTab(sLastTab);
	}
	else
	{
		sLastTab = 0;
	}

   	return true;
}

void LLFloaterExperiences::refreshContents()
{
	LLHandle<LLFloaterExperiences> handle =
		getDerivedHandle<LLFloaterExperiences>();
	name_map_t name_map;
	const std::string& url = gAgent.getRegionCapability("GetExperiences");
	if (!url.empty())
	{
		name_map["experiences"] = "Allowed_Experiences_Tab";
		name_map["blocked"] = "Blocked_Experiences_Tab";
		retrieveExperienceList(url, handle, name_map);
	}

	updateInfo("GetAdminExperiences", "Admin_Experiences_Tab");
	updateInfo("GetCreatorExperiences", "Contrib_Experiences_Tab");

	const std::string& url2 = gAgent.getRegionCapability("AgentExperiences");
	if (!url2.empty())
	{
		name_map["experience_ids"] = "Owned_Experiences_Tab";
		retrieveExperienceList(url2, handle, name_map,
							   "ExperienceAcquireFailed",
							   boost::bind(&LLFloaterExperiences::checkPurchaseInfo,
										  this, _1, _2));
	}
}

void LLFloaterExperiences::onOpen()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		return;
	}
	if (regionp->capabilitiesReceived())
	{
		refreshContents();
		return;
	}
	// Register our callback for when capabilities will have been received.
	regionp->setCapsReceivedCB(boost::bind(&LLFloaterExperiences::refreshContents,
										   this));
}

bool LLFloaterExperiences::updatePermissions(const LLSD& permission)
{
	LLUUID experience;
	std::string permission_string;
	if (permission.has("experience"))
	{
		experience = permission["experience"].asUUID();
		permission_string = permission[experience.asString()]["permission"].asString();

	}

	LLPanelExperiences* tab;
	tab = (LLPanelExperiences*)mTabContainer->getPanelByName("Allowed_Experiences_Tab");
	if (tab)
	{
		if (permission.has("experiences"))
		{
			tab->setExperienceList(permission["experiences"]);
		}
		else if (experience.notNull())
		{
			if (permission_string != "Allow")
			{
				tab->removeExperience(experience);
			}
			else
			{
				tab->addExperience(experience);
			}
		}
	}

	tab = (LLPanelExperiences*)mTabContainer->getPanelByName("Blocked_Experiences_Tab");
	if (tab)
	{
		if (permission.has("blocked"))
		{
			tab->setExperienceList(permission["blocked"]);
		}
		else if (experience.notNull())
		{
			if (permission_string != "Block")
			{
				tab->removeExperience(experience);
			}
			else
			{
				tab->addExperience(experience);
			}
		}
	}
	return false;
}

void LLFloaterExperiences::onClose(bool app_quitting)
{
	LLEventPump& pump = gEventPumps.obtain(PUMP_EXPERIENCE);
	pump.stopListening("LLFloaterExperiences");
	LLFloater::onClose(app_quitting);
}

void LLFloaterExperiences::checkPurchaseInfo(LLPanelExperiences* panel,
											 const LLSD& content)
{
	if (panel)
	{
		panel->enableButton(content.has("purchase"));
		updateInfo("GetAdminExperiences", "Admin_Experiences_Tab");
		updateInfo("GetCreatorExperiences", "Contrib_Experiences_Tab");
	}
}

void LLFloaterExperiences::updateInfo(const char* exp_cap, const char* tab)
{
	const std::string& url = gAgent.getRegionCapability(exp_cap);
	if (!url.empty())
	{
		name_map_t name_map;
		name_map["experience_ids"] = tab;
		retrieveExperienceList(url, getDerivedHandle<LLFloaterExperiences>(),
							   name_map);
	}
}

void LLFloaterExperiences::doSendPurchaseRequest()
{
	const std::string& url = gAgent.getRegionCapability("AgentExperiences");
	if (!url.empty())
	{
		name_map_t name_map;
		name_map["experience_ids"] = "Owned_Experiences_Tab";
		requestNewExperience(url, getDerivedHandle<LLFloaterExperiences>(),
							 name_map, "ExperienceAcquireFailed",
							 boost::bind(&LLFloaterExperiences::checkPurchaseInfo,
										 this, _1, _2));
	}
}

//static
void LLFloaterExperiences::sendPurchaseRequest(void* data)
{
	LLFloaterExperiences* self = findInstance();
	if (self && self == (LLFloaterExperiences*)data)
	{
		self->doSendPurchaseRequest();
	}
}

//static
void LLFloaterExperiences::onCloseBtn(void* data)
{
	LLFloaterExperiences* self = (LLFloaterExperiences*)data;
	if (self)
	{
		self->close();
	}
}

// LLPanelExperiences class

//static
LLPanelExperiences* LLPanelExperiences::create(const std::string& name)
{
	LLPanelExperiences* panel= new LLPanelExperiences();
	panel->setName(name);
	return panel;
}

LLPanelExperiences::LLPanelExperiences()
:	mListEmpty(true)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_experiences.xml");
}

bool LLPanelExperiences::postBuild()
{
	mExperiencesList = getChild<LLScrollListCtrl>("experiences_list");
	mExperiencesList->addCommentText(getString("no_experiences_text"));
	mExperiencesList->setDoubleClickCallback(onDoubleClickProfile);
	mExperiencesList->setCallbackUserData(this);

	mActionBtn = getChild<LLButton>("btn_action");
	mActionBtn->setVisible(false);

	return true;
}

//static
void LLPanelExperiences::cacheCallback(LLHandle<LLPanelExperiences> handle,
									   const LLSD& experience)
{
	LLPanelExperiences* self = handle.get();
	if (self)
	{
		if (self->mListEmpty)
		{
			// Remove the entry containing the "no experiences" comment
			self->mExperiencesList->deleteAllItems();
			self->mListEmpty = false;
		}

		const LLUUID& id = experience[LLExperienceCache::EXPERIENCE_ID];
		const LLSD& name = experience[LLExperienceCache::NAME];
		LLScrollListItem* item = self->mExperiencesList->getItem(id);
		if (item)
		{
			// Update the existing entry
			item->getColumn(0)->setValue(name);
		}
		else
		{
			// Create a new entry
			LLSD entry;
			entry["id"] = id;
			LLSD& columns = entry["columns"];
			columns[0]["column"] = "experience_name";
			columns[0]["value"] = name.asString();
			self->mExperiencesList->addElement(entry);
		}
	}
}

void LLPanelExperiences::addExperience(const LLUUID& id)
{
	if (!mExperiencesList->getItem(id))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->get(id,
				 boost::bind(&LLPanelExperiences::cacheCallback,
							 getDerivedHandle<LLPanelExperiences>(), _1));
		if (mListEmpty)
		{
			mExperiencesList->deleteAllItems();
			mExperiencesList->addCommentText(getString("loading_experiences"));
		}
	}
}

void LLPanelExperiences::setExperienceList(const LLSD& experiences)
{
	mExperiencesList->deleteAllItems();
	mListEmpty = true;
	mExperiencesList->addCommentText(getString("no_experiences_text"));

	for (LLSD::array_const_iterator it = experiences.beginArray(),
									end = experiences.endArray();
		 it != end; ++it)
	{
		addExperience(it->asUUID());
	}
}

void LLPanelExperiences::removeExperience(const LLUUID& id)
{
	LLScrollListItem* item = mExperiencesList->getItem(id);
	if (item)
	{
		mExperiencesList->deleteSingleItem(mExperiencesList->getItemIndex(item));
	}
}

void LLPanelExperiences::removeExperiences(const LLSD& ids)
{
	for (LLSD::array_const_iterator it = ids.beginArray(),
									end = ids.endArray();
		 it != end; ++it)
	{
		removeExperience(it->asUUID());
	}
}

void LLPanelExperiences::enableButton(bool enable)
{
	mActionBtn->setEnabled(enable);
}

void LLPanelExperiences::setButtonAction(const std::string& label,
										 void (*cb)(void*), void* data)
{
	if (label.empty())
	{
		mActionBtn->setVisible(false);
	}
	else
	{
		mActionBtn->setVisible(true);
		mActionBtn->setClickedCallback(cb, data);
		mActionBtn->setLabel(getString(label));
	}
}

#if 0	// not used
std::string LLPanelExperiences::getSelectedExperienceName() const
{
	std::string name;

	LLScrollListItem* item = mExperiencesList->getFirstSelected();
	if (item)
	{
		name = item->mExperiencesList->getSelectedItemLabel();
	}

	return name;
}

LLUUID LLPanelExperiences::getSelectedExperienceId() const
{
	LLUUID id;

	LLScrollListItem* item = mExperiencesList->getFirstSelected();
	if (item)
	{
		id = item->mExperiencesList->getUUID();
	}

	return id;
}
#endif

//static
void LLPanelExperiences::onDoubleClickProfile(void* data)
{
	LLPanelExperiences* self = (LLPanelExperiences*)data;
	if (self)
	{
		LLScrollListItem* item = self->mExperiencesList->getFirstSelected();
		if (item)
		{
			LLFloaterExperienceProfile::show(item->getUUID());
		}
	}
}

#define COROCAST(T)		static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, const LLSD&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)
#define COROCAST2(T)	static_cast<LLSD(LLCoreHttpUtil::HttpCoroutineAdapter::*)(const std::string&, LLCore::HttpOptions::ptr_t, LLCore::HttpHeaders::ptr_t)>(T)

void LLFloaterExperiences::retrieveExperienceList(const std::string& url,
												  const LLHandle<LLFloaterExperiences>& handle,
												  const name_map_t& tab_map,
												  const std::string& error_notify,
												  Callback_t cb)
{
	invokationFn_t getfn =
		boost::bind(COROCAST2(&LLCoreHttpUtil::HttpCoroutineAdapter::getAndSuspend),
					// _1 -> adapter
					// _2 -> url
					// _3 -> options
					// _4 -> headers
					_1, _2, _3, _4);
	gCoros.launch("LLFloaterExperiences::retrieveExperienceList",
				  boost::bind(&LLFloaterExperiences::retrieveExperienceListCoro,
							  url, handle, tab_map, error_notify, cb, getfn));
}

void LLFloaterExperiences::requestNewExperience(const std::string& url,
												const LLHandle<LLFloaterExperiences>& handle,
												const name_map_t& tab_map,
												const std::string& error_notify,
												Callback_t cb)
{
	invokationFn_t postfn =
		boost::bind(COROCAST(&LLCoreHttpUtil::HttpCoroutineAdapter::postAndSuspend),
					// _1 -> adapter
					// _2 -> url
					// _3 -> options
					// _4 -> headers
					_1, _2, LLSD(), _3, _4);
	gCoros.launch("LLFloaterExperiences::requestNewExperience",
				  boost::bind(&LLFloaterExperiences::retrieveExperienceListCoro,
							  url, handle, tab_map, error_notify, cb, postfn));
}

//static
void LLFloaterExperiences::retrieveExperienceListCoro(std::string url, 
													  LLHandle<LLFloaterExperiences> handle,
													  name_map_t tab_map, 
													  std::string error_notify,
													  Callback_t cb,
													  invokationFn_t invoker)
{
	if (url.empty())
	{
		llwarns << "Capability is empty !" << llendl;
		return;
	}

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
		adapter(new LLCoreHttpUtil::HttpCoroutineAdapter("retrieveExperienceListCoro"));
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
	LLSD result = invoker(adapter, url, options, headers);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		LLSD args;
		args["ERROR_MESSAGE"] = status.getType();
		gNotifications.add(error_notify, args);
		return;
	}

	LLFloaterExperiences* self = handle.get();
	if (!self || !self->mTabContainer)
	{
		return;
	}

	LLTabContainer* tabs = self->mTabContainer;
	LLPanelExperiences* tab;
	for (name_map_t::iterator it = tab_map.begin(), end = tab_map.end();
		 it != end; ++it)
	{
		if (result.has(it->first))
		{
			tab = dynamic_cast<LLPanelExperiences*>(tabs->getPanelByName(it->second));
			if (tab)
			{
				const LLSD& ids = result[it->first];
				tab->setExperienceList(ids);
				if (!cb.empty())
				{
					cb(tab, result);
				}
			}
		}
	}
}
