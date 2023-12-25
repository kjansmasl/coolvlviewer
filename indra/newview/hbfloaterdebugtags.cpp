/**
 * @file hbfloaterdebugtags.cpp
 * @brief The HBFloaterDebugTags class definition
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Henri Beauchamp
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

#include "hbfloaterdebugtags.h"

#include "lldir.h"
#include "llerrorcontrol.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "lluictrlfactory.h"

#include "llstartup.h"

std::set<std::string> HBFloaterDebugTags::sDefaultTagsList;
std::set<std::string> HBFloaterDebugTags::sAddedTagsList;

//static
void HBFloaterDebugTags::primeTagsFromLogControl()
{
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "logcontrol.xml");
	LLSD configuration;
	llifstream file(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::fromXML(configuration, file);
		file.close();
	}
	LLError::configure(configuration);

	// Remember the default tags list
	sDefaultTagsList = LLError::getTagsForLevel(LLError::LEVEL_DEBUG);

	for (std::set<std::string>::iterator it = sAddedTagsList.begin(),
											  end = sAddedTagsList.end();
		 it != end; ++it)
	{
			LLError::setTagLevel(*it, LLError::LEVEL_DEBUG);
	}
}

//static
void HBFloaterDebugTags::setTag(const std::string& tag, bool enable)
{
	if (sAddedTagsList.count(tag))
	{
		if (!enable)
		{
			llinfos << "Removing LL_DEBUGS tag \"" << tag
					<< "\" from logging controls" << llendl;
			sAddedTagsList.erase(tag);
			primeTagsFromLogControl();
		}
	}
	else if (enable)
	{
		llinfos << "Adding LL_DEBUGS tag \"" << tag
				<< "\" to logging controls" << llendl;
		sAddedTagsList.emplace(tag);

		LLError::setTagLevel(tag, LLError::LEVEL_DEBUG);
	}

	// Enable/disable debug message checks depending whether there are
	// debug tags or not.
	LLError::Log::sDebugMessages = !sAddedTagsList.empty() ||
									// Always allow debug messages when the
									// viewer is not yet connected
									!LLStartUp::isLoggedIn();
}

// Floater code proper

HBFloaterDebugTags::HBFloaterDebugTags(const LLSD&)
:	mIsDirty(false)
{
    LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_debug_tags.xml");
	primeTagsFromLogControl();
}

//virtual
bool HBFloaterDebugTags::postBuild()
{
	mDebugTagsList = getChild<LLScrollListCtrl>("tags_list");
	mDebugTagsList->setCommitCallback(onSelectLine);
	mDebugTagsList->setCallbackUserData(this);

	mIsDirty = true;

	return true;
}

//virtual
void HBFloaterDebugTags::draw()
{
	if (mIsDirty)
	{
		mIsDirty = false;
		refreshList();
	}

	LLFloater::draw();
}

void HBFloaterDebugTags::refreshList()
{
	if (!mDebugTagsList)
	{
		mIsDirty = true;
		return;
	}

	S32 scrollpos = mDebugTagsList->getScrollPos();
	mDebugTagsList->deleteAllItems();

	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														  "debug_tags.xml");
	llifstream file(filename.c_str());
	if (file.is_open())
	{
		LLSD list;
		llinfos << "Loading the debug tags list from: " << filename << llendl;
		LLSDSerialize::fromXML(list, file);
		S32 id = 0;
		std::string tag;
		LLScrollListItem* item;
		while (id < (S32)list.size())
		{
			bool has_tag = false;
			bool has_ref = false;
			bool has_other = false;
			LLSD data = list[id];
			if (data.has("columns"))
			{
				for (S32 i = 0; i < (S32)data["columns"].size(); ++i)
				{
					LLSD map = data["columns"][i];
					if (map.has("column"))
					{
						if (map["column"].asString() == "tag")
						{
							has_tag = true;
							tag = map.get("value").asString();
						}
						else if (map["column"].asString() == "references")
						{
							has_ref = true;
						}
						else
						{
							has_other = true;
						}
					}
					else
					{
						// Make sure the entry will be removed
						has_other = true;
						break;
					}
				}
			}
			if (!has_other && has_tag && has_ref)
			{
				data["columns"][2]["column"] = "active";
				data["columns"][2]["type"] = "checkbox";
				bool is_default = sDefaultTagsList.count(tag) != 0;
				bool active = is_default || sAddedTagsList.count(tag) != 0;
				data["columns"][2]["value"] = active;
				if (is_default)
				{
					data["columns"][0]["color"] = LLColor4::red2.getValue();
					data["columns"][1]["color"] = LLColor4::red2.getValue();
				}
				item = mDebugTagsList->addElement(data, ADD_BOTTOM);
				item->setEnabled(!is_default);	// cannot change default
				mDebugTagsList->deselectAllItems(true);
				++id;
			}
			else
			{
				list.erase(id);
			}
		}
		file.close();
	}

	mDebugTagsList->setScrollPos(scrollpos);
}

//static
void HBFloaterDebugTags::onSelectLine(LLUICtrl* ctrl, void* data)
{
	HBFloaterDebugTags* self = (HBFloaterDebugTags*)data;
	if (self && self->mDebugTagsList)
	{
		LLScrollListItem* item = self->mDebugTagsList->getFirstSelected();
		if (item && item->getColumn(0) && item->getColumn(1))
		{
			const std::string tag = item->getColumn(1)->getValue().asString();
			bool value = item->getColumn(0)->getValue().asBoolean();
			setTag(tag, value);
		}
	}
}
