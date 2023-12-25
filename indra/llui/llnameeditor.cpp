/**
 * @file llnameeditor.cpp
 * @brief Name Editor to refresh a name.
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

#include "llnameeditor.h"

#include "llcachename.h"

static const std::string LL_NAME_EDITOR_TAG = "name_editor";
static LLRegisterWidget<LLNameEditor> r12(LL_NAME_EDITOR_TAG);

// statics
LLNameEditor::nameeditor_list_t LLNameEditor::sInstances;

LLNameEditor::LLNameEditor(const std::string& name, const LLRect& rect,
						   const LLUUID& name_id, bool is_group,
						   const LLFontGL* glfont, S32 max_text_length,
						   void (*commit_cb)(LLUICtrl*, void*),
						   void (*keystroke_cb)(LLLineEditor*, void*),
						   void (*focus_lost_cb)(LLFocusableElement*, void*),
						   void* userdata, LLLinePrevalidateFunc pfunc)
:	LLLineEditor(name, rect, "(retrieving)", glfont, max_text_length,
				 commit_cb, keystroke_cb, focus_lost_cb, userdata, pfunc),
	mNameID(name_id)
{
	LLNameEditor::sInstances.insert(this);
	if (name_id.notNull())
	{
		setNameID(name_id, is_group);
	}
}

LLNameEditor::~LLNameEditor()
{
	LLNameEditor::sInstances.erase(this);
}

void LLNameEditor::setNameID(const LLUUID& name_id, bool is_group)
{
	mNameID = name_id;

	std::string name;

	if (gCacheNamep)
	{
		if (!is_group)
		{
			gCacheNamep->getFullName(name_id, name);
		}
		else
		{
			gCacheNamep->getGroupName(name_id, name);
		}
	}

	setText(name);
}

void LLNameEditor::refresh(const LLUUID& id, const std::string& fullname,
						   bool is_group)
{
	if (id == mNameID)
	{
		setText(fullname);
	}
}

void LLNameEditor::refreshAll(const LLUUID& id, const std::string& fullname,
							  bool is_group)
{
	for (nameeditor_list_t::iterator it = LLNameEditor::sInstances.begin(),
									 end = LLNameEditor::sInstances.end();
		 it != end; ++it)
	{
		LLNameEditor* editor = *it;
		editor->refresh(id, fullname, is_group);
	}
}

void LLNameEditor::setValue(const LLSD& value)
{
	setNameID(value.asUUID(), false);
}

LLSD LLNameEditor::getValue() const
{
	return LLSD(mNameID);
}

// virtual
LLXMLNodePtr LLNameEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLLineEditor::getXML();

	node->setName(LL_NAME_EDITOR_TAG);

	return node;
}

LLView* LLNameEditor::fromXML(LLXMLNodePtr node, LLView* parent,
							  LLUICtrlFactory* factory)
{
	std::string name = LL_NAME_EDITOR_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 128;
	node->getAttributeS32("max_length", max_text_length);

	LLFontGL* font = LLView::selectFont(node);

	LLUICtrlCallback commit_cb = NULL;
	LLNameEditor* line_editor = new LLNameEditor(name, rect, LLUUID::null,
												 false, font, max_text_length,
												 commit_cb);

	std::string label;
	if (node->getAttributeString("label", label))
	{
		line_editor->setLabel(label);
	}
	line_editor->setColorParameters(node);
	line_editor->initFromXML(node, parent);

	return line_editor;
}
