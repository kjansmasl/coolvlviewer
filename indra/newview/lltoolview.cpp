/**
 * @file lltoolview.cpp
 * @brief A UI contains for tool palette tools
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "lltoolview.h"

#include "llbutton.h"
#include "llstl.h"				// For DeletePointer()
#include "lltextbox.h"

#include "lltool.h"
#include "lltoolmgr.h"

LLToolContainer::LLToolContainer(LLToolView* parent)
:	mParent(parent),
	mButton(NULL),
	mPanel(NULL),
	mTool(NULL)
{
}

LLToolContainer::~LLToolContainer()
{
	// mParent is a pointer to the tool view
	// mButton is owned by the tool view
	// mPanel is owned by the tool view
	delete mTool;
	mTool = NULL;
}

LLToolView::LLToolView(const std::string& name, const LLRect& rect)
:	LLView(name, rect, true),
	mButtonCount(0)
{
}

LLToolView::~LLToolView()
{
	std::for_each(mContainList.begin(), mContainList.end(), DeletePointer());
	mContainList.clear();
}

#if 0
void LLToolView::addTool(const std::string& icon_off,
						 const std::string& icon_on, LLPanel* panel,
						 LLTool* tool, LLView*, const char* label)
{
	llassert(tool);

	LLToolContainer* contain = new LLToolContainer(this);

	LLRect btn_rect = getButtonRect(mButtonCount);

	contain->mButton = new LLButton("ToolBtn", btn_rect, icon_off, icon_on,
									"", &LLToolView::onClickToolButton,
									contain, LLFontGL::getFontSansSerif ());

	contain->mPanel = panel;
	contain->mTool = tool;

	addChild(contain->mButton);
	++mButtonCount;

	constexpr S32 LABEL_TOP_SPACING = 0;
	static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	S32 label_width = font->getWidth(label);
	LLRect label_rect;
	label_rect.setLeftTopAndSize(btn_rect.mLeft + btn_rect.getWidth() / 2 -
								 label_width / 2,
								 btn_rect.mBottom - LABEL_TOP_SPACING,
								 label_width, llfloor(font->getLineHeight()));
	addChild(new LLTextBox("tool label", label_rect, label, font));

	// Can optionally ignore panel
	if (contain->mPanel)
	{
		contain->mPanel->setBackgroundVisible(false);
		contain->mPanel->setBorderVisible(false);
		addChild(contain->mPanel);
	}

	mContainList.push_back(contain);
}
#endif

LLRect LLToolView::getButtonRect(S32 button_index)
{
	constexpr S32 HPAD = 7;
	constexpr S32 VPAD = 7;
	constexpr S32 TOOL_SIZE = 32;
	constexpr S32 HORIZ_SPACING = TOOL_SIZE + 5;
	constexpr S32 VERT_SPACING = TOOL_SIZE + 14;

	S32 tools_per_row = getRect().getWidth() / HORIZ_SPACING;

	S32 row = button_index / tools_per_row;
	S32 column = button_index % tools_per_row;

	// Build the rectangle, recalling the origin is at lower left and we want
	// the icons to build down from the top.
	LLRect rect;
	rect.setLeftTopAndSize(HPAD + column * HORIZ_SPACING,
						   -VPAD + getRect().getHeight() - row * VERT_SPACING,
						   TOOL_SIZE, TOOL_SIZE);
	return rect;
}

void LLToolView::draw()
{
	// Turn off highlighting for all containers and hide all option panels
	// except for the selected one.
	LLTool* selected = gToolMgr.getCurrentToolset()->getSelectedTool();

	for (contain_list_t::iterator iter = mContainList.begin(),
								  end = mContainList.end();
		 iter != end; ++iter)
	{
		LLToolContainer* contain = *iter;
		bool state = contain->mTool == selected;
		contain->mButton->setToggleState(state);
		if (contain->mPanel)
		{
			contain->mPanel->setVisible(state);
		}
	}

	// Draw children normally
	LLView::draw();
}

LLToolContainer* LLToolView::findToolContainer(LLTool* tool)
{
	// Find the container for this tool
	llassert(tool);

	for (contain_list_t::iterator iter = mContainList.begin();
		 iter != mContainList.end(); ++iter)
	{
		LLToolContainer* contain = *iter;
		if (contain->mTool == tool)
		{
			return contain;
		}
	}

	llerrs << "Tool not found !" << llendl;
	return NULL;
}

//static
void LLToolView::onClickToolButton(void* userdata)
{
	LLToolContainer* clicked = (LLToolContainer*)userdata;
	if (!clicked) return;

	// Switch to this one
	gToolMgr.getCurrentToolset()->selectTool(clicked->mTool);
}
