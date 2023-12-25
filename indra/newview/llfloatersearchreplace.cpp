/**
 * @file llfloatersearchreplace.cpp
 * @brief LLFloaterSearchReplace class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"

#include "llfloatersearchreplace.h"

LLFloaterSearchReplace::LLFloaterSearchReplace(const LLSD&)
:	mEditor(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_search_replace.xml");
}

//virtual
void LLFloaterSearchReplace::open()
{
	LLFloater::open();

	if (mEditor)
	{
		bool read_only = mEditor->isReadOnly();
		childSetEnabled("replace_label", !read_only);
		childSetEnabled("replace_text", !read_only);
		childSetEnabled("replace_btn", !read_only);
		childSetEnabled("replace_all_btn", !read_only);
	}

	childSetFocus("search_text", true);
}

//virtual
bool LLFloaterSearchReplace::postBuild()
{
	childSetAction("search_btn", onBtnSearch, this);
	childSetAction("replace_btn", onBtnReplace, this);
	childSetAction("replace_all_btn", onBtnReplaceAll, this);

	setDefaultBtn("search_btn");

	return true;
}

//static
void LLFloaterSearchReplace::show(LLTextEditor* editor)
{
	// creates a new floater if needed
	LLFloaterSearchReplace* self = getInstance();

	if (self && editor)
	{
		self->mEditor = editor;

		LLFloater* newdependee;
		LLFloater* olddependee = self->getDependee();
		LLView* viewp = editor->getParent();
		while (viewp)
		{
			newdependee = viewp->asFloater();
			if (newdependee)
			{
				if (newdependee != olddependee)
				{
					if (olddependee)
					{
						olddependee->removeDependentFloater(self);
					}

					if (!newdependee->getHost())
					{
						newdependee->addDependentFloater(self);
					}
					else
					{
						newdependee->getHost()->addDependentFloater(self);
					}
				}
				break;
			}
			viewp = viewp->getParent();
		}

		// brings old instance to foreground if needed and refreshes labels
		self->open();
	}
}

//static
void LLFloaterSearchReplace::onBtnSearch(void* userdata)
{
	LLFloaterSearchReplace* self = (LLFloaterSearchReplace*)userdata;
	if (self && self->mEditor && self->getDependee())
	{
		LLCheckBoxCtrl* check = self->getChild<LLCheckBoxCtrl>("case_text");
		self->mEditor->selectNext(self->childGetText("search_text"),
								  check->get());
	}
}

//static
void LLFloaterSearchReplace::onBtnReplace(void* userdata)
{
	LLFloaterSearchReplace* self = (LLFloaterSearchReplace*)userdata;
	if (self && self->mEditor && self->getDependee())
	{
		LLCheckBoxCtrl* check = self->getChild<LLCheckBoxCtrl>("case_text");
		self->mEditor->replaceText(self->childGetText("search_text"),
								   self->childGetText("replace_text"),
								   check->get());
	}
}

//static
void LLFloaterSearchReplace::onBtnReplaceAll(void* userdata)
{
	LLFloaterSearchReplace* self = (LLFloaterSearchReplace*)userdata;
	if (self && self->mEditor && self->getDependee())
	{
		LLCheckBoxCtrl* check = self->getChild<LLCheckBoxCtrl>("case_text");
		self->mEditor->replaceTextAll(self->childGetText("search_text"),
									  self->childGetText("replace_text"),
									  check->get());
	}
}
