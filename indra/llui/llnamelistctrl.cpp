/**
 * @file llnamelistctrl.cpp
 * @brief A list of names, automatically refreshed from name cache.
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

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llnamelistctrl.h"

#include "llframetimer.h"
#include "llinventory.h"

static const std::string LL_NAME_LIST_CTRL_TAG = "name_list";
static LLRegisterWidget<LLNameListCtrl> r13(LL_NAME_LIST_CTRL_TAG);

//static
LLNameListCtrl::instances_list_t LLNameListCtrl::sInstances;

LLNameListCtrl::LLNameListCtrl(const std::string& name, const LLRect& rect,
							   LLUICtrlCallback cb, void* userdata,
							   bool allow_multiple_selection, bool draw_border,
							   S32 name_column_index,
							   const std::string& tooltip)
:	LLScrollListCtrl(name, rect, cb, userdata, allow_multiple_selection,
					 draw_border),
	mNameColumnIndex(name_column_index),
	mAllowCallingCardDrop(false),
	mUseDisplayNames(false),
	mLastUpdate(0.0),
	mLazyUpdateInterval(0.f)
{
	setToolTip(tooltip);
	sInstances.insert(this);
}

//virtual
LLNameListCtrl::~LLNameListCtrl()
{
	sInstances.erase(this);
}

bool LLNameListCtrl::addNameItem(const LLUUID& agent_id, EAddPosition pos,
								 bool enabled, const std::string& suffix)
{
	std::string fullname;
	bool result = getResidentName(agent_id, fullname);

	fullname.append(suffix);

	addStringUUIDItem(fullname, agent_id, pos, enabled);

	return result;
}

//virtual
bool LLNameListCtrl::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									   EDragAndDropType cargo_type,
									   void* cargo_data, EAcceptance* accept,
									   std::string& tooltip_msg)
{
	if (!mAllowCallingCardDrop)
	{
		return false;
	}

	if (cargo_type == DAD_CALLINGCARD)
	{
		if (drop)
		{
			LLInventoryItem* item = (LLInventoryItem*)cargo_data;
			addNameItem(item->getCreatorUUID());
		}

		*accept = ACCEPT_YES_MULTI;
	}
	else
	{
		*accept = ACCEPT_NO;
		if (tooltip_msg.empty())
		{
			if (!getToolTip().empty())
			{
				tooltip_msg = getToolTip();
			}
			else
			{
				// Backwards compatible English tooltip (should be overridden
				// in xml)
				tooltip_msg.assign("Drag a calling card here\nto add a resident.");
			}
		}
	}

	LL_DEBUGS("UserInput") << "dragAndDrop handled by LLNameListCtrl "
						   << getName() << LL_ENDL;

	return true;
}

void LLNameListCtrl::addGroupNameItem(const LLUUID& group_id, EAddPosition pos,
									  bool enabled)
{
	std::string group_name;
	getGroupName(group_id, group_name);
	addStringUUIDItem(group_name, group_id, pos, enabled);
}

void LLNameListCtrl::addGroupNameItem(LLScrollListItem* item, EAddPosition pos)

{
	std::string group_name;
	getGroupName(item->getUUID(), group_name);

	LLScrollListText* cell =
		(LLScrollListText*)item->getColumn(mNameColumnIndex);
	cell->setText(group_name);

	addItem(item, pos);
}

bool LLNameListCtrl::addNameItem(LLScrollListItem* item, EAddPosition pos)
{
	std::string fullname;
	bool result = getResidentName(item->getUUID(), fullname);

	LLScrollListText* cell =
		(LLScrollListText*)item->getColumn(mNameColumnIndex);
	cell->setText(fullname);

	addItem(item, pos);

	// This column is resizable
	LLScrollListColumn* columnp = getColumn(mNameColumnIndex);
	if (columnp && columnp->mHeader)
	{
		columnp->mHeader->setHasResizableElement(true);
	}

	return result;
}

LLScrollListItem* LLNameListCtrl::addElement(const LLSD& value, EAddPosition pos,
											 void* userdata)
{
	LLScrollListItem* item = LLScrollListCtrl::addElement(value, pos,
														  userdata);

	// Use supplied name by default
	std::string fullname = value["name"].asString();
	bool has_target = value.has("target");
	if (has_target && value["target"].asString() == "GROUP")
	{
		std::string name;
		if (getGroupName(item->getUUID(), name))
		{
			fullname = name;
		}
	}
	// When "SPECIAL", we just use supplied name, else it must be a resident
	else if (!has_target || value["target"].asString() != "SPECIAL")
	{
		// Normal resident
		std::string name;
		if (getResidentName(item->getUUID(), name))
		{
			fullname = name;
		}
	}

	LLScrollListText* cell =
		(LLScrollListText*)item->getColumn(mNameColumnIndex);
	cell->setText(fullname);

	dirtyColumns();

	// This column is resizable
	LLScrollListColumn* columnp = getColumn(mNameColumnIndex);
	if (columnp && columnp->mHeader)
	{
		columnp->mHeader->setHasResizableElement(true);
	}

	return item;
}

void LLNameListCtrl::removeNameItem(const LLUUID& agent_id)
{
	if (selectByID(agent_id))
	{
		S32 index = getItemIndex(getFirstSelected());
		if (index >= 0)
		{
			deleteSingleItem(index);
		}
	}
}

void LLNameListCtrl::draw()
{
	if (LLFrameTimer::getElapsedSeconds() - mLastUpdate >= (F64)mLazyUpdateInterval &&
		mPendingUpdates.size())
	{
		pending_map_t::iterator av_it;
		pending_map_t::iterator av_end = mPendingUpdates.end();
		for (item_list::iterator iter = getItemList().begin(),
								 end = getItemList().end();
			 iter != end; ++iter)
		{
			LLScrollListItem* item = *iter;
			av_it = mPendingUpdates.find(item->getUUID());
			if (av_it != av_end)
			{
				LLScrollListText* cell =
					(LLScrollListText*)item->getColumn(mNameColumnIndex);
				cell->setText(av_it->second);

				mPendingUpdates.erase(av_it);
				if (mPendingUpdates.size() == 0)
				{
					break;
				}
				av_end = mPendingUpdates.end();
			}
		}
		dirtyColumns();
		// Updates received via the legacy cache requests callback may not be
		// for us, so there may be ids left in mPendingUpdates that do not
		// belong to our list, also, some list lines may have been removed
		// while waiting for the name request reply: just trash them all.
		mPendingUpdates.clear();
		mLastUpdate = LLFrameTimer::getElapsedSeconds();
	}

	LLScrollListCtrl::draw();
}

void LLNameListCtrl::refresh(const LLUUID& id, const std::string& fullname,
							 bool is_group)
{
	if (mLazyUpdateInterval > 0.f)
	{
		// Perform a lazy update for names that can come in large amount (100+)
		// at a short interval of time.
		mPendingUpdates[id] = fullname;
		return;
	}

	for (item_list::iterator iter = getItemList().begin(),
							 end = getItemList().end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item->getUUID() == id)
		{
			LLScrollListText* cell =
				(LLScrollListText*)item->getColumn(mNameColumnIndex);
			cell->setText(fullname);
		}
	}

	dirtyColumns();
}

//static
void LLNameListCtrl::refreshAll(const LLUUID& id, const std::string& fullname,
								bool is_group)
{
	for (instances_list_t::iterator it = sInstances.begin(),
									end = sInstances.end();
		 it != end; ++it)
	{
		LLNameListCtrl* ctrl = *it;
		ctrl->refresh(id, fullname, is_group);
	}
}

//virtual
LLXMLNodePtr LLNameListCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLScrollListCtrl::getXML();

	node->setName(LL_NAME_LIST_CTRL_TAG);

	node->createChild("allow_calling_card_drop",
					  true)->setBoolValue(mAllowCallingCardDrop);

	node->createChild("use_display_names",
					  true)->setBoolValue(mUseDisplayNames);

	if (mNameColumnIndex)
	{
		node->createChild("name_column_index",
						  true)->setIntValue(mNameColumnIndex);
	}

	// Do not save contents, probably filled by code

	return node;
}

LLView* LLNameListCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
								LLUICtrlFactory* factory)
{
	std::string name = LL_NAME_LIST_CTRL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	bool multi_select = false;
	node->getAttributeBool("multi_select", multi_select);

	bool draw_border = true;
	node->getAttributeBool("draw_border", draw_border);

	bool draw_heading = false;
	node->getAttributeBool("draw_heading", draw_heading);

	S32 name_column_index = 0;
	node->getAttributeS32("name_column_index", name_column_index);

	LLUICtrlCallback callback = NULL;

	LLNameListCtrl* name_list = new LLNameListCtrl(name, rect, callback, NULL,
												   multi_select, draw_border,
												   name_column_index);

	name_list->setDisplayHeading(draw_heading);
	if (node->hasAttribute("heading_height"))
	{
		S32 heading_height;
		node->getAttributeS32("heading_height", heading_height);
		name_list->setHeadingHeight(heading_height);
	}

	bool allow_calling_card_drop = false;
	if (node->getAttributeBool("allow_calling_card_drop",
							   allow_calling_card_drop))
	{
		name_list->setAllowCallingCardDrop(allow_calling_card_drop);
	}

	if (node->hasAttribute("use_display_names"))
	{
		bool use_display_names = false;
		if (node->getAttributeBool("use_display_names", use_display_names))
		{
			name_list->setUseDisplayNames(use_display_names);
		}
	}

	name_list->setScrollListParameters(node);

	name_list->initFromXML(node, parent);

	LLSD columns;
	S32 index = 0;
	std::string labelname, columnname;
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName("column"))
		{
			labelname.clear();
			child->getAttributeString("label", labelname);

			columnname.clear();
			child->getAttributeString("name", columnname);
			if (columnname.empty())
			{
				columnname = labelname;
			}
			else if (labelname.empty())
			{
				labelname = columnname;
			}

			bool columndynamicwidth = false;
			child->getAttributeBool("dynamicwidth", columndynamicwidth);

			std::string sortname(columnname);
			child->getAttributeString("sort", sortname);

			S32 columnwidth = -1;
			if (child->hasAttribute("relwidth"))
			{
				F32 columnrelwidth = 0.f;
				child->getAttributeF32("relwidth", columnrelwidth);
				columns[index]["relwidth"] = columnrelwidth;
			}
			else
			{
				child->getAttributeS32("width", columnwidth);
				columns[index]["width"] = columnwidth;
			}

			LLFontGL::HAlign h_align = LLFontGL::LEFT;
			h_align = LLView::selectFontHAlign(child);

			columns[index]["name"] = columnname;
			columns[index]["label"] = labelname;
			columns[index]["halign"] = (S32)h_align;
			columns[index]["dynamicwidth"] = columndynamicwidth;
			columns[index++]["sort"] = sortname;
		}
	}
	name_list->setColumnHeadings(columns);

	LLUUID id;
	std::string value, font, font_style;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName("row"))
		{
			child->getAttributeUUID("id", id);

			LLSD row;
			row["id"] = id;

			S32 column_idx = 0;
			for (LLXMLNodePtr row_child = node->getFirstChild();
				 row_child.notNull(); row_child = row_child->getNextSibling())
			{
				if (row_child->hasName("column"))
				{
					value = row_child->getTextContents();

					columnname.clear();
					row_child->getAttributeString("name", columnname);

					font.clear();
					row_child->getAttributeString("font", font);

					font_style.clear();
					row_child->getAttributeString("font-style", font_style);

					LLSD& columns = row["columns"];
					columns[column_idx]["column"] = columnname;
					columns[column_idx]["value"] = value;
					columns[column_idx]["font"] = font;
					columns[column_idx++]["font-style"] = font_style;
				}
			}
			name_list->addElement(row);
		}
	}

	std::string contents = node->getTextContents();

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\t\n");
	tokenizer tokens(contents, sep);
	tokenizer::iterator token_iter = tokens.begin();
	tokenizer::iterator token_end = tokens.end();

	while (token_iter != token_end)
	{
		const std::string& line = *token_iter++;
		name_list->addCommentText(line);
	}

	return name_list;
}

bool LLNameListCtrl::getResidentName(const LLUUID& agent_id,
									 std::string& fullname)
{
	LLAvatarName av_name;
	if (LLAvatarNameCache::get(agent_id, &av_name))
	{
		if (!LLAvatarName::sLegacyNamesForFriends && mUseDisplayNames &&
			LLAvatarNameCache::useDisplayNames())
		{
			if (LLAvatarNameCache::useDisplayNames() == 2)
			{
				fullname = av_name.mDisplayName;
			}
			else
			{
				fullname = av_name.getNames();
			}
		}
		else
		{
			fullname = av_name.getLegacyName();
		}

		return true;
	}

	// Schedule a callback
	LLAvatarNameCache::get(agent_id,
						   boost::bind(&LLNameListCtrl::onAvatarNameCache,
						   			   _1, _2, this));
	return false;
}

//static
void LLNameListCtrl::onAvatarNameCache(const LLUUID& agent_id,
									   const LLAvatarName& av_name,
									   LLNameListCtrl* self)
{
	if (!sInstances.count(self)) return;	// Stale callback, instance closed.

	std::string fullname;
	if (!LLAvatarName::sLegacyNamesForFriends && self->mUseDisplayNames &&
		LLAvatarNameCache::useDisplayNames())
	{
		if (LLAvatarNameCache::useDisplayNames() == 2)
		{
			fullname = av_name.mDisplayName;
		}
		else
		{
			fullname = av_name.getNames();
		}
	}
	else
	{
		fullname = av_name.getLegacyName();
	}

	self->refresh(agent_id, fullname, false);
}

bool LLNameListCtrl::getGroupName(const LLUUID& group_id, std::string& name)
{
	if (gCacheNamep->getGroupName(group_id, name))
	{
		return true;
	}

	// Schedule a callback
	gCacheNamep->get(group_id, true,
					 boost::bind(&LLNameListCtrl::onGroupNameCache, _1, _2,
								 this));
	return false;
}

//static
void LLNameListCtrl::onGroupNameCache(const LLUUID& group_id,
									  const std::string& name,
									  LLNameListCtrl* self)
{
	if (sInstances.count(self))
	{
		self->refresh(group_id, name, true);
	}
}

LLScrollListItem* LLNameListCtrl:: getItemById(const LLUUID& id)
{
	for (item_list::iterator iter = getItemList().begin(),
							 end = getItemList().end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item->getUUID() == id)
		{
			return item;
		}
	}
	return NULL;
}

void LLNameListCtrl::sortByName(bool ascending)
{
	sortByColumnIndex(mNameColumnIndex, ascending);
}
