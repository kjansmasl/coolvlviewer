/**
 * @file llpanelexperiencelisteditor.cpp
 * @brief Editor for building a list of experiences
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

#include "llpanelexperiencelisteditor.h"

#include "llbutton.h"
#include "llexperiencecache.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterexperiencepicker.h"
#include "llfloaterexperienceprofile.h"
#include "llviewerregion.h"

LLPanelExperienceListEditor::LLPanelExperienceListEditor()
:	mItems(NULL),
	mProfile(NULL),
	mRemove(NULL),
	mReadonly(false),
	mDisabled(false),
	mListEmpty(true),
	mMaxExperienceIDs(0)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_experience_list_editor.xml");
}

//virtual
LLPanelExperienceListEditor::~LLPanelExperienceListEditor()
{
	if (!mPicker.isDead())
	{
		mPicker.get()->close();
	}
}

//virtual
bool LLPanelExperienceListEditor::postBuild()
{
	mItemsCount = getChild<LLTextBox>("text_count");

	mItems = getChild<LLScrollListCtrl>("experience_list");
	mItems->setCommitCallback(checkButtonsEnabled);
	mItems->setDoubleClickCallback(onProfile);
	mItems->setCallbackUserData(this);

	mAdd = getChild<LLButton>("btn_add");
	mAdd->setClickedCallback(onAdd, this);

	mRemove = getChild<LLButton>("btn_remove");
	mRemove->setClickedCallback(onRemove, this);

	mProfile = getChild<LLButton>("btn_profile");
	mProfile->setClickedCallback(onProfile, this);

	checkButtonsEnabled(NULL, this);

	return LLPanel::postBuild();
}

void LLPanelExperienceListEditor::addExperience(const LLUUID& id)
{
	mExperienceIds.emplace(id);
	onItems(mItems, this);
}

void LLPanelExperienceListEditor::addExperienceIds(const uuid_vec_t& ids)
{
#if 0	// Now handled by the callback
	mExperienceIds.insert(ids.begin(), ids.end());
	onItems(mItems, this);
#endif
	if (!mAddedCallback.empty())
	{
		for (S32 i = 0, count = ids.size(); i < count; ++i)
		{
			mAddedCallback(ids[i]);
		}
	}
}

void LLPanelExperienceListEditor::setExperienceIds(const LLSD& experience_ids)
{
	mExperienceIds.clear();

	for (LLSD::array_const_iterator it = experience_ids.beginArray(),
									end = experience_ids.endArray();
		 it != end; ++it)
	{
		mExperienceIds.emplace(it->asUUID());
	}

	onItems(mItems, this);
}

//static
void LLPanelExperienceListEditor::checkButtonsEnabled(LLUICtrl*, void* data)
{
	LLPanelExperienceListEditor* self = (LLPanelExperienceListEditor*)data;
	if (!self)
	{
		return;
	}

	if (self->mDisabled)
	{
		self->mItems->setEnabled(false);
		self->mAdd->setEnabled(false);
		self->mRemove->setEnabled(false);
		self->mProfile->setEnabled(false);
		return;
	}

	S32 selected = self->mItems->getNumSelected();
	bool can_modify = !self->mReadonly;
	bool remove_enabled = can_modify && selected > 0;

	if (remove_enabled && self->mSticky)
	{
		std::vector<LLScrollListItem*> items = self->mItems->getAllSelected();
		for (std::vector<LLScrollListItem*>::iterator it = items.begin(),
													  end = items.end();
			 it != end; ++it)
		{
			LLScrollListItem* item = *it;
			if (item && self->mSticky(item->getValue()))
			{
				remove_enabled = false;
				break;
			}
		}
	}

	self->mAdd->setEnabled(can_modify);
	self->mRemove->setEnabled(remove_enabled);
	self->mProfile->setEnabled(selected == 1);
}

void LLPanelExperienceListEditor::onExperienceDetails(const LLSD& experience)
{
	if (mListEmpty)
	{
		// Remove the dummy, comment entry
		mItems->deleteAllItems();
		mListEmpty = false;
	}

	const LLUUID& id = experience[LLExperienceCache::EXPERIENCE_ID];
	std::string name = experience[LLExperienceCache::NAME];
	if (name.empty())
	{
		name = LLTrans::getString("ExperienceNameUntitled");
	}
	LLScrollListItem* item = mItems->getItem(id);
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

		LLSD& column_name = entry["columns"][0];
		column_name["column"] = "experience_name";
		column_name["value"] = name;
		mItems->addElement(entry);
	}

	checkButtonsEnabled(NULL, this);
}

void LLPanelExperienceListEditor::loading()
{
	mItems->deleteAllItems();
	mItems->addCommentText(getString("loading"));
	mListEmpty = true;
}

void LLPanelExperienceListEditor::setDisabled(bool val)
{
	mDisabled = val;
	setEnabled(!val);
	mItems->setEnabled(!val);
	checkButtonsEnabled(NULL, this);
}

void LLPanelExperienceListEditor::setReadonly(bool val)
{
	mReadonly = val;
	checkButtonsEnabled(NULL, this);
}

void LLPanelExperienceListEditor::refreshExperienceCounter()
{
	if (mMaxExperienceIDs > 0)
	{
		LLStringUtil::format_map_t args;
		args["[EXPERIENCES]"] = llformat("%d",
										 mListEmpty ? 0
													: mItems->getItemCount());
		args["[MAXEXPERIENCES]"] = llformat("%d", mMaxExperienceIDs);
		mItemsCount->setText(LLTrans::getString("ExperiencesCounter", args));
	}
}

boost::signals2::connection LLPanelExperienceListEditor::setAddedCallback(list_changed_signal_t::slot_type cb)
{
	return mAddedCallback.connect(cb);
}

boost::signals2::connection LLPanelExperienceListEditor::setRemovedCallback(list_changed_signal_t::slot_type cb)
{
	return mRemovedCallback.connect(cb);
}

//static
void LLPanelExperienceListEditor::onAdd(void* data)
{
	LLPanelExperienceListEditor* self = (LLPanelExperienceListEditor*)data;
	if (!self || self->mReadonly)
	{
		return;
	}

	if (!self->mPicker.isDead())
	{
		self->mPicker.markDead();
	}

	self->mKey.generateNewID();

	LLFloaterExperiencePicker* picker =
		LLFloaterExperiencePicker::show(boost::bind(&LLPanelExperienceListEditor::addExperienceIds,
													self, _1),
										self->mKey, false, true,
										self->mFilters);
	self->mPicker = picker->getDerivedHandle<LLFloaterExperiencePicker>();
	LLFloater* parent = self->getParentFloater();
	if (parent)
	{
		parent->addDependentFloater(picker);
	}
}

//static
void LLPanelExperienceListEditor::onRemove(void* data)
{
	LLPanelExperienceListEditor* self = (LLPanelExperienceListEditor*)data;
	if (!self || self->mReadonly)
	{
		return;
	}

	std::vector<LLScrollListItem*> items = self->mItems->getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = items.begin(),
												  end = items.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		if (item)
		{
#if 0		// Now handled by the callback
			self->mExperienceIds.erase(item->getValue());
#endif
			self->mRemovedCallback(item->getValue());
		}
	}

	self->mItems->selectFirstItem();
	checkButtonsEnabled(NULL, self);

#if 0	// Now handled by the callback
	onItems(self->mItems, self);
#endif
}

//static
void LLPanelExperienceListEditor::onProfile(void* data)
{
	LLPanelExperienceListEditor* self = (LLPanelExperienceListEditor*)data;
	if (self)
	{
		LLScrollListItem* item = self->mItems->getFirstSelected();
		if (item)
		{
			LLFloaterExperienceProfile::show(item->getUUID());
		}
	}
}

//static
void LLPanelExperienceListEditor::onItems(LLUICtrl*, void* data)
{
	LLPanelExperienceListEditor* self = (LLPanelExperienceListEditor*)data;
	if (!self) return;

	if (self->mExperienceIds.empty())
	{
		self->mItems->deleteAllItems();
		self->mItems->addCommentText(self->getString("no_results"));
		self->mListEmpty = true;
		return;
	}
		
	if (self->mListEmpty)
	{
		self->loading();
	}

	LLExperienceCache* exp = LLExperienceCache::getInstance();
	for (uuid_list_t::iterator it = self->mExperienceIds.begin(),
							   end = self->mExperienceIds.end();
		 it != end; ++it)
	{
		const LLUUID& experience = *it;
		exp->get(experience,
				 boost::bind(&LLPanelExperienceListEditor::experienceDetailsCallback,
							 self->getDerivedHandle<LLPanelExperienceListEditor>(),
							 _1));
	}
}

//static
void LLPanelExperienceListEditor::experienceDetailsCallback(LLHandle<LLPanelExperienceListEditor> panel,
															const LLSD& experience)
{
	if (!panel.isDead())
	{
		panel.get()->onExperienceDetails(experience);
	}
}
