/**
 * @file lluploaddialog.cpp
 * @brief LLUploadDialog class implementation
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

#include "linden_common.h"

#include "lluploaddialog.h"

#include "lltextbox.h"

//static
LLUploadDialog*	LLUploadDialog::sDialog = NULL;

//static
LLUploadDialog* LLUploadDialog::modalUploadDialog(const std::string& msg)
{
	// Note: object adds, removes, and destroys itself.
	return new LLUploadDialog(msg);
}

//static
void LLUploadDialog::modalUploadFinished()
{
	// Note: object adds, removes, and destroys itself.
	if (sDialog)
	{
		delete sDialog;
		sDialog = NULL;
	}
}

LLUploadDialog::LLUploadDialog(const std::string& msg)
:	LLPanel("upload_dialog", LLRect(0, 100, 100, 0))
{
	mFont = LLFontGL::getFontSansSerif();

	setBackgroundVisible(true);

	if (sDialog)
	{
		delete sDialog;
	}
	sDialog = this;

	LLRect msg_rect;
	for (S32 line = 0; line < 16; ++line)
	{
		mLabelBox[line] = new LLTextBox(" ", msg_rect, " ", mFont);
		addChild(mLabelBox[line]);
	}

	setMessage(msg);

	// The dialog view is a root view
	gFocusMgr.setTopCtrl(this);
}

//virtual
LLUploadDialog::~LLUploadDialog()
{
	gFocusMgr.releaseFocusIfNeeded(this);
	LLUploadDialog::sDialog = NULL;
}

void LLUploadDialog::setMessage(const std::string& msg)
{
	constexpr S32 VPAD = 16;
	constexpr S32 HPAD = 25;

	// Make the text boxes a little wider than the text
	constexpr S32 TEXT_PAD = 8;

	// Split message into lines, separated by '\n'
	S32 max_msg_width = 0;
	std::list<std::string> msg_lines;

	size_t size = msg.size() + 1;
	char* temp_msg = new (std::nothrow) char[size];
	if (!temp_msg)
	{
		llwarns << "Out of memory !" << llendl;
		return;
	}

	strcpy(temp_msg, msg.c_str());
	char* token = strtok(temp_msg, "\n");
	while (token)
	{
		std::string tokstr(token);
		S32 cur_width = S32(mFont->getWidth(tokstr) + 0.99f) + TEXT_PAD;
		max_msg_width = llmax(max_msg_width, cur_width);
		msg_lines.push_back(tokstr);
		token = strtok(NULL, "\n");
	}
	delete[] temp_msg;

	S32 line_height = S32(mFont->getLineHeight() + 0.99f);
	S32 dialog_width = max_msg_width + 2 * HPAD;
	S32 dialog_height = line_height * msg_lines.size() + 2 * VPAD;

	reshape(dialog_width, dialog_height, false);

	// Message
	S32 msg_x = (getRect().getWidth() - max_msg_width) / 2;
	S32 msg_y = getRect().getHeight() - VPAD - line_height;
	S32 line;
	for (line = 0; line < 16; ++line)
	{
		mLabelBox[line]->setVisible(false);
	}

	line = 0;
	for (std::list<std::string>::iterator iter = msg_lines.begin();
		 iter != msg_lines.end(); ++iter)
	{
		std::string& cur_line = *iter;
		LLRect msg_rect;
		msg_rect.setOriginAndSize(msg_x, msg_y, max_msg_width, line_height);
		mLabelBox[line]->setRect(msg_rect);
		mLabelBox[line]->setText(cur_line);
		mLabelBox[line]->setColor(LLUI::sLabelTextColor);
		mLabelBox[line++]->setVisible(true);
		msg_y -= line_height;
	}

	LLVector2 window_size = LLUI::getWindowSize();
	centerWithin(LLRect(0, 0, ll_round(window_size.mV[VX]),
						ll_round(window_size.mV[VY])));
}
