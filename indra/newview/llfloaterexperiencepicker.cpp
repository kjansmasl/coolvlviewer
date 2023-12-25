/**
 * @file llfloaterexperiencepicker.cpp
 * @brief Implementation of LLFloaterExperiencePicker and
 *        LLPanelExperiencePicker
 * @author dolphin@lindenlab.com
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Linden Research, Inc.
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

#include "llfloaterexperiencepicker.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "llexperiencecache.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "lluri.h"

#include "llagent.h"
#include "llfloaterexperienceprofile.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

LLFloaterExperiencePicker::instances_map_t LLFloaterExperiencePicker::sInstancesMap;

// LLPanelExperiencePicker class

LLPanelExperiencePicker::LLPanelExperiencePicker()
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_experience_search.xml");
	setDefaultFilters();
}

bool LLPanelExperiencePicker::postBuild()
{
	mLineEditor = getChild<LLLineEditor>("edit");
	mLineEditor->setFocus(true);

	mSearchResultsList = getChild<LLScrollListCtrl>("search_results");
	mSearchResultsList->setCommitCallback(onList);
	mSearchResultsList->setDoubleClickCallback(onBtnSelect);
	mSearchResultsList->setCallbackUserData(this);
	mSearchResultsList->setEnabled(false);
	mSearchResultsList->addCommentText(getString("no_results"));

	mOkBtn = getChild<LLButton>("ok_btn");
	mOkBtn->setClickedCallback(onBtnSelect, this);
	mOkBtn->setEnabled(false);

	mCancelBtn = getChild<LLButton>("cancel_btn");
	mCancelBtn->setClickedCallback(onBtnClose, this);

	mProfileBtn = getChild<LLButton>("profile_btn");
	mProfileBtn->setClickedCallback(onBtnProfile, this);
	mProfileBtn->setEnabled(false);

	mMaturityCombo = getChild<LLComboBox>("maturity");
	mMaturityCombo->setCurrentByIndex(gSavedSettings.getS32("ExperiencesMaturity"));
	mMaturityCombo->setCommitCallback(onMaturity);
	mMaturityCombo->setCallbackUserData(this);

	mNextBtn = getChild<LLButton>("right_btn");
	mNextBtn->setClickedCallback(onNextPage, this);

	mPrevBtn = getChild<LLButton>("left_btn");
	mPrevBtn->setClickedCallback(onPrevPage, this);

	childSetAction("find", onBtnFind, this);
	// Start searching when Return is pressed in the line editor.
	setDefaultBtn("find");

	return true;
}

void LLPanelExperiencePicker::hideOkCancel()
{
	mOkBtn->setVisible(false);
	mCancelBtn->setVisible(false);
}

void LLPanelExperiencePicker::find()
{
	if (gAgent.hasRegionCapability("FindExperienceByName"))
	{
		std::string text = mLineEditor->getValue().asString();
		mQueryID.generate();
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->findExperienceByName(text, mCurrentPage,
								  boost::bind(&LLPanelExperiencePicker::findResults,
											  getDerivedHandle<LLPanelExperiencePicker>(),
											  mQueryID, _1));
	}

	mSearchResultsList->deleteAllItems();
	mSearchResultsList->addCommentText(getString("searching"));

	mOkBtn->setEnabled(false);
	mProfileBtn->setEnabled(false);
	mNextBtn->setEnabled(false);
	mPrevBtn->setEnabled(false);
}

//static
void LLPanelExperiencePicker::findResults(LLHandle<LLPanelExperiencePicker> handle,
										  LLUUID query_id, LLSD result)
{
	LLPanelExperiencePicker* self = handle.get();
	if (self)
	{
		self->processResponse(query_id, result);
	}
}

bool LLPanelExperiencePicker::isSelectButtonEnabled()
{
	return mSearchResultsList->getFirstSelectedIndex() >= 0;
}

void LLPanelExperiencePicker::setAllowMultiple(bool allow_multiple)
{
	mSearchResultsList->setAllowMultipleSelection(allow_multiple);
}

void LLPanelExperiencePicker::getSelectedExperienceIds(const LLScrollListCtrl* results,
													   uuid_vec_t &experience_ids)
{
	std::vector<LLScrollListItem*> items = results->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = items.begin(),
												  end = items.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		if (item && item->getUUID().notNull())
		{
			experience_ids.emplace_back(item->getUUID());
		}
	}
}

//static
void LLPanelExperiencePicker::nameCallback(const LLHandle<LLPanelExperiencePicker>& floater,
										   const LLUUID& experience_id,
										   const LLUUID& agent_id,
										   const LLAvatarName& av_name)
{
	if (floater.isDead())
	{
		return;
	}

	LLPanelExperiencePicker* picker = floater.get();
	LLScrollListCtrl* search_results = picker->mSearchResultsList;

	LLScrollListItem* item = search_results->getItem(experience_id);
	if (item)
	{
		item->getColumn(2)->setValue(" " + av_name.getLegacyName());
	}
}

void LLPanelExperiencePicker::processResponse(const LLUUID& query_id,
											  const LLSD& content)
{
	if (query_id != mQueryID)
	{
		return;
	}

	mResponse = content;

	mNextBtn->setEnabled(content.has("next_page_url"));
	mPrevBtn->setEnabled(content.has("previous_page_url"));

	filterContent();
}

void LLPanelExperiencePicker::filterContent()
{
	mSearchResultsList->deleteAllItems();

	LLSD item;
	std::string name;
	const LLSD& experiences = mResponse["experience_keys"];
	for (LLSD::array_const_iterator it = experiences.beginArray(),
									end = experiences.endArray();
		 it != end; ++it)
	{
		const LLSD& experience = *it;

		if (isExperienceHidden(experience))
		{
			continue;
		}

		item["id"] = experience[LLExperienceCache::EXPERIENCE_ID];
		name = experience[LLExperienceCache::NAME].asString();
		if (name.empty())
		{
			name = LLTrans::getString("ExperienceNameUntitled");
		}

		LLSD& columns = item["columns"];

		columns[0]["column"] = "maturity";
		U8 maturity = (U8)experience[LLExperienceCache::MATURITY].asInteger();
		columns[0]["value"] = LLViewerRegion::getMaturityIconName(maturity);
		columns[0]["type"] = "icon";
		columns[0]["halign"] = "right";

		columns[1]["column"] = "experience_name";
		columns[1]["value"] = " " + name;

		columns[2]["column"] = "owner";
		columns[2]["value"] = " " + getString("loading");

		mSearchResultsList->addElement(item);
		LLAvatarNameCache::get(experience[LLExperienceCache::AGENT_ID],
							   boost::bind(&LLPanelExperiencePicker::nameCallback,
										   getDerivedHandle<LLPanelExperiencePicker>(),
										   experience[LLExperienceCache::EXPERIENCE_ID],
										   _1, _2));
	}

	if (mSearchResultsList->isEmpty())
	{
		LLStringUtil::format_map_t map;
		std::string search_text = mLineEditor->getValue().asString();
		map["[TEXT]"] = search_text;
		if (search_text.empty())
		{
			mSearchResultsList->addCommentText(getString("no_results"));
		}
		else
		{
			mSearchResultsList->addCommentText(getString("not_found", map));
		}
		mSearchResultsList->setEnabled(false);
		mOkBtn->setEnabled(false);
		mProfileBtn->setEnabled(false);
	}
	else
	{
		mOkBtn->setEnabled(true);
		mSearchResultsList->setEnabled(true);
		mSearchResultsList->sortByColumnIndex(1, true);
		std::string text = mLineEditor->getValue().asString();
		if (!mSearchResultsList->selectItemByLabel(text, true, 1))
		{
			mSearchResultsList->selectFirstItem();
		}			
		onList(mSearchResultsList, this);
		mSearchResultsList->setFocus(true);
	}
}

bool LLPanelExperiencePicker::isExperienceHidden(const LLSD& experience) const
{
	for (filter_list::const_iterator it = mFilters.begin(),
									 end = mFilters.end();
		 it != end; ++it)
	{
		if ((*it)(experience))
		{
			return true;
		}
	}

	return false;
}

bool LLPanelExperiencePicker::FilterOverRating(const LLSD& experience)
{
	S32 maturity = mMaturityCombo->getSelectedValue().asInteger();
	return experience[LLExperienceCache::MATURITY].asInteger() > maturity;
}

void LLPanelExperiencePicker::setDefaultFilters()
{
	mFilters.clear();
	addFilter(boost::bind(&LLPanelExperiencePicker::FilterOverRating,
						  this, _1));
}

void LLPanelExperiencePicker::closeParent()
{
	LLView* viewp = getParent();
	if (viewp)
	{
		LLFloater* floaterp = viewp->asFloater();
		if (floaterp)
		{
			floaterp->close();
		}
	}
}

//static
void LLPanelExperiencePicker::onBtnClose(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		self->closeParent();
	}
}

//static
void LLPanelExperiencePicker::onBtnFind(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		self->mCurrentPage = 1;
		self->find();
	}
}

//static
void LLPanelExperiencePicker::onBtnProfile(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		LLScrollListItem* item = self->mSearchResultsList->getFirstSelected();
		if (item)
		{
			LLFloaterExperienceProfile::show(item->getUUID());
		}
	}
}

//static
void LLPanelExperiencePicker::onNextPage(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		self->mCurrentPage++;
		self->find();
	}
}

//static
void LLPanelExperiencePicker::onPrevPage(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		if (--self->mCurrentPage < 1)
		{
			self->mCurrentPage = 1;
		}
		self->find();
	}
}

//static
void LLPanelExperiencePicker::onBtnSelect(void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		if (!self->isSelectButtonEnabled())
		{
			return;
		}

		if (self->mSelectionCallback)
		{
			LLScrollListCtrl* results = self->mSearchResultsList;

			uuid_vec_t experience_ids;
			self->getSelectedExperienceIds(results, experience_ids);
			self->mSelectionCallback(experience_ids);

			results->deselectAllItems(true);

			if (self->mCloseOnSelect)
			{
				self->mCloseOnSelect = false;
				onBtnClose(data);
			}
		}
		else
		{
			onBtnProfile(data);
		}
	}
}

//static
void LLPanelExperiencePicker::onList(LLUICtrl* ctrl, void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		bool enabled = self->isSelectButtonEnabled();
		self->mOkBtn->setEnabled(enabled);

		enabled = enabled && self->mSearchResultsList->getNumSelected() == 1;
		self->mProfileBtn->setEnabled(enabled);
	}
}

//static
void LLPanelExperiencePicker::onMaturity(LLUICtrl* ctrl, void* data)
{
	LLPanelExperiencePicker* self = (LLPanelExperiencePicker*)data;
	if (self)
	{
		gSavedSettings.setS32("ExperiencesMaturity",
							  self->mMaturityCombo->getCurrentIndex());
		if (self->mResponse.has("experience_keys"))
		{
			const LLSD& experiences = self->mResponse["experience_keys"];
			if (experiences.beginArray() != experiences.endArray())
			{
				self->filterContent();
			}
		}
	}
}

// LLFloaterExperiencePicker class

//static
LLFloaterExperiencePicker* LLFloaterExperiencePicker::show(select_callback_t callback,
														   const LLUUID& key,
														   bool allow_multiple,
														   bool close_on_select,
														   filter_list filters)
{
	LLFloaterExperiencePicker* self;
	instances_map_t::iterator it = sInstancesMap.find(key);
	if (it == sInstancesMap.end())
	{
		self = new LLFloaterExperiencePicker(key);
	}
	else
	{
		self = it->second;
	}

	if (!self)
	{
		llwarns << "Cannot instantiate experience picker" << llendl;
		return NULL;
	}

	if (self->mSearchPanel)
	{
		self->mSearchPanel->mSelectionCallback = callback;
		self->mSearchPanel->mCloseOnSelect = close_on_select;
		self->mSearchPanel->setAllowMultiple(allow_multiple);
		self->mSearchPanel->setDefaultFilters();
		self->mSearchPanel->addFilters(filters.begin(), filters.end());
		self->mSearchPanel->filterContent();
	}

	return self;
}

//static
void* LLFloaterExperiencePicker::createSearchPanel(void* data)
{
	LLFloaterExperiencePicker* self = (LLFloaterExperiencePicker*)data;
	self->mSearchPanel = new LLPanelExperiencePicker();
	return self->mSearchPanel;
}

LLFloaterExperiencePicker::LLFloaterExperiencePicker(const LLUUID& key)
:	LLFloater(key.asString()),
	mKey(key),
	mSearchPanel(NULL)
{
	sInstancesMap[key] = this;

	LLCallbackMap::map_t factory_map;
	factory_map["experience_search"] = LLCallbackMap(createSearchPanel, this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_experience_search.xml",
												 &factory_map);
}

LLFloaterExperiencePicker::~LLFloaterExperiencePicker()
{
	gFocusMgr.releaseFocusIfNeeded(this);
	sInstancesMap.erase(mKey);
}
