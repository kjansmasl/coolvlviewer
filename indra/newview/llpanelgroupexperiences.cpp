/**
 * @file llpanelgroupexperiences.cpp
 * @brief List of experiences owned by a group.
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

#include "llpanelgroupexperiences.h"

#include "llexperiencecache.h"
#include "llfloaterexperienceprofile.h"
#include "llscrolllistctrl.h"

#include "llagent.h"

//static
void* LLPanelGroupExperiences::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupExperiences("panel group experiences", *group_id);
}

LLPanelGroupExperiences::LLPanelGroupExperiences(const std::string& name,
												 const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mListEmpty(true)
{
}

bool LLPanelGroupExperiences::postBuild()
{
	mExperiencesList = getChild<LLScrollListCtrl>("experiences_list");
	mExperiencesList->addCommentText(getString("no_experiences_text"));
	mExperiencesList->setDoubleClickCallback(onDoubleClickProfile);
	mExperiencesList->setCallbackUserData(this);

	return LLPanelGroupTab::postBuild();
}

void LLPanelGroupExperiences::activate()
{
	if (mGroupID.notNull() &&
		gAgent.hasRegionCapability("GroupExperiences"))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->getGroupExperiences(mGroupID,
								 boost::bind(&LLPanelGroupExperiences::groupExperiencesResults,
											 getDerivedHandle<LLPanelGroupExperiences>(),
											 _1));
	}
}

//static
void LLPanelGroupExperiences::groupExperiencesResults(LLHandle<LLPanelGroupExperiences> handle,
													  const LLSD& experiences)
{
	LLPanelGroupExperiences* self = handle.get();
	if (self)
	{
		self->setExperienceList(experiences);
	}
}

bool LLPanelGroupExperiences::isVisibleByAgent()
{
	return mAllowEdit && gAgent.isInGroup(mGroupID) &&
		   gAgent.hasRegionCapability("GroupExperiences");
}

//static
void LLPanelGroupExperiences::cacheCallback(LLHandle<LLPanelGroupExperiences> handle,
											const LLSD& experience)
{
	LLPanelGroupExperiences* self = handle.get();
	if (self && experience.has(LLExperienceCache::EXPERIENCE_ID))
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

void LLPanelGroupExperiences::addExperience(const LLUUID& id)
{
	if (!mExperiencesList->getItem(id))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->get(id,
				 boost::bind(&LLPanelGroupExperiences::cacheCallback,
							 getDerivedHandle<LLPanelGroupExperiences>(), _1));
	}
}

void LLPanelGroupExperiences::setExperienceList(const LLSD& experiences)
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


//static
void LLPanelGroupExperiences::onDoubleClickProfile(void* data)
{
	LLPanelGroupExperiences* self = (LLPanelGroupExperiences*)data;
	if (self)
	{
		LLScrollListItem* item = self->mExperiencesList->getFirstSelected();
		if (item)
		{
			LLFloaterExperienceProfile::show(item->getUUID());
		}
	}
}
